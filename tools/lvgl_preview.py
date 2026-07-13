# lvgl_preview.py
#
# Parses thermostat_480.yaml and thermostat_240.yaml, including the shared
# thermostat_common.yaml package, and renders both LVGL layouts as an HTML
# preview. This is not a replacement for real LVGL rendering, but it is useful
# for checking positions, sizes, labels, and spacing.
#
# Usage:
#   python tools\lvgl_preview.py          -> watch mode with a local server at
#       http://localhost:8123/tools/preview.html. The browser reloads whenever
#       the 480/240/common YAML files change. Stop with Ctrl+C.
#   python tools\lvgl_preview.py --once   -> generate once
#
# Output: tools\preview.html

import re
import math
import sys
import time
import threading
import webbrowser
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
COMMON_FILE = PROJECT / "thermostat_common.yaml"
OUT_FILE = HERE / "preview.html"
VERSION_FILE = HERE / "preview_version.txt"
PORT = 8123

# Board variants: name -> (YAML file, native panel size)
BOARDS = [
    ("480x480", PROJECT / "thermostat_480.yaml", 480),
    ("240x240", PROJECT / "thermostat_240.yaml", 240),
]
WATCH_FILES = [COMMON_FILE] + [p for _, p, _ in BOARDS]


# ---- Load ESPHome YAML. Unknown tags like !lambda/!secret are ignored,
# while !include is resolved for packages. ----
class EsphomeLoader(yaml.SafeLoader):
    pass


def _unknown(loader, tag_suffix, node):
    if isinstance(node, yaml.ScalarNode):
        return f"<{node.tag}>"
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node)
    return loader.construct_mapping(node)


def _include(loader, node):
    rel = loader.construct_scalar(node)
    path = PROJECT / rel
    return load_yaml(path)


EsphomeLoader.add_multi_constructor("!", _unknown)
EsphomeLoader.add_constructor("!include", _include)


def load_yaml(path):
    raw = path.read_text(encoding="utf-8")
    return yaml.load(raw, Loader=EsphomeLoader) or {}


def merge_config(old, new):
    """Simplified packages merge: dicts recurse, lists append."""
    if isinstance(new, dict):
        if not isinstance(old, dict):
            return new
        res = dict(old)
        for k, v in new.items():
            res[k] = merge_config(old.get(k), v)
        return res
    if isinstance(new, list):
        if not isinstance(old, list):
            return new
        return old + new
    if new is None:
        return old
    return new


def load_board_config(path):
    """Load a board YAML file and merge in its packages."""
    config = load_yaml(path)
    packages = config.pop("packages", {}) or {}
    merged = {}
    for pkg_config in packages.values():
        merged = merge_config(merged, pkg_config)
    merged = merge_config(merged, config)
    return merged


# ---- Fonts: id -> (size, weight, family) ----
WEIGHTS = {"bold": 700, "semibold": 600, "medium": 500, "regular": 400}


def font_map(config):
    fonts = {}
    for f in config.get("font", []):
        fid = f.get("id", "")
        size = f.get("size", 24)
        file_ = str(f.get("file", "")).lower()
        family = "Material Design Icons" if "materialdesignicons" in file_ else "Rajdhani"
        weight = 400
        for key, w in WEIGHTS.items():
            if key in fid.lower() or key in file_:
                weight = w
        fonts[fid] = (size, weight, family)
    return fonts


# Real MDI glyphs by widget ID. Runtime code sets them through
# thermostat_helpers.h, so the preview needs hints.
ICON_HINTS = [
    (re.compile(r"hum"), "\U000F058E"),            # mdi-water-percent
    (re.compile(r"thermo_current"), "\U000F050F"),  # mdi-thermometer
    (re.compile(r"mode"), "\U000F0238"),            # mdi-fire
    (re.compile(r"menu_icon"), "\U000F0393"),       # mdi-thermostat
]


def label_text(widget):
    wid = widget.get("id", "")
    txt = str(widget.get("text", ""))
    if txt.startswith("<"):  # !lambda
        txt = "(lambda)"
    if "icon" in wid:
        for rx, repl in ICON_HINTS:
            if rx.search(wid):
                return repl
        return "\U000F02D6"  # mdi-help-circle
    return txt if txt else "(empty)"


def color_css(value, default="#FFFFFF"):
    if value is None:
        return default
    s = str(value)
    if s.startswith("0x"):
        return "#" + s[2:].zfill(6)
    if re.fullmatch(r"[0-9A-Fa-f]{6}", s):
        return "#" + s
    if s.isdigit():
        return f"#{int(s):06X}"
    return default


def pos_css(widget, size):
    """align + x/y -> CSS position in the square preview container."""
    align = str(widget.get("align", "TOP_LEFT")).upper()
    x = int(widget.get("x", 0) or 0)
    y = int(widget.get("y", 0) or 0)
    css, tx, ty = [], "0", "0"
    # Horizontal position.
    if "MID" in align or align == "CENTER":
        css.append(f"left:{size // 2 + x}px")
        tx = "-50%"
    elif "RIGHT" in align:
        css.append(f"right:{-x}px")
    else:
        css.append(f"left:{x}px")
    # Vertical position.
    if align.startswith("TOP"):
        css.append(f"top:{y}px")
    elif align.startswith("BOTTOM"):
        css.append(f"bottom:{-y}px")
    else:  # CENTER / LEFT_MID / RIGHT_MID
        css.append(f"top:{size // 2 + y}px")
        ty = "-50%"
    css.append(f"transform:translate({tx},{ty})")
    return ";".join(css)


def render_widget(widget_entry, fonts, size):
    (wtype, w), = widget_entry.items()
    wid = w.get("id", "?")
    hidden = w.get("hidden", False)
    hid_css = "opacity:.35;outline:1px dashed #666;" if hidden else ""
    title = f"{wid} ({wtype})" + (" [hidden]" if hidden else "")

    if wtype == "label":
        fsize, weight, family = fonts.get(str(w.get("text_font", "")), (24, 400, "Rajdhani"))
        color = color_css(w.get("text_color"))
        extra = ""
        if w.get("width"):
            ta = str(w.get("text_align", "CENTER")).lower()
            extra = f"width:{int(w['width'])}px;text-align:{ta};"
        return (
            f'<div class="w" title="{title}" style="{pos_css(w, size)};'
            f"font-family:'{family}';font-size:{fsize}px;font-weight:{weight};"
            f'color:{color};{extra}{hid_css}">'
            f"{label_text(w)}</div>"
        )
    if wtype == "arc":
        d = int(w.get("width", 200))
        indicator = w.get("indicator") or {}
        aw = int(indicator.get("arc_width", w.get("arc_width", 10)))
        color = color_css(indicator.get("arc_color"), "#FF5A2D")
        bg = color_css(w.get("arc_color"), "#2A2A2A")
        bg_opacity = 0 if str(w.get("arc_opa", "COVER")).upper() == "TRANSP" else 1
        indicator_opacity = 0 if str(indicator.get("arc_opa", "COVER")).upper() == "TRANSP" else 1
        start = float(w.get("start_angle", 135)) + float(w.get("rotation", 0))
        end = float(w.get("end_angle", 45)) + float(w.get("rotation", 0))
        sweep = (end - start) % 360
        if sweep == 0:
            sweep = 360
        minimum = float(w.get("min_value", 0))
        maximum = float(w.get("max_value", 100))
        value = min(max(float(w.get("value", minimum)), minimum), maximum)
        progress = sweep * ((value - minimum) / (maximum - minimum)) if maximum != minimum else 0
        center = d / 2
        radius = (d - aw) / 2

        def point(angle):
            rad = math.radians(angle)
            return center + radius * math.cos(rad), center + radius * math.sin(rad)

        def arc_path(angle, angle_sweep):
            x1, y1 = point(angle)
            x2, y2 = point(angle + angle_sweep)
            large = 1 if angle_sweep > 180 else 0
            return f"M {x1:.2f} {y1:.2f} A {radius:.2f} {radius:.2f} 0 {large} 1 {x2:.2f} {y2:.2f}"

        reverse = str(w.get("mode", "NORMAL")).upper() == "REVERSE"
        indicator_start = start + progress if reverse else start
        indicator_sweep = sweep - progress if reverse else progress
        knob = w.get("knob") or {}
        knob_html = ""
        if w.get("adjustable", False):
            kx, ky = point(start + progress)
            kr = aw / 2 + int(knob.get("pad_all", 0) or 0)
            kfill = color_css(knob.get("bg_color"), "#F7FBFD")
            kstroke = color_css(knob.get("border_color"), "#159DDD")
            kbw = int(knob.get("border_width", 0) or 0)
            # SVG strokes are centered on their path. Reduce the circle radius
            # by half the stroke so the knob border grows inward only, matching
            # LVGL's border geometry instead of enlarging the marker.
            stroke_radius = max(0.0, kr - kbw / 2)
            knob_html = (
                f'<circle cx="{kx:.2f}" cy="{ky:.2f}" r="{stroke_radius:.2f}" '
                f'fill="{kfill}" stroke="{kstroke}" stroke-width="{kbw}"/>'
            )
        return (
            f'<div class="w" title="{title}" style="{pos_css(w, size)};width:{d}px;height:{d}px;{hid_css}">'
            f'<svg width="{d}" height="{d}" viewBox="0 0 {d} {d}">'
            f'<path d="{arc_path(start, sweep)}" fill="none" stroke="{bg}" '
            f'stroke-width="{aw}" stroke-linecap="round" opacity="{bg_opacity}"/>'
            f'<path d="{arc_path(indicator_start, indicator_sweep)}" fill="none" stroke="{color}" '
            f'stroke-width="{aw}" stroke-linecap="round" opacity="{indicator_opacity}"/>{knob_html}</svg></div>'
        )
    if wtype == "obj":
        wd, ht = int(w.get("width", 10)), int(w.get("height", 10))
        bg = color_css(w.get("bg_color"), "#3A3A3A")
        return (
            f'<div class="w" title="{title}" style="{pos_css(w, size)};width:{wd}px;'
            f'height:{ht}px;background:{bg};{hid_css}"></div>'
        )
    return ""  # Skip transparent buttons and unsupported widgets.


def render_board(name, path, size):
    config = load_board_config(path)
    fonts = font_map(config)
    pages = config.get("lvgl", {}).get("pages", [])

    tiles = []
    for page in pages:
        pid = page.get("id", "?")
        widgets = "".join(render_widget(we, fonts, size) for we in page.get("widgets", []))
        tiles.append(
            f'<div class="page"><h2>{pid}</h2>'
            f'<div class="screen" style="width:{size}px;height:{size}px">{widgets}</div></div>'
        )

    return (
        f'<section class="board">'
        f'<h1>{name} &ndash; {path.name}</h1>'
        f'<div class="board-row">{"".join(tiles)}</div>'
        f'</section>'
    )


def generate():
    board_html = "".join(render_board(name, path, size) for name, path, size in BOARDS)

    version = str(time.time_ns())
    VERSION_FILE.write_text(version, encoding="utf-8")
    OUT_FILE.write_text(
        f"""<!doctype html><html><head><meta charset="utf-8">
<title>LVGL Preview - thermostat_480 / thermostat_240</title>
<style>
 @font-face{{font-family:'Rajdhani';font-weight:700;
   src:url('../assets/fonts/Rajdhani-Bold.ttf') format('truetype')}}
 @font-face{{font-family:'Rajdhani';font-weight:600;
   src:url('../assets/fonts/Rajdhani-SemiBold.ttf') format('truetype')}}
 @font-face{{font-family:'Rajdhani';font-weight:500;
   src:url('../assets/fonts/Rajdhani-Medium.ttf') format('truetype')}}
 @font-face{{font-family:'Rajdhani';font-weight:400;
   src:url('../assets/fonts/Rajdhani-Regular.ttf') format('truetype')}}
 @font-face{{font-family:'Material Design Icons';
   src:url('https://cdn.jsdelivr.net/npm/@mdi/font@7.4.47/fonts/materialdesignicons-webfont.woff2') format('woff2')}}
 body{{background:#1b1b1b;color:#ddd;font-family:'Rajdhani','Segoe UI',sans-serif;
      padding:24px 390px 24px 24px}}
 h1{{font-size:18px;font-weight:600;color:#ccc;margin:0 0 12px}}
 h2{{font-size:16px;font-weight:500;color:#9a9a9a;text-align:center}}
 .board{{margin-bottom:36px}}
 .board-row{{display:flex;gap:40px;flex-wrap:wrap}}
 .screen{{position:relative;background:#000;
      border-radius:50%;overflow:hidden;box-shadow:0 0 0 6px #333}}
 .w{{position:absolute;white-space:nowrap;line-height:1}}
 .w:hover{{outline:1px solid #0f0}}
 #status{{position:fixed;right:12px;top:8px;font-size:13px;color:#6a6}}
 #mdi-panel{{position:fixed;z-index:10;top:0;right:0;width:350px;height:100vh;
      box-sizing:border-box;display:flex;flex-direction:column;background:#242424;
      border-left:1px solid #444;box-shadow:-8px 0 24px #0006;padding:16px}}
 #mdi-panel h2{{margin:0 0 10px;text-align:left;color:#eee;font-size:20px}}
 #mdi-search{{box-sizing:border-box;width:100%;padding:10px 12px;border:1px solid #555;
      border-radius:7px;background:#181818;color:#fff;font:500 16px 'Rajdhani','Segoe UI',sans-serif;
      outline:none}}
 #mdi-search:focus{{border-color:#55BCEB;box-shadow:0 0 0 2px #55BCEB33}}
 #mdi-count{{min-height:20px;margin:8px 2px;color:#999;font-size:13px}}
 #mdi-results{{min-height:0;overflow-y:auto;padding-right:4px}}
 #mdi-grid{{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:7px}}
 .mdi-card{{min-width:0;min-height:82px;display:flex;flex-direction:column;align-items:center;
      justify-content:center;gap:6px;padding:8px 5px;border:1px solid #3b3b3b;border-radius:7px;
      background:#2d2d2d;color:#ddd;font:500 12px 'Rajdhani','Segoe UI',sans-serif;cursor:default}}
 .mdi-card:hover{{border-color:#55BCEB;background:#333}}
 .mdi-glyph{{font:32px/1 'Material Design Icons';color:#f3f3f3}}
 .mdi-name{{width:100%;overflow:hidden;text-align:center;text-overflow:ellipsis;white-space:nowrap}}
 #mdi-sentinel{{height:2px}}
</style></head><body>
<aside id="mdi-panel" aria-label="Material Design Icons">
  <h2>MDI Icons</h2>
  <input id="mdi-search" type="search" placeholder="Icons durchsuchen..." autocomplete="off">
  <div id="mdi-count">Icon-Bibliothek wird geladen...</div>
  <div id="mdi-results"><div id="mdi-grid"></div><div id="mdi-sentinel"></div></div>
</aside>
<div id="status">live \N{BLACK CIRCLE}</div>
{board_html}
<script>
const VERSION = "{version}";
const MDI_META_URL = "https://cdn.jsdelivr.net/npm/@mdi/svg@7.4.47/meta.json";
const MDI_BATCH_SIZE = 160;
let mdiIcons = [];
let mdiFiltered = [];
let mdiRendered = 0;

const mdiSearch = document.getElementById("mdi-search");
const mdiCount = document.getElementById("mdi-count");
const mdiGrid = document.getElementById("mdi-grid");
const mdiResults = document.getElementById("mdi-results");
const mdiSentinel = document.getElementById("mdi-sentinel");

function updateMdiCount() {{
  const total = mdiFiltered.length;
  mdiCount.textContent = `${{total.toLocaleString("de-DE")}} Icons` +
      (mdiRendered < total ? ` · ${{mdiRendered.toLocaleString("de-DE")}} angezeigt` : "");
}}

function renderMoreMdiIcons() {{
  if (mdiRendered >= mdiFiltered.length) {{
    updateMdiCount();
    return;
  }}
  const fragment = document.createDocumentFragment();
  const next = mdiFiltered.slice(mdiRendered, mdiRendered + MDI_BATCH_SIZE);
  for (const icon of next) {{
    const card = document.createElement("div");
    card.className = "mdi-card";
    card.title = `mdi:${{icon.name}} · U+${{icon.codepoint}}`;

    const glyph = document.createElement("span");
    glyph.className = "mdi-glyph";
    glyph.textContent = String.fromCodePoint(parseInt(icon.codepoint, 16));
    glyph.setAttribute("aria-hidden", "true");

    const name = document.createElement("span");
    name.className = "mdi-name";
    name.textContent = icon.name;
    card.append(glyph, name);
    fragment.append(card);
  }}
  mdiGrid.append(fragment);
  mdiRendered += next.length;
  updateMdiCount();
}}

function filterMdiIcons() {{
  const query = mdiSearch.value.trim().toLowerCase();
  mdiFiltered = query ? mdiIcons.filter(icon => icon.search.includes(query)) : mdiIcons;
  mdiRendered = 0;
  mdiGrid.replaceChildren();
  renderMoreMdiIcons();
  mdiResults.scrollTop = 0;
}}

let mdiSearchTimer;
mdiSearch.addEventListener("input", () => {{
  clearTimeout(mdiSearchTimer);
  mdiSearchTimer = setTimeout(filterMdiIcons, 100);
}});

new IntersectionObserver(entries => {{
  if (entries.some(entry => entry.isIntersecting)) renderMoreMdiIcons();
}}, {{root: mdiResults, rootMargin: "300px"}}).observe(mdiSentinel);

async function loadMdiIcons() {{
  try {{
    const response = await fetch(MDI_META_URL);
    if (!response.ok) throw new Error(`HTTP ${{response.status}}`);
    mdiIcons = (await response.json()).map(icon => ({{
      ...icon,
      search: [icon.name, ...(icon.aliases || []), ...(icon.tags || [])].join(" ").toLowerCase()
    }}));
    mdiFiltered = mdiIcons;
    renderMoreMdiIcons();
    mdiSearch.focus();
  }} catch (error) {{
    mdiCount.textContent = "Icon-Bibliothek konnte nicht geladen werden.";
    mdiCount.style.color = "#e88";
  }}
}}

async function poll() {{
  try {{
    const r = await fetch("preview_version.txt", {{cache: "no-store"}});
    const v = (await r.text()).trim();
    if (v && v !== VERSION) location.reload();
    document.getElementById("status").style.color = "#6a6";
  }} catch (e) {{
    document.getElementById("status").style.color = "#a66";
  }}
  setTimeout(poll, 700);
}}
loadMdiIcons();
poll();
</script></body></html>""",
        encoding="utf-8",
    )
    print(f"[{time.strftime('%H:%M:%S')}] preview.html updated")


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass  # Keep request logs out of the console.


def serve():
    # Serve the project directory so ../assets/fonts/* is reachable.
    handler = partial(QuietHandler, directory=str(PROJECT))
    httpd = ThreadingHTTPServer(("127.0.0.1", PORT), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return httpd


def watch():
    generate()
    serve()
    url = f"http://localhost:{PORT}/tools/preview.html"
    print(f"Watcher running: {url}  (Ctrl+C stops it)")
    webbrowser.open(url)
    last = {p: p.stat().st_mtime_ns for p in WATCH_FILES}
    try:
        while True:
            time.sleep(0.5)
            changed = False
            for p in WATCH_FILES:
                mtime = p.stat().st_mtime_ns
                if mtime != last[p]:
                    last[p] = mtime
                    changed = True
            if changed:
                time.sleep(0.2)  # The editor may still be writing.
                try:
                    generate()
                except yaml.YAMLError as e:
                    print(f"[{time.strftime('%H:%M:%S')}] YAML error: {e}")
    except KeyboardInterrupt:
        print("\nWatcher stopped.")


if __name__ == "__main__":
    if "--once" in sys.argv:
        generate()
        webbrowser.open(OUT_FILE.as_uri())
    else:
        watch()
