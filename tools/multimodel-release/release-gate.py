#!/usr/bin/env python3
"""Verify release packages and run bounded per-model checks."""
import argparse
import ctypes
import hashlib
import json
import mmap
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import socket
import subprocess
import tarfile
import time
from urllib.request import Request, urlopen
import zipfile


CHECKS = ("archive_sha256", "contents_sha256", "build", "link", "dependencies",
          "wine_model_check", "launch", "model_load", "generation", "correctness", "memory")
SYSTEM_DLLS = {
    "advapi32.dll", "comdlg32.dll", "kernel32.dll", "msvcrt.dll", "shell32.dll",
    "user32.dll", "winhttp.dll", "ws2_32.dll", "ucrtbase.dll", "bcrypt.dll",
    "crypt32.dll", "psapi.dll", "ole32.dll", "ntdll.dll",
}


def run(argv, *, timeout=15, **kw):
    return subprocess.run(argv, check=True, capture_output=True, text=True, timeout=timeout, **kw).stdout


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def checked_path(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name or ":" in name or not path.parts:
        raise ValueError(f"unsafe archive path: {name}")
    return path


def sums(text):
    result = {}
    for line in text.splitlines():
        digest, name = line.split(maxsplit=1)
        path = checked_path(name.removeprefix("*"))
        if path in result or not re.fullmatch(r"[a-fA-F0-9]{64}", digest):
            raise ValueError(f"invalid or duplicate checksum: {name}")
        result[path] = digest.lower()
    return result


def extract(archive, output):
    output.mkdir()
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as source:
            names = set()
            for item in source.infolist():
                path = checked_path(item.filename)
                if path in names or (item.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError(f"duplicate or symlink: {path}")
                names.add(path)
            source.extractall(output)
    else:
        with tarfile.open(archive, "r:gz") as source:
            names = set()
            for item in source.getmembers():
                path = checked_path(item.name)
                if path in names or not (item.isfile() or item.isdir()):
                    raise ValueError(f"unsupported or duplicate tar member: {path}")
                names.add(path)
            source.extractall(output, filter="data")
    roots = list(output.iterdir())
    if len(roots) != 1 or not roots[0].is_dir():
        raise ValueError("archive must contain one top-level directory")
    return roots[0]


def check_contents(root):
    manifest = root / "CONTENTS.sha256"
    files = sums(manifest.read_text())
    present = {PurePosixPath(p.relative_to(root).as_posix()) for p in root.rglob("*")
               if p.is_file() and p != manifest}
    if set(files) != present:
        raise ValueError("contents manifest does not describe every packaged file")
    for path, digest in files.items():
        if sha(root / path) != digest:
            raise ValueError(f"contents SHA-256 mismatch: {path}")
    return files


def static_checks(row, root, matrix):
    entry = root / matrix["entry"]
    missing = [name for name in [matrix["entry"], *matrix["files"]] if not (root / name).is_file()]
    if missing:
        row["link"] = ("FAIL", "Missing package files: " + ", ".join(missing))
        return False
    try:
        if matrix["os"] == "linux-x64":
            header = run(["readelf", "-h", str(entry)])
            if "ELF64" not in header or "X86-64" not in header or not os.access(entry, os.X_OK):
                raise ValueError("expected an executable ELF x86-64 binary")
        else:
            tool = shutil.which("x86_64-w64-mingw32-objdump") or shutil.which("objdump")
            if not tool:
                raise FileNotFoundError("objdump")
            header = run([tool, "-f", str(entry)])
            if "pei-x86-64" not in header:
                raise ValueError("expected a PE32+ x86-64 executable")
        row["link"] = ("PASS", matrix["entry"])
        if matrix["os"] == "linux-x64":
            libs = run(["ldd", str(entry)])
            if "not found" in libs:
                raise ValueError(libs.strip())
        else:
            libs = run([tool, "-p", str(entry)])
            imports = re.findall(r"DLL Name:\s*(\S+)", libs)
            if not imports:
                raise ValueError("missing PE import table")
            missing = [lib for lib in imports if lib.lower() not in SYSTEM_DLLS and not (entry.parent / lib).is_file()]
            if missing:
                raise ValueError("unresolved package imports: " + ", ".join(missing))
        row["dependencies"] = ("PASS", "Static imports checked. Driver loading is covered by the native run.")
        return True
    except FileNotFoundError as e:
        row["dependencies"] = ("NOT TESTED", str(e))
    except (subprocess.SubprocessError, ValueError) as e:
        step = "dependencies" if row["link"][0] == "PASS" else "link"
        row[step] = ("FAIL", str(e))
    return False


def fetch(url, route, payload=None, timeout=2):
    if payload is None:
        request = url + route
    else:
        request = Request(url + route, json.dumps(payload).encode(),
                          {"Content-Type": "application/json"})
    with urlopen(request, timeout=timeout) as response:
        return json.load(response)


def process_rss(pid):
    if os.name == "nt":
        command = f"(Get-Process -Id {int(pid)}).PeakWorkingSet64"
        ram = int(run(["powershell", "-NoProfile", "-Command", command]).strip()) / 1048576
    else:
        fields = dict(line.split(":", 1) for line in Path(f"/proc/{pid}/status").read_text().splitlines())
        ram = int(fields["VmHWM"].split()[0]) / 1024
    return ram


def process_memory(pid):
    ram = process_rss(pid)
    gpu = run(["nvidia-smi", "--query-compute-apps=pid,used_gpu_memory", "--format=csv,noheader,nounits"])
    samples = [v.strip() for p, v in (line.split(",", 1) for line in gpu.splitlines()) if p.strip() == str(pid)]
    vram = sum(map(int, samples)) if samples and all(v.isdecimal() for v in samples) else None
    return ram, vram


def resident_weights(weight):
    resident, total = map(int, run(["fincore", "-b", "-n", "-o", "RES,SIZE", str(weight)]).split())
    if resident < total:
        page = os.sysconf("SC_PAGE_SIZE")
        missing = bytearray((total + page - 1) // page)
        libc = ctypes.CDLL(None, use_errno=True)
        with weight.open("rb") as stream:
            with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_COPY) as mapping:
                address = ctypes.c_char.from_buffer(mapping)
                vector = (ctypes.c_ubyte * len(missing)).from_buffer(missing)
                status = libc.mincore(ctypes.c_void_p(ctypes.addressof(address)),
                                      ctypes.c_size_t(total), vector)
                del address
                if status:
                    raise OSError(ctypes.get_errno(), "mincore")
            until = time.monotonic() + 40
            for hole in re.finditer(rb"\x00+", missing):
                start, stop = hole.start() * page, min(hole.end() * page, total)
                for offset in range(start, stop, 8 * 1024 * 1024):
                    if time.monotonic() > until:
                        raise TimeoutError("file-cache warmup exceeded 40 seconds")
                    os.pread(stream.fileno(), min(stop - offset, 8 * 1024 * 1024), offset)
        resident, total = map(int, run(["fincore", "-b", "-n", "-o", "RES,SIZE", str(weight)]).split())
    if resident < total:
        raise ValueError(f"{resident}/{total} model bytes are resident")


def wine_metadata(row, spec, model, root, weights, prefix):
    if not spec.get("wine_model_check"):
        row["wine_model_check"] = ("NOT APPLICABLE", "No metadata-only entry point")
        return
    if not weights or not prefix or not shutil.which("wine") or os.name == "nt":
        return
    weight = weights / model["weights"]
    if not weight.is_file():
        return
    winpath = "Z:" + str(weight.resolve()).replace("/", "\\")
    env = dict(os.environ, GOOSE_CUDA="0", WINEDEBUG="-all", WINEPREFIX=str(prefix))
    try:
        answer = run(["wine", str(root / spec["wine_model_check"]), "--model-check",
                      winpath, "Hello Goose"], timeout=25, cwd=root, env=env).strip()
        parts = answer.split()
        if len(parts) < 3 or parts[0] != spec["model"] or not all(p.isdecimal() for p in parts[1:]):
            raise ValueError(f"unexpected metadata probe: {answer!r}")
        row["wine_model_check"] = ("PASS", answer)
    except (OSError, ValueError, subprocess.SubprocessError) as e:
        row["wine_model_check"] = ("FAIL", (getattr(e, "stderr", None) or str(e)).strip())


def has_amd_gpu():
    if os.name == "nt":
        try:
            ids = run(["powershell", "-NoProfile", "-Command",
                       "(Get-CimInstance Win32_VideoController).PNPDeviceID"])
            return "VEN_1002" in ids.upper()
        except (OSError, subprocess.SubprocessError):
            return False
    for path in Path("/sys/bus/pci/devices").glob("*"):
        try:
            if (path / "vendor").read_text().strip() == "0x1002" and (path / "class").read_text().startswith("0x03"):
                return True
        except OSError:
            continue
    return False


def probe(row, spec, model, root, logs, weights, prefix, gpu_index, vulkan_device=None, vulkan_wine_icd=None):
    kind = spec["runner"]
    vulkan = "-vulkan" in kind
    if kind in ("linux-vulkan-amd", "windows-vulkan-amd"):
        if (os.name == "nt") != (kind == "windows-vulkan-amd") or not has_amd_gpu():
            row["launch"] = ("NOT TESTED", "Native AMD display device not available on this host")
            return
    if kind == "linux-vulkan-explicit" and (os.name == "nt" or vulkan_device is None):
        return
    if kind == "wine-vulkan" and (os.name == "nt" or prefix is None or vulkan_wine_icd is None or
                                  shutil.which("wine") is None):
        return
    if kind == "windows-cuda" and os.name != "nt":
        return
    if kind == "wine-cpu" and (prefix is None or shutil.which("wine") is None):
        return
    if kind == "linux-cuda" and (os.name == "nt" or shutil.which("nvidia-smi") is None):
        return
    weight = weights / model["weights"] if weights else None
    if not weight or not weight.is_file():
        return
    if model.get("weights_sha256") and sha(weight) != model["weights_sha256"]:
        row["model_load"] = ("FAIL", "Model SHA-256 does not match the reference")
        return
    if kind.endswith("-cuda"):
        try:
            gpu = run(["nvidia-smi", "-i", str(gpu_index), "--query-gpu=name", "--format=csv,noheader"]).strip()
        except (OSError, subprocess.SubprocessError):
            return
        if not gpu:
            return

    if (kind == "linux-cuda" or (vulkan and os.name != "nt")) and model.get("requires_residency"):
        try:
            resident_weights(weight)
        except (OSError, ValueError, subprocess.SubprocessError) as e:
            row["model_load"] = ("NOT TESTED", str(e))
            return

    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    command = [str(root / spec["entry"])]
    env = {k: v for k, v in os.environ.items() if not k.startswith(("GOOSE_", "BONSAI_", "I2_", "GGML_"))}
    env.update(FRANKENMOE_CONSOLE="0", CUDA_VISIBLE_DEVICES=str(gpu_index))
    if vulkan:
        env["CUDA_VISIBLE_DEVICES"] = ""
        if kind == "wine-vulkan":
            command.insert(0, "wine")
            env.update(WINEDEBUG="-all", WINEPREFIX=str(prefix), VK_DRIVER_FILES=str(vulkan_wine_icd))
            env.pop("VK_ICD_FILENAMES", None)
            weight = "Z:" + str(weight.resolve()).replace("/", "\\")
        command += [str(weight), *model.get("vulkan_args", []), "--ctx-size", "96", "--serve", str(port)]
        if kind == "wine-vulkan":
            command += ["--device-index", "0", "--allow-cpu"]
        elif kind == "linux-vulkan-explicit":
            command += ["--device-index", str(vulkan_device)]
    elif kind == "linux-cuda":
        env["GOOSE_CUDA"] = "1"
        command = [str(root / "frankenmoe"), spec["model"], str(weight), str(port)]
    else:
        profile = spec.get("profile", ["--ctx-size", "256", "--batch-size", "32", "--ubatch-size", "32",
                                      "--threads", "8", "--threads-batch", "8"])
        if kind == "wine-cpu":
            command.insert(0, "wine")
            env.update(GOOSE_CUDA="0", WINEDEBUG="-all", WINEPREFIX=str(prefix))
            weight = "Z:" + str(weight.resolve()).replace("/", "\\")
        else:
            env["GOOSE_CUDA"] = "1"
            for variable, value in spec.get("variables", {}).items():
                env[variable] = value
            if spec.get("profile_env"):
                for line in (root / spec["profile_env"]).read_text().splitlines():
                    key, value = line.split("=", 1)
                    if re.fullmatch(r"(?:GOOSE_|I2_|GGML_)[A-Z0-9_]+", key):
                        env[key] = value
            for variable, file in spec.get("environment", {}).items():
                env[variable] = str(root / file)
        command += [*profile, str(weight), "--serve", str(port)]
    outcome_file = logs / f'{kind}-{spec["model"]}.log'
    with outcome_file.open("w") as log:
        proc = subprocess.Popen(command, env=env, cwd=root, stdout=log, stderr=subprocess.STDOUT)
        row["launch"] = ("PASS", "Executable started")
        try:
            url = f"http://127.0.0.1:{port}"
            until = time.monotonic() + (40 if kind == "wine-cpu" else 60)
            while proc.poll() is None and time.monotonic() < until:
                try:
                    fetch(url, "/health")
                    row["model_load"] = ("PASS", "HTTP /health with real weights")
                    break
                except (OSError, ValueError):
                    time.sleep(.2)
            else:
                row["model_load"] = ("FAIL", f"Server did not reach /health. See {outcome_file.name}")
                return

            payload = model["request"]
            if kind == "wine-cpu":
                payload = dict(payload, prompt=payload["prompt"][:1], n_predict=1)
            result = fetch(url, "/completion", payload, timeout=25 if kind == "wine-cpu" else 60)
            ids = result.get("tokens")
            if not isinstance(ids, list) or len(ids) != payload["n_predict"] or not all(type(i) is int for i in ids):
                raise ValueError(f"wrong token count or type: {ids!r}")
            row["generation"] = ("PASS", f"{len(ids)} generated token IDs")
            expected = model.get("wine_cpu_first_id") if kind == "wine-cpu" else model["expected_ids"]
            if expected is not None:
                if kind == "wine-cpu":
                    good = ids == [expected]
                else:
                    good = ids == expected and hashlib.sha256(result["content"].encode()).hexdigest() == model["expected_text_sha256"]
                row["correctness"] = ("PASS" if good else "FAIL", f"IDs {ids}")
            elif kind == "wine-cpu":
                row["correctness"] = ("NOT TESTED", "No independent CPU token reference")
            if kind.endswith("-cuda"):
                ram, vram = process_memory(proc.pid)
                ram_limit = model["ram_limit_mib"]
                vram_limit = model["vram_limit_mib"]
                evidence = f"{gpu}. Peak RSS {ram:.1f} MiB, sampled VRAM {vram} MiB"
                if not 0 < ram <= ram_limit or (vram is not None and not 0 < vram <= vram_limit):
                    row["memory"] = ("FAIL", evidence)
                elif vram is None:
                    row["memory"] = ("NOT TESTED" if kind == "windows-cuda" else "FAIL", evidence)
                else:
                    row["memory"] = ("PASS", evidence)
            elif vulkan:
                try:
                    ram = process_rss(proc.pid)
                    evidence = f"Peak process RSS {ram:.1f} MiB"
                    if not 0 < ram <= model["ram_limit_mib"]:
                        row["memory"] = ("FAIL", evidence)
                    else:
                        vram = None
                        if model.get("vram_limit_mib") and shutil.which("nvidia-smi"):
                            try:
                                _, vram = process_memory(proc.pid)
                            except (OSError, ValueError, KeyError, subprocess.SubprocessError):
                                pass
                        if vram is None:
                            row["memory"] = ("NOT TESTED", evidence + "; device-attributed VRAM unavailable")
                        else:
                            evidence += f", sampled VRAM {vram} MiB"
                            row["memory"] = ("PASS" if 0 < vram <= model["vram_limit_mib"] else "FAIL", evidence)
                except (OSError, ValueError, KeyError, subprocess.SubprocessError):
                    row["memory"] = ("NOT TESTED", "Process memory sampling unavailable")
        except (OSError, ValueError, KeyError, subprocess.SubprocessError) as e:
            step = "memory" if row["generation"][0] == "PASS" else "generation"
            row[step] = ("FAIL", f"{type(e).__name__}: {e}")
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()


def row_key(spec):
    return "/".join(spec[part] for part in ("os", "backend", "model")) + (
        "/" + spec["validation"] if spec.get("validation") else "")


def distribution_files(matrix, platform):
    allowed = {"README.md", "LICENSE"}
    for spec in matrix["rows"]:
        if spec["os"] == platform and spec["backend"] == "vulkan" and spec["entry"]:
            allowed.update((spec["entry"], *spec["files"]))
    return {PurePosixPath(name) for name in allowed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--linux", type=Path, required=True)
    parser.add_argument("--windows", type=Path, required=True)
    parser.add_argument("--sha256", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="fresh report/extraction directory")
    parser.add_argument("--weights", type=Path, help="run live checks using these GGUFs")
    parser.add_argument("--wine-prefix", type=Path, help="isolated Wine prefix; CUDA stays disabled")
    parser.add_argument("--gpu-index", type=int, default=0)
    parser.add_argument("--vulkan-device-index", type=int, help="separate Linux diagnostic using an explicit Vulkan device")
    parser.add_argument("--vulkan-wine-icd", type=Path, help="host ICD for the separate Wine Vulkan diagnostic")
    parser.add_argument("--only", action="append", default=[], metavar="OS/BACKEND/MODEL",
                        help="rerun only the specified model row; repeat to select multiple rows")
    parser.add_argument("--build-plan", type=Path, help="trusted JSON map of OS to build command and matching executable hashes")
    parser.add_argument("--matrix", type=Path, default=Path(__file__).with_name("release-matrix.json"))
    parser.add_argument("--distribution", choices=("all", "vulkan"), default="all",
                        help="physical archive scope; Vulkan-only archives may contain no CUDA package files")
    args = parser.parse_args()
    if args.vulkan_device_index is not None and not 0 <= args.vulkan_device_index < 2**32 - 1:
        parser.error("invalid Vulkan device index")
    if args.vulkan_wine_icd:
        args.vulkan_wine_icd = args.vulkan_wine_icd.resolve()
        if not args.vulkan_wine_icd.is_file():
            parser.error("Wine Vulkan ICD must be an existing JSON manifest")
    args.output = args.output.resolve()
    args.output.mkdir()
    weights = args.weights.resolve() if args.weights else None
    prefix = args.wine_prefix.resolve() if args.wine_prefix else None
    config = json.loads(args.matrix.read_text())
    registry = [row_key(spec) for spec in config["rows"]]
    rows = [spec for spec in config["rows"] if args.distribution == "all" or spec["backend"] == "vulkan"]
    if not rows:
        parser.error("the matrix has no rows for this distribution")
    keys = [row_key(spec) for spec in rows]
    required = {f"{platform}/{backend}/{model}"
                for backend, platforms in config["required_platforms"].items()
                for platform in platforms
                for model in config.get("backend_models", {}).get(backend, config["models"])}
    if len(set(registry)) != len(registry) or set(args.only) - set(keys) or required - set(registry):
        parser.error("duplicate rows, missing required combinations or unknown --only selection")
    build_plan = json.loads(args.build_plan.read_text()) if args.build_plan else {}
    expected = sums(args.sha256.read_text())
    report = {"distribution": args.distribution, "scope": args.only or "all", "archives": {}, "rows": {}}
    extracted = {}
    for os_name, archive in (("linux-x64", args.linux), ("windows-x64", args.windows)):
        digest = sha(archive)
        info = {
            "file": archive.name, "sha256": digest,
            "archive_sha256": ("PASS", digest) if expected.get(PurePosixPath(archive.name)) == digest else ("FAIL", "Archive digest mismatch"),
            "contents_sha256": ("NOT TESTED", ""),
        }
        report["archives"][os_name] = info
        if info["archive_sha256"][0] == "FAIL":
            continue
        try:
            root = extract(archive, args.output / os_name)
            files = check_contents(root)
            if args.distribution == "vulkan" and set(files) - distribution_files(config, os_name):
                raise ValueError("Vulkan-only archive contains files outside its declared distribution")
            info["contents_sha256"] = ("PASS", f"{len(files)} verified files")
            info["files"] = {str(path): checksum for path, checksum in sorted(files.items())}
            extracted[os_name] = root
        except (OSError, ValueError, tarfile.TarError, zipfile.BadZipFile) as e:
            info["contents_sha256"] = ("FAIL", str(e))
    build_outputs = {}
    for os_name, plan in build_plan.items():
        try:
            output = run(plan["argv"], timeout=plan["timeout_s"])
            (args.output / f"{os_name}-build.log").write_text(output)
            build_outputs[os_name] = {name: sha(Path(path)) for name, path in plan["outputs"].items()}
        except (OSError, ValueError, subprocess.SubprocessError) as e:
            build_outputs[os_name] = str(e)

    for spec in rows:
        os_name = spec["os"]
        key = row_key(spec)
        if args.only and key not in args.only:
            continue
        row = dict.fromkeys(CHECKS, ("NOT TESTED", ""))
        if not spec.get("wine_model_check"):
            row["wine_model_check"] = ("NOT APPLICABLE", "No metadata-only entry point")
        report["rows"][key] = row
        if spec["entry"] is None:
            row.update(dict.fromkeys(CHECKS, ("NOT APPLICABLE", "No runtime path in the architecture registry")))
            continue
        root = extracted.get(os_name)
        for check in ("archive_sha256", "contents_sha256"):
            row[check] = report["archives"][os_name][check]
        if root is None:
            continue
        built = build_outputs.get(os_name)
        if isinstance(built, str):
            row["build"] = ("FAIL", built)
        elif built is not None:
            good = built.get(spec["entry"]) == report["archives"][os_name]["files"].get(spec["entry"]) and spec["entry"] in built
            row["build"] = ("PASS" if good else "FAIL", "Built executable compared with packaged SHA-256")
        if static_checks(row, root, spec):
            wine_metadata(row, spec, config["models"][spec["model"]], root, weights, prefix)
            try:
                probe(row, spec, config["models"][spec["model"]], root, args.output, weights, prefix,
                      args.gpu_index, args.vulkan_device_index, args.vulkan_wine_icd)
            except (OSError, subprocess.SubprocessError) as e:
                row["launch"] = ("FAIL", str(e))

    statuses = [status for row in report["rows"].values() for status, _ in row.values()]
    statuses.extend(info[check][0] for info in report["archives"].values()
                    for check in ("archive_sha256", "contents_sha256"))
    report["verdict"] = "FAIL" if "FAIL" in statuses else "NOT TESTED" if "NOT TESTED" in statuses else "PASS"
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    lines = ["# Release Gate", "", f"Distribution: **{args.distribution}**", "",
             f"Verdict: **{report['verdict']}**", ""]
    if args.only:
        lines += ["Selective rerun: " + ", ".join(args.only), ""]
    for info in report["archives"].values():
        lines.append(f"- `{info['file']}` — SHA-256 `{info['sha256']}`")
    lines += ["", "| Platform / backend / model | " + " | ".join(CHECKS) + " |",
              "| " + " | ".join(["---"] * (len(CHECKS) + 1)) + " |"]
    for key, row in report["rows"].items():
        lines.append(f"| {key} | " + " | ".join(row[k][0] for k in CHECKS) + " |")
    lines += ["", "See report.json for evidence, reasons, and the complete file inventory."]
    (args.output / "report.md").write_text("\n".join(lines) + "\n")
    print(args.output / "report.md")
    return {"PASS": 0, "FAIL": 1, "NOT TESTED": 2}[report["verdict"]]


if __name__ == "__main__":
    raise SystemExit(main())
