"""Stage Vulkan alongside byte-identical frozen CUDA package contents."""

import argparse
import importlib.util
from pathlib import Path
import re
import shutil
import tarfile
import zipfile


HERE = Path(__file__).resolve().parent
MODULE = importlib.util.spec_from_file_location("gate", HERE / "release-gate.py")
gate = importlib.util.module_from_spec(MODULE)
MODULE.loader.exec_module(gate)


def stage(output, version, platform, base, expected, binary):
    name = f"FrankenMoE-{version}-{platform}"
    package = output / name
    release_root = HERE.parents[1]
    original = {}
    if base:
        if expected.get(Path(base.name)) != gate.sha(base):
            raise ValueError(f"{base.name}: base archive hash mismatch")
        unpacked = gate.extract(base, output / f"base-{platform}")
        original = gate.check_contents(unpacked)
        shutil.move(unpacked, package)
    else:
        package.mkdir()
        shutil.copy2(release_root / "LICENSE", package / "LICENSE")
        (package / "README.md").write_text("# FrankenMoE Vulkan Alpha\n\nSee [Vulkan setup](vulkan/README.md).\n")
    payload = package / "vulkan"
    payload.mkdir()
    suffix = ".exe" if platform == "windows-x64" else ""
    target = payload / f"frankenmoe-vulkan{suffix}"
    shutil.copy2(binary, target)
    shutil.copy2(release_root / "docs/releases/VULKAN-ALPHA.md", payload / "README.md")
    files = sorted(path for path in package.rglob("*") if path.is_file() and path.name != "CONTENTS.sha256")
    (package / "CONTENTS.sha256").write_text(
        "".join(f"{gate.sha(path)}  {path.relative_to(package).as_posix()}\n" for path in files))
    staged = gate.check_contents(package)
    if any(staged.get(path) != digest for path, digest in original.items()):
        raise ValueError(f"{platform}: a frozen CUDA package file changed")
    if gate.sha(binary) != gate.sha(target):
        raise ValueError(f"{platform}: packaged Vulkan binary changed")
    if suffix:
        archive = output / f"{name}.zip"
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zipped:
            for path in [*files, package / "CONTENTS.sha256"]:
                zipped.write(path, path.relative_to(output).as_posix())
    else:
        archive = output / f"{name}.tar.gz"
        with tarfile.open(archive, "w:gz") as packed:
            packed.add(package, arcname=name)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--base-linux", type=Path)
    parser.add_argument("--base-windows", type=Path)
    parser.add_argument("--base-sha256", type=Path)
    parser.add_argument("--vulkan-linux", type=Path, required=True)
    parser.add_argument("--vulkan-windows", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"v\d+\.\d+\.\d+-[a-z0-9-]+", args.version):
        parser.error("invalid pre-release version")
    base = (args.base_linux, args.base_windows, args.base_sha256)
    if any(base) and not all(base):
        parser.error("provide both CUDA archives and their checksum receipt, or none for Vulkan only")
    args.output.mkdir()
    expected = gate.sums(args.base_sha256.read_text()) if args.base_sha256 else {}
    archives = [
        stage(args.output, args.version, "linux-x64", args.base_linux, expected, args.vulkan_linux),
        stage(args.output, args.version, "windows-x64", args.base_windows, expected, args.vulkan_windows),
    ]
    (args.output / "SHA256SUMS").write_text("".join(f"{gate.sha(path)}  {path.name}\n" for path in archives))
    for archive in archives:
        print(archive)


if __name__ == "__main__":
    main()
