#!/usr/bin/env python3
"""FrankenMoE terminal console: banner, live telemetry panels and a tagged event log.

The console only reads: the server's own log stream, `nvidia-smi` and /proc/meminfo. It never
talks to the inference process, so it can be attached to a running server at any time.

    # follow a log file written by a running server (e.g. `... 2>&1 | tee server.log`)
    python3 tools/frankenmoe-console.py --log server.log --model /path/Apex-35B-V1.0-Q4_K_M.gguf

    # read the server output from a pipe
    frankenmoe-server ... 2>&1 | python3 tools/frankenmoe-console.py --stdin

    # start the server as a child and show its output through the console
    python3 tools/frankenmoe-console.py --save server.log -- ./frankenmoe-server <args>

    # render one frame from a finished log (no screen switching), for checks and screenshots
    python3 tools/frankenmoe-console.py --replay server.log --once --width 132

Options: --build LABEL overrides the build shown in the banner (default: git branch and short
hash of this checkout), --profile LABEL sets the footer profile (default: model + last prompt),
--client LABEL the footer client (default OpenCode), --no-color for plain text.
Colors use 24-bit ANSI when COLORTERM advertises it and 256 colors otherwise.
"""
import argparse, collections, datetime, os, re, select, shutil, signal, struct, subprocess, sys, time, unicodedata

# ---------------------------------------------------------------- colors and text width

TRUECOLOR = os.environ.get("COLORTERM", "").lower() in ("truecolor", "24bit")
PLAIN = False


def rgb(r, g, b):
    if PLAIN:
        return ""
    if TRUECOLOR:
        return f"\x1b[38;2;{r};{g};{b}m"
    to6 = lambda v: 0 if v < 48 else (1 if v < 115 else (v - 35) // 40)
    return f"\x1b[38;5;{16 + 36 * to6(r) + 6 * to6(g) + to6(b)}m"


def style(code):
    return "" if PLAIN else f"\x1b[{code}m"


RESET, BOLD, DIM = (lambda: style("0")), (lambda: style("1")), (lambda: style("2"))
WHITE = lambda: rgb(235, 235, 235)
GREY = lambda: rgb(150, 150, 150)
YELLOW = lambda: rgb(255, 214, 10)
ORANGE = lambda: rgb(255, 140, 26)
RED = lambda: rgb(235, 50, 20)
GREEN = lambda: rgb(80, 230, 60)
CYAN = lambda: rgb(40, 210, 230)
MAGENTA = lambda: rgb(230, 70, 230)
BLUE = lambda: rgb(70, 150, 255)
PURPLE = lambda: rgb(170, 110, 255)

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


def cell_width(ch):
    if unicodedata.combining(ch) or ch in "\u200d\ufe0f":
        return 0
    o = ord(ch)
    if unicodedata.east_asian_width(ch) in "WF" or 0x1F300 <= o <= 0x1FAFF:
        return 2
    return 1


def width(text):
    return sum(cell_width(c) for c in ANSI.sub("", text))


def pad(text, cols):
    """Pad or clip a colored string to exactly `cols` terminal cells."""
    out, used, i = [], 0, 0
    while i < len(text):
        m = ANSI.match(text, i)
        if m:
            out.append(m.group()); i = m.end(); continue
        w = cell_width(text[i])
        if used + w > cols:
            break
        out.append(text[i]); used += w; i += 1
    return "".join(out) + RESET() + " " * (cols - used)


def gradient(text, stops):
    """Color each visible column along a list of RGB stops."""
    n = max(1, len(text) - 1)
    out = []
    for i, ch in enumerate(text):
        t = i / n * (len(stops) - 1)
        k = min(int(t), len(stops) - 2)
        f = t - k
        r, g, b = (int(stops[k][j] + (stops[k + 1][j] - stops[k][j]) * f) for j in range(3))
        out.append(ch if ch == " " else rgb(r, g, b) + ch)
    return "".join(out) + RESET()


FIRE = [(200, 20, 10), (255, 90, 0), (255, 170, 20), (255, 230, 80)]


def group3(n):
    return f"{int(n):,}".replace(",", " ")


# ---------------------------------------------------------------- banner art

GOOSE = [
    " (  ) ( (   ____        ",
    "( )( ) ) )_/ o  \\___    ",
    " )( ( ( (/ ,_     __>   ",
    "( ) ) ) /  / \\___/      ",
    " ) ( ( /  /             ",
    "( ( ) /__/              ",
]


def paint_goose(line):
    out, bird = [], False
    for i, ch in enumerate(line):
        if ch in "_/\\" or bird and ch not in "()":
            bird = True
        if ch in "()":
            t = i / 9
            out.append(gradient(ch, FIRE[:2] if t < 0.4 else FIRE[1:3] if t < 0.8 else FIRE[2:]))
        elif ch == "o":
            out.append(YELLOW() + BOLD() + "o" + RESET())
        elif ch == ">":
            out.append(ORANGE() + BOLD() + ">" + RESET())
        elif ch == " ":
            out.append(" ")
        else:
            out.append(WHITE() + ch)
    return "".join(out) + RESET()


FONT = {  # three-row block font, slanted at render time
    "A": ["▄▀▀▄", "█▀▀█", "▀  ▀"], "E": ["█▀▀", "█▀▀", "▀▀▀"], "F": ["█▀▀", "█▀ ", "▀  "],
    "G": ["▄▀▀▀", "█ ▀█", " ▀▀▀"], "I": ["█", "█", "▀"], "K": ["█ ▄▀", "██▄ ", "▀  ▀"],
    "M": ["█▄ ▄█", "█ ▀ █", "▀   ▀"], "N": ["█▄  █", "█ ▀▄█", "▀   ▀"], "O": ["▄▀▀▄", "█  █", " ▀▀ "],
    "R": ["█▀▀▄", "█▀▀▄", "▀  ▀"], "S": ["▄▀▀▀", " ▀▀▄", "▀▀▀ "], " ": ["  ", "  ", "  "],
}


def big(text):
    rows = ["", "", ""]
    for ch in text:
        glyph = FONT[ch]
        for r in range(3):
            rows[r] += glyph[r] + " "
    return [" " * (2 - r) + rows[r] for r in range(3)]  # italic slant


def title_rows(marks=True):
    top = [gradient(r, [(250, 250, 250), (200, 200, 200), (250, 250, 250)]) for r in big("FRANKENMOE")]
    bottom = [gradient(r, FIRE) for r in big("GOOSE ON FIRE")]
    if marks:
        bottom = [ORANGE() + "⸙ " + RESET() + b + " " + ORANGE() + "⸙" + RESET() for b in bottom]
    return top + bottom


# ---------------------------------------------------------------- telemetry

class Telemetry:
    def __init__(self, args):
        self.args = args
        self.events = collections.deque(maxlen=200)
        self.decode = None; self.decode_hist = collections.deque(maxlen=240); self.decode_min = None
        self.decode_avg = []; self.decode_progress = ""
        self.prefill = None; self.prefill_tokens = None; self.prefill_seconds = None
        self.context = None; self.kv = "Q4_0/Q4_0"; self.cache = None; self.router = "—"
        self.prefetch_on = False; self.prefetch = {}
        self.layers = None; self.experts = None; self.active = None; self.model_name = None
        self.gpu = {}; self.ram = (0, 0); self.cuda_version = None; self.last_gpu = 0
        self.build = args.build or git_build()
        if args.model:
            self.read_gguf(args.model)

    def read_gguf(self, path):
        try:
            meta = gguf_metadata(path, ("block_count", "expert_count", "expert_used_count", "general.name"))
        except (OSError, ValueError, struct.error):
            return
        self.layers = meta.get("block_count", self.layers)
        self.experts = meta.get("expert_count")
        self.active = meta.get("expert_used_count")
        name = meta.get("general.name") or os.path.basename(path)
        self.model_name = name.split("-")[0].upper()

    # one line of server output
    def feed(self, raw):
        line = ANSI.sub("", raw.rstrip("\n"))
        if not line.strip():
            return
        stamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        tag, text = classify(line, self)
        if tag:
            self.events.append((stamp, tag, text))

    def sample_system(self):
        now = time.monotonic()
        if now - self.last_gpu < 1.0:
            return
        self.last_gpu = now
        try:
            out = subprocess.run(["nvidia-smi", "--query-gpu=name,memory.used,memory.total,utilization.gpu,temperature.gpu",
                                  "--format=csv,noheader,nounits"], capture_output=True, text=True, timeout=2).stdout
            name, used, total, util, temp = [x.strip() for x in out.splitlines()[0].split(",")]
            self.gpu = dict(name=name, used=float(used), total=float(total), util=float(util), temp=float(temp))
            if self.cuda_version is None:
                head = subprocess.run(["nvidia-smi"], capture_output=True, text=True, timeout=2).stdout
                m = re.search(r"CUDA Version:\s*([0-9.]+)", head)
                self.cuda_version = m.group(1) if m else ""
        except (OSError, ValueError, IndexError, subprocess.SubprocessError):
            pass
        try:
            info = dict(l.split(":", 1) for l in open("/proc/meminfo"))
            total = int(info["MemTotal"].split()[0]); avail = int(info["MemAvailable"].split()[0])
            self.ram = ((total - avail) / 1024 / 1024, total / 1024 / 1024)
        except (OSError, KeyError, ValueError):
            pass


def gpu_label(g):
    if not g:
        return "—"
    name = g.get("name", "").replace("GeForce ", "")
    return f"{name} ({g.get('total', 0) / 1024:.0f} GB)"


def git_build():
    here = os.path.dirname(os.path.abspath(__file__))
    try:
        branch = subprocess.run(["git", "-C", here, "rev-parse", "--abbrev-ref", "HEAD"], capture_output=True, text=True).stdout.strip()
        sha = subprocess.run(["git", "-C", here, "rev-parse", "--short=7", "HEAD"], capture_output=True, text=True).stdout.strip()
        return f"{branch.split('/')[-1]} ({sha})" if branch else "unknown"
    except OSError:
        return "unknown"


def gguf_metadata(path, wanted):
    """Read selected scalar keys from a GGUF header (suffix match on the key name)."""
    sizes = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
    fmt = {0: "<B", 1: "<b", 2: "<H", 3: "<h", 4: "<I", 5: "<i", 6: "<f", 7: "<?", 10: "<Q", 11: "<q", 12: "<d"}
    out = {}
    with open(path, "rb") as f:
        if f.read(4) != b"GGUF":
            raise ValueError("not a GGUF file")
        version, = struct.unpack("<I", f.read(4))
        _tensors, kv_count = struct.unpack("<QQ", f.read(16))
        rd_str = lambda: f.read(struct.unpack("<Q", f.read(8))[0]).decode("utf-8", "replace")
        for _ in range(kv_count):
            key = rd_str(); kind, = struct.unpack("<I", f.read(4))
            if kind == 8:
                value = rd_str()
            elif kind == 9:
                sub, n = struct.unpack("<IQ", f.read(12))
                if sub == 8:
                    for _ in range(n): rd_str()
                else:
                    f.seek(sizes[sub] * n, 1)
                continue
            else:
                value, = struct.unpack(fmt[kind], f.read(sizes[kind]))
            for w in wanted:
                if key == w or key.endswith("." + w):
                    out[w] = value
            if len(out) == len(wanted):
                break
    return out


RE_DECODE = re.compile(r"\[decode\]\s+(\d+)\s*/\s*(\d+) tokens \| ([0-9.]+) tok/s")
RE_SLOT_DEC = re.compile(r"\[slot\]\s+decode\s+(\d+) tokens \| ([0-9.]+) ms \| ([0-9.]+) tok/s")
RE_SLOT_PRE = re.compile(r"\[slot\]\s+prefill\s+(\d+) tokens \| ([0-9.]+) ms \| ([0-9.]+) tok/s")
RE_PREFILL = re.compile(r"\[prefill\]\s+(\d+)\s*/\s*(\d+) tokens \| ([0-9.]+) tok/s \| ([0-9.]+) s(.*)")
RE_CONTEXT = re.compile(r"context[ =](\d+)")
RE_PF = re.compile(r"expert prefetch launched=(\d+) waves=(\d+) jobs=(\d+) skipped=(\d+)(?: us launch=(\d+) wait=(\d+) prepare=(\d+) submit=(\d+))?")


def classify(line, t):
    """Update telemetry from one line and return the (tag, text) shown in the event log."""
    body = line.split("]", 1)[1].strip() if line.lstrip().startswith(("🪿", "🔥", "🧱", "[")) or "] " in line[:16] else line
    m = RE_DECODE.search(line)
    if m:
        n, total, rate = int(m.group(1)), int(m.group(2)), float(m.group(3))
        t.decode = rate; t.decode_hist.append(rate); t.decode_avg.append(rate)
        t.decode_min = rate if t.decode_min is None else min(t.decode_min, rate)
        t.decode_progress = f"{n} / {total}"
        return "DECODE", f"{n} / {total} tokens  |  {GREEN()}{rate:.2f} tok/s{RESET()}  (recent)"
    m = RE_SLOT_DEC.search(line)
    if m:
        rate = float(m.group(3)); t.decode = rate; t.decode_hist.append(rate); t.decode_avg.append(rate)
        t.decode_min = rate if t.decode_min is None else min(t.decode_min, rate)
        return "DECODE", f"{m.group(1)} tokens  |  {float(m.group(2)):.0f} ms  |  {GREEN()}{rate:.2f} tok/s{RESET()}  (final)"
    m = RE_SLOT_PRE.search(line)
    if m:
        t.prefill = float(m.group(3)); t.prefill_tokens = int(m.group(1)); t.prefill_seconds = float(m.group(2)) / 1000
        return "PREFILL", f"{m.group(1)} tokens  |  {float(m.group(2)):.0f} ms  |  {CYAN()}{t.prefill:.2f} tok/s{RESET()}"
    m = RE_PREFILL.search(line)
    if m:
        done = "done" in m.group(5)
        if done:
            t.prefill = float(m.group(3)); t.prefill_tokens = int(m.group(2)); t.prefill_seconds = float(m.group(4))
        return "PREFILL", f"{m.group(1)} / {m.group(2)} tokens  |  {CYAN()}{float(m.group(3)):.2f} tok/s{RESET()}  |  {m.group(4)} s{'  (done)' if done else ''}"
    m = RE_PF.search(line)
    if m:
        t.prefetch_on = True
        keys = ("launched", "waves", "jobs", "skipped", "launch", "wait", "prepare", "submit")
        t.prefetch = {k: int(v) for k, v in zip(keys, m.groups()) if v is not None}
        return "PREFETCH", f"expert prefetch launched  |  waves={m.group(2)}  jobs={m.group(3)}  skipped={m.group(4)}"
    if "expert prefetch from predicted routes active" in line:
        t.prefetch_on = True
        return "PREFETCH", "expert prefetch from predicted routes active" + line.split("active", 1)[1].replace("k=", "  |  k=")
    if "router softmax" in line:
        t.router = "Softmax"
    m = re.search(r"compact route weights consumed=(\d+)", line)
    if m:
        return "ROUTER", f"compact route weights consumed={m.group(1)}"
    m = re.search(r"one-touch SLRU enabled at (\d+) MiB", line)
    if m:
        t.cache = int(m.group(1))
    if "loading model" in line:
        c = RE_CONTEXT.search(line)
        if c: t.context = int(c.group(1))
        return "INIT", f"FrankenMoE  |  loading model  |  context={c.group(1) if c else '?'} toks"
    if "[model]" in line:
        m = re.search(r"layers (\d+) \| vocabulary (\d+) \| resident (\d+) / (\d+) MiB", line)
        if m:
            t.layers = int(m.group(1))
            return "MODEL", f"layers={m.group(1)}  |  vocab={m.group(2)}  |  resident={m.group(3)} / {m.group(4)} MiB"
        return "MODEL", body
    if "[slot]" in line:
        c = re.search(r"context (\d+)", line)
        if c: t.context = int(c.group(1))
        k = re.search(r"KV (\w+)/(\w+)", line)
        if k: t.kv = f"{k.group(1).upper()}/{k.group(2).upper()}"
        return "SLOT", body
    if "[cuda]" in line:
        e = re.search(r"experts (\d+) MiB", line)
        if e and t.cache is None: t.cache = int(e.group(1))
        return "CUDA", body
    if "[http]" in line or line.startswith("listening on"):
        return "HTTP", body
    if "[kv]" in line:
        return "KV", body
    if line.startswith("Goose CUDA"):
        text = line.split(":", 1)[1].strip()
        text = text.replace("Attention", "attention")
        return "CUDA", text
    return "LOG", line.strip()


# ---------------------------------------------------------------- rendering

TAGS = {
    "INIT": ("⚙", BLUE, CYAN), "CUDA": ("◆", GREEN, GREEN), "MODEL": ("▣", YELLOW, YELLOW),
    "PREFETCH": ("➶", MAGENTA, MAGENTA), "ROUTER": ("⑂", PURPLE, PURPLE), "DECODE": ("🔥", ORANGE, GREEN),
    "PREFILL": ("≋", CYAN, CYAN), "SLOT": ("◇", GREY, WHITE), "HTTP": ("⇄", BLUE, BLUE),
    "KV": ("▤", GREY, WHITE), "LOG": ("·", GREY, GREY),
}


def box(lines, cols, title_color=WHITE):
    """Dashed rounded box around already-colored lines."""
    edge = GREY()
    top = edge + "╭" + "┄" * (cols - 2) + "╮" + RESET()
    bottom = edge + "╰" + "┄" * (cols - 2) + "╯" + RESET()
    return [top] + [edge + "┆" + RESET() + pad(" " + l, cols - 2) + edge + "┆" + RESET() for l in lines] + [bottom]


def bar(frac, cols, color):
    fill = max(0, min(cols, round(frac * cols)))
    return color() + "[" + "█" * fill + DIM() + "▒" * (cols - fill) + RESET() + color() + "]" + RESET()


def spark(values, cols):
    if not values:
        return " " * cols
    vals = list(values)[-cols:]
    lo, hi = min(vals), max(vals)
    ticks = "▁▂▃▄▅▆▇█"
    s = "".join(ticks[0 if hi == lo else int((v - lo) / (hi - lo) * 7)] for v in vals)
    return gradient(s.rjust(cols), [(255, 120, 0), (255, 200, 40)])


def render(t, cols, rows):
    out = []
    # banner
    goose = [paint_goose(g.rstrip().ljust(22)) for g in GOOSE]
    g = t.gpu
    vram_used, vram_total = g.get("used", 0) / 1024, g.get("total", 0) / 1024
    vram_pct = 100 * vram_used / vram_total if vram_total else 0
    cuda = f"  {GREEN()}CUDA {t.cuda_version}{RESET()}" if t.cuda_version else ""
    info = [
        f"{WHITE()}Apex Runtime (Goose){RESET()}",
        f"{WHITE()}Build  : {t.build}{RESET()}",
        f"{WHITE()}GPU    : {gpu_label(g)}{RESET()}{cuda}",
        f"{WHITE()}VRAM   : {vram_used:.2f} / {vram_total:.2f} GB  {GREEN()}({vram_pct:.1f}%){RESET()}",
        f"{WHITE()}Context: {group3(t.context) + ' tokens' if t.context else '—'}{RESET()}",
        "",
    ]
    info_w = max(width(x) for x in info) + 1
    inner = cols - 4

    def art(space):
        """Best goose + title combination that fits `space` columns, or None."""
        for marks, goose_on in ((True, True), (False, True), (True, False), (False, False)):
            titles = title_rows(marks)
            need = max(width(x) for x in titles) + (24 if goose_on else 0)
            if need <= space:
                return [(goose[i] + "  " if goose_on else "") + titles[i] for i in range(6)]
        return None

    side = art(inner - info_w - 2)
    if side:
        for i in range(6):
            out.append(pad(side[i], inner - info_w - 2) + GREY() + "│ " + RESET() + info[i])
    else:
        top = art(inner) or [gradient("FRANKENMOE", [(250, 250, 250), (200, 200, 200)]),
                             gradient("GOOSE ON FIRE", FIRE)]
        out += top + [GREY() + "┄" * inner + RESET()] + info[:5]
    out = box(out, cols)

    # metric cells
    inner = cols - 2
    per_row = 6 if inner >= 126 else 3
    shares = [24, 22, 21, 21, 12, 12] if per_row == 6 else [1, 1, 1, 1, 1, 1]

    def row_widths(group):
        ws = [inner * shares[k] // sum(shares[j] for j in group) for k in group]
        ws[-1] = inner - sum(ws[:-1])
        return ws
    groups = [list(range(6))] if per_row == 6 else [[0, 1, 2], [3, 4, 5]]
    widths = {}
    for gr in groups:
        widths.update(zip(gr, row_widths(gr)))
    cw = widths[2]
    dec = f"{t.decode:.2f}" if t.decode is not None else "—"
    avg = sum(t.decode_avg) / len(t.decode_avg) if t.decode_avg else None
    pre = f"{t.prefill:,.2f}".replace(",", " ") if t.prefill is not None else "—"
    ram_used, ram_total = t.ram
    temp = g.get("temp")
    c = [
        (f"{YELLOW()}⚡ {BOLD()}DECODE{RESET()}", f"{GREEN()}{BOLD()}{dec}{RESET()} {GREEN()}tok/s{RESET()}",
         f"{GREY()}(avg: {avg:.2f}  min: {t.decode_min:.2f}){RESET()}" if avg is not None else f"{GREY()}(waiting){RESET()}"),
        (f"{CYAN()}≋ {BOLD()}PREFILL{RESET()}", f"{CYAN()}{BOLD()}{pre}{RESET()} {CYAN()}tok/s{RESET()}",
         f"{CYAN()}({t.prefill_tokens} toks · {t.prefill_seconds:.2f} s){RESET()}" if t.prefill_tokens else ""),
        (f"{MAGENTA()}▣ {BOLD()}VRAM{RESET()}", bar(vram_pct / 100, max(4, widths[2] - 12), MAGENTA) + f" {MAGENTA()}{vram_pct:.1f}%{RESET()}",
         f"{WHITE()}{vram_used:.2f} / {vram_total:.2f} GB{RESET()}"),
        (f"{BLUE()}▦ {BOLD()}RAM{RESET()}", bar(ram_used / ram_total if ram_total else 0, max(4, widths[3] - 12), BLUE) +
         f" {BLUE()}{100 * ram_used / ram_total if ram_total else 0:.1f}%{RESET()}", f"{WHITE()}{ram_used:.1f} / {ram_total:.0f} GB{RESET()}"),
        (f"{YELLOW()}▂▄▆ {BOLD()}GPU{RESET()}", f"{YELLOW()}{BOLD()}{g.get('util', 0):.0f}%{RESET()}", ""),
        (f"{ORANGE()}🌡 {BOLD()}TEMP{RESET()}", f"{ORANGE()}{BOLD()}{temp:.0f} °C{RESET()}" if temp is not None else "—", ""),
    ]
    lines = []
    for n, gr in enumerate(groups):
        row = [[], [], []]
        for i in gr:
            last = i == gr[-1]
            sep = "" if last else GREY() + "│" + RESET()
            for r in range(3):
                row[r].append(pad(" " + c[i][r], widths[i] - (0 if last else 1)) + sep)
        if n:
            lines.append(GREY() + "┄" * (inner - 2) + RESET())
        lines += ["".join(r) for r in row]
    out += box(lines, cols)

    # experts and prefetch strips
    sep = f" {GREY()}|{RESET()} "
    def wrap(items):
        lines, cur = [], ""
        for item in items:
            cand = cur + sep + item if cur else item
            if cur and width(cand) > inner - 2:
                lines.append(cur); cur = "  " + item
            else:
                cur = cand
        return lines + [cur]
    exp = [f"{YELLOW()}👥 {BOLD()}EXPERTS:{RESET()} {WHITE()}{t.experts or t.layers or '—'}{RESET()}",
           f"{WHITE()}Active: {t.active or 8}{RESET()}", f"{WHITE()}Router: {MAGENTA()}{t.router}{RESET()}",
           f"{WHITE()}KV: {t.kv}{RESET()}", f"{WHITE()}Cache: {GREEN()}{t.cache if t.cache else '—'} MiB{RESET()}"]
    out += box(wrap(exp), cols)
    p = t.prefetch
    state = f"{GREEN()}ON{RESET()}" if t.prefetch_on else f"{GREY()}OFF{RESET()}"
    pf = [f"{MAGENTA()}🚀 {BOLD()}PREFETCH:{RESET()} {state}"]
    for label, key in (("Waves", "waves"), ("Jobs", "jobs"), ("Skipped", "skipped"), ("Launch", "launch"),
                       ("Wait", "wait"), ("Prepare", "prepare"), ("Submit", "submit")):
        if key in p:
            value = group3(p[key]).replace(" ", " ")
            pf.append(f"{WHITE()}{label}: {GREEN() if key == 'skipped' else WHITE()}{value}{RESET()}")
    out += box(wrap(pf), cols)

    # event log
    footer_h = 3
    log_h = min(12, len(t.events)) if rows is None else max(3, rows - len(out) - footer_h)
    events = list(t.events)[-log_h:]
    for stamp, tag, text in events:
        icon, tag_color, _ = TAGS.get(tag, TAGS["LOG"])
        label = f"[{tag:<8}]" if tag != "INIT" else "[INIT    ]"
        icon = icon + " " * (2 - width(icon))
        out.append(pad(f" {WHITE()}[{stamp}]{RESET()}  {tag_color()}{icon}{RESET()} {tag_color()}{BOLD()}{label}{RESET()}  {WHITE()}{text}", cols))
    out += [" " * cols] * (log_h - len(events))

    # footer
    tokens = t.prefill_tokens or 0
    size = f"{round(tokens / 1024)}K" if tokens >= 1024 else (str(tokens) if tokens else "")
    profile = t.args.profile or f"{t.model_name or 'APEX'} {size}".strip()
    left = (f"{WHITE()}🪿 {ORANGE()}FrankenMoE-CUDA (Goose){RESET()}" + sep + f"{WHITE()}{t.build.split(' ')[0]}{RESET()}" + sep +
            f"{WHITE()}{profile}{RESET()}" + sep + f"{WHITE()}{t.args.client}{RESET()}")
    sig = GREEN() + "▂▃▄▅▆▇█" + RESET() if t.decode else GREY() + "▂▃▄▅▆▇█" + RESET()
    spark_w = cols - 4 - width(left) - 12
    if spark_w >= 8:
        out += box([pad(left, width(left) + 2) + spark(t.decode_hist, spark_w) + "  " + sig], cols)
    else:
        out += box([left, spark(t.decode_hist, inner - 13) + "  " + sig], cols)
    return out if rows is None else out[:rows]


# ---------------------------------------------------------------- input loop

def main():
    global PLAIN
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--log", help="follow a server log file")
    src.add_argument("--stdin", action="store_true", help="read server output from stdin")
    src.add_argument("--replay", help="read a finished log once")
    ap.add_argument("--save", help="also append the raw server output to this file")
    ap.add_argument("--model", help="GGUF path for the expert counts shown in the console")
    ap.add_argument("--build"); ap.add_argument("--profile"); ap.add_argument("--client", default="OpenCode")
    ap.add_argument("--once", action="store_true", help="print one frame and exit (with --replay)")
    ap.add_argument("--width", type=int); ap.add_argument("--height", type=int)
    ap.add_argument("--no-color", action="store_true")
    ap.add_argument("command", nargs=argparse.REMAINDER, help="-- server command to start as a child")
    args = ap.parse_args()
    PLAIN = args.no_color or os.environ.get("NO_COLOR") is not None
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    t = Telemetry(args)

    def size():
        s = shutil.get_terminal_size((132, 40))
        return args.width or s.columns, args.height or s.lines

    if args.replay:
        for line in open(args.replay, errors="replace"):
            t.feed(line)
        t.sample_system()
        cols, rows = size()
        print("\n".join(render(t, cols, args.height)))
        return 0

    save = None
    save_error = None

    def stop_saving(error=None):
        nonlocal save, save_error
        if save is not None:
            try:
                save.close()
            except OSError as exc:
                if error is None:
                    error = exc
            save = None
        if error is not None and save_error is None:
            save_error = f"Could not save the server log: {error}"
            t.feed(save_error)

    if args.save:
        try:
            save = open(args.save, "a", buffering=1)
        except OSError as exc:
            stop_saving(exc)

    child = None
    if command:
        child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        stream = child.stdout
    elif args.log:
        stream = open(args.log, "rb")
        stream.seek(0)
    else:
        stream = sys.stdin.buffer

    sys.stdout.write("\x1b[?1049h\x1b[?25l")
    stopping = False
    tail = collections.deque(maxlen=12)

    def stop(signum, _frame):
        nonlocal stopping
        stopping = True
        if child is not None and child.poll() is None:
            child.send_signal(signum)

    def feed(raw):
        text = raw.decode("utf-8", "replace")
        tail.append(text)
        if save is not None:
            try:
                save.write(text + "\n")
            except OSError as exc:
                stop_saving(exc)
        t.feed(text)

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    buf = b""
    try:
        last = 0.0
        while not stopping:
            ready = select.select([stream], [], [], 0.25)[0] if not args.log else [stream]
            if ready:
                chunk = os.read(stream.fileno(), 65536)
                if chunk:
                    buf += chunk
                    *lines, buf = buf.split(b"\n")
                    for raw in lines:
                        feed(raw)
                elif args.log:
                    time.sleep(0.2)
                else:
                    if buf:
                        feed(buf)
                    break
            now = time.monotonic()
            if now - last >= 0.25:
                t.sample_system()
                cols, rows = size()
                sys.stdout.write("\x1b[H" + "\n".join(render(t, cols, rows)))
                sys.stdout.flush()
                last = now
    except BaseException:
        if child is not None and child.poll() is None:
            child.terminate()
            child.wait()
        raise
    finally:
        sys.stdout.write("\x1b[?25h\x1b[?1049l")
        sys.stdout.flush()
        stop_saving()
        if save_error is not None:
            print(save_error, file=sys.stderr)
    status = child.wait() if child is not None else 0
    if status > 0 and not stopping:
        for line in tail:
            print(line, file=sys.stderr)
    return 128 - status if status < 0 else status


if __name__ == "__main__":
    sys.exit(main())
