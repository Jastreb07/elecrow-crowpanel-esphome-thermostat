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

import colorsys
import copy
import hashlib
import os
import re
import math
import subprocess
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

# Mirrors LIGHT_COLOR_PRESETS in thermostat_helpers.h (hue, saturation).
LIGHT_COLOR_PRESETS = [(i * 20.0, 70.0) for i in range(18)]


def hsv_int(hue, saturation, value=100.0):
    r, g, b = colorsys.hsv_to_rgb(hue / 360.0, saturation / 100.0, value / 100.0)
    return (round(r * 255) << 16) | (round(g * 255) << 8) | round(b * 255)


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
    (re.compile(r"loading_config_error"), "\U000F05D6"),  # mdi-alert-circle-outline
    (re.compile(r"light_icon"), "\U000F05A8"),       # mdi-white-balance-sunny
    (re.compile(r"hum"), "\U000F058E"),            # mdi-water-percent
    (re.compile(r"thermo_current"), "\U000F050F"),  # mdi-thermometer
    (re.compile(r"mode"), "\U000F0238"),            # mdi-fire
    (re.compile(r"menu_icon"), "\U000F0393"),       # mdi-thermostat
]


def label_text(widget):
    if "_preview_text" in widget:
        return str(widget["_preview_text"])
    wid = widget.get("id", "")
    txt = str(widget.get("text", ""))
    if txt.startswith("<"):  # !lambda
        txt = "(lambda)"
    # Icon widgets with real static glyph text (e.g. the per-mode light/cover
    # icons) render as-is; only the ones left blank/lambda-driven in their
    # static definition (filled in later via a separate update action) need
    # an ICON_HINTS guess.
    if "icon" in wid and txt in ("", "(lambda)"):
        for rx, repl in ICON_HINTS:
            if rx.search(wid):
                return repl
        return "\U000F02D6"  # mdi-help-circle
    if not txt:
        return "(empty)"
    # The .w class sets white-space:nowrap, which collapses literal "\n"
    # like any other whitespace; turn multi-line label text (e.g. stacked
    # tick marks) into explicit line breaks so it renders as LVGL shows it.
    return txt.replace("\n", "<br>")


def color_css(value, default="#FFFFFF"):
    if value is None:
        return default
    s = str(value)
    if s.startswith("0x"):
        return "#" + s[2:].zfill(6)
    # PyYAML parses 0xRRGGBB as an integer. Handle decimal integers before
    # treating six-character strings as already formatted hexadecimal colors.
    if s.isdigit():
        return f"#{int(s):06X}"
    if re.fullmatch(r"[0-9A-Fa-f]{6}", s):
        return "#" + s
    return default


def pos_css(widget, size):
    """align + x/y -> CSS position in the square preview container."""
    if isinstance(size, tuple):
        container_width, container_height = size
    else:
        container_width = container_height = size
    align = str(widget.get("align", "TOP_LEFT")).upper()
    x = int(widget.get("x", 0) or 0)
    y = int(widget.get("y", 0) or 0)
    css, tx, ty = [], "0", "0"
    # Horizontal position.
    if align in {"TOP_MID", "BOTTOM_MID", "CENTER"}:
        css.append(f"left:{container_width // 2 + x}px")
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
        css.append(f"top:{container_height // 2 + y}px")
        ty = "-50%"
    angle = float(widget.get("transform_angle", 0) or 0)
    rotate = f" rotate({angle}deg)" if angle else ""
    css.append(f"transform:translate({tx},{ty}){rotate}")
    return ";".join(css)


def render_widget(widget_entry, fonts, size):
    (wtype, w), = widget_entry.items()
    wid = w.get("id", "?")
    hidden = w.get("hidden", False)
    # LVGL does not paint hidden objects at all; the preview still renders
    # them with the .w-hidden class (faded, dotted outline). The "Hidden
    # anzeigen" toggle flips a class on <body> to show/hide them globally,
    # including after a live-reload swap of #boards.
    hid_class = " w-hidden" if hidden else ""
    title = f"{wid} ({wtype})" + (" [hidden]" if hidden else "")

    if wtype == "label":
        fsize, weight, family = fonts.get(str(w.get("text_font", "")), (24, 400, "Rajdhani"))
        color = color_css(w.get("text_color"))
        extra = ""
        if w.get("width"):
            ta = str(w.get("text_align", "CENTER")).lower()
            extra = f"width:{int(w['width'])}px;text-align:{ta};"
        return (
            f'<div class="w{hid_class}" title="{title}" style="{pos_css(w, size)};'
            f"font-family:'{family}';font-size:{fsize}px;font-weight:{weight};"
            f'color:{color};{extra}">'
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
        # LVGL maps REVERSE value 0 to the right/end side of the arc.
        marker_progress = sweep - progress if reverse else progress
        indicator_start = start + marker_progress if reverse else start
        indicator_sweep = sweep - marker_progress if reverse else marker_progress
        knob = w.get("knob") or {}
        knob_html = ""
        if w.get("adjustable", False):
            kx, ky = point(start + marker_progress)
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
            f'<div class="w{hid_class}" title="{title}" style="{pos_css(w, size)};width:{d}px;height:{d}px;">'
            f'<svg width="{d}" height="{d}" viewBox="0 0 {d} {d}">'
            f'<path d="{arc_path(start, sweep)}" fill="none" stroke="{bg}" '
            f'stroke-width="{aw}" stroke-linecap="round" opacity="{bg_opacity}"/>'
            f'<path d="{arc_path(indicator_start, indicator_sweep)}" fill="none" stroke="{color}" '
            f'stroke-width="{aw}" stroke-linecap="round" opacity="{indicator_opacity}"/>{knob_html}</svg></div>'
        )
    if wtype == "obj":
        wd, ht = int(w.get("width", 10)), int(w.get("height", 10))
        bg = color_css(w.get("bg_color"), "#3A3A3A")
        grad = w.get("bg_grad_color")
        if grad is not None:
            grad_color = color_css(grad, bg)
            direction = str(w.get("bg_grad_dir", "HOR")).upper()
            axis = "to bottom" if direction in {"VER", "VERTICAL"} else "to right"
            bg = f"linear-gradient({axis},{bg},{grad_color})"
        border_width = int(w.get("border_width", 0) or 0)
        border_color = color_css(w.get("border_color"), "#FFFFFF")
        radius = w.get("radius", 0)
        radius_css = str(radius) if str(radius).endswith("%") else f"{int(radius)}px"
        children = "".join(
            render_widget(child, fonts, (wd, ht)) for child in w.get("widgets", [])
        )
        overflow = "overflow:hidden;" if w.get("clip_corner", False) else ""
        return (
            f'<div class="w{hid_class}" title="{title}" style="{pos_css(w, size)};width:{wd}px;'
            f'height:{ht}px;background:{bg};border:{border_width}px solid {border_color};'
            f'box-sizing:border-box;border-radius:{radius_css};{overflow}">{children}</div>'
        )
    if wtype == "spinner":
        diameter = int(w.get("width", w.get("height", 80)))
        arc_width = int(w.get("arc_width", 6) or 6)
        color = color_css(w.get("arc_color"), "#FFFFFF")
        return (
            f'<div class="w preview-spinner{hid_class}" title="{title}" style="{pos_css(w, size)};'
            f'width:{diameter}px;height:{diameter}px;border:{arc_width}px solid #FFFFFF22;'
            f'border-top-color:{color};border-radius:50%;box-sizing:border-box"></div>'
        )
    if wtype == "bar":
        wd, ht = int(w.get("width", 100)), int(w.get("height", 10))
        minimum = float(w.get("min_value", 0))
        maximum = float(w.get("max_value", 100))
        value = min(max(float(w.get("value", minimum)), minimum), maximum)
        progress = 100 * (value - minimum) / (maximum - minimum) if maximum != minimum else 0
        bg = color_css(w.get("bg_color"), "#292D31")
        indicator = w.get("indicator") or {}
        fill = color_css(indicator.get("bg_color"), "#FFFFFF")
        radius = w.get("radius", 0)
        radius_css = "9999px" if str(radius).endswith("%") else f"{int(radius or 0)}px"
        return (
            f'<div class="w preview-bar{hid_class}" title="{title}" style="{pos_css(w, size)};'
            f'width:{wd}px;height:{ht}px;background:{bg};border-radius:{radius_css};'
            f'overflow:hidden;"><div style="width:{progress:.1f}%;height:100%;'
            f'background:{fill};border-radius:inherit;"></div></div>'
        )
    if wtype == "button":
        wd, ht = int(w.get("width", 100)), int(w.get("height", 50))
        bg_opa = str(w.get("bg_opa", "COVER")).upper()
        bg = "transparent" if bg_opa == "TRANSP" else color_css(w.get("bg_color"), "#333333")
        border_width = int(w.get("border_width", 0) or 0)
        border_color = color_css(w.get("border_color"), "#FFFFFF")
        radius = w.get("radius", 0)
        radius_css = "9999px" if str(radius).upper() == "FULL" else f"{int(radius or 0)}px"
        children = "".join(
            render_widget(child, fonts, (wd, ht)) for child in w.get("widgets", [])
        )

        # Full-screen transparent buttons are input catchers, not visible UI.
        if bg == "transparent" and border_width == 0 and not children:
            return ""

        return (
            f'<div class="w preview-button{hid_class}" title="{title}" style="{pos_css(w, size)};'
            f'width:{wd}px;height:{ht}px;background:{bg};border:{border_width}px solid {border_color};'
            f'border-radius:{radius_css};box-sizing:border-box;">{children}</div>'
        )
    return ""  # Skip unsupported widgets.


def render_board(name, path, size):
    config = load_board_config(path)
    fonts = font_map(config)
    pages = config.get("lvgl", {}).get("pages", [])

    page_by_id = {page.get("id"): page for page in pages}
    light_source = page_by_id.get("screen_light_brightness", {}).get("widgets", [])

    def widget_configs(entries):
        for entry in entries:
            if not isinstance(entry, dict) or not entry:
                continue
            cfg = next(iter(entry.values()))
            if not isinstance(cfg, dict):
                continue
            yield cfg
            yield from widget_configs(cfg.get("widgets", []))

    def runtime_widgets(pid, original):
        if pid not in {"screen_light_brightness", "screen_light_temperature", "screen_light_color",
                       "screen_navigation"}:
            return original

        if pid == "screen_navigation":
            widgets = copy.deepcopy(original)
            text = {
                "label_settings_title": "Entities",
                "label_menu_prev2": "Living Room",
                "label_menu_prev": "Bedroom",
                "label_menu_current": "Light",
                "label_menu_next": "Kitchen Light",
                "label_menu_next2": "Settings",
                "label_menu_value": "3 / 6",
            }
            for cfg in widget_configs(widgets):
                if cfg.get("id") in text:
                    cfg["text"] = text[cfg["id"]]
            return widgets

        widgets = copy.deepcopy(light_source)
        brightness = pid == "screen_light_brightness"
        temperature = pid == "screen_light_temperature"
        color = pid == "screen_light_color"
        if temperature or color:
            # Each non-brightness page owns its own statically positioned
            # icon (label_light_icon_temperature / _color), plus the color
            # page's swatch/presets.
            widgets += copy.deepcopy(original)
        show = ({"light_brightness_scale", "label_light_brightness_ticks", "light_slider_knob"}
                if brightness else
                {"light_kelvin_scale", "light_slider_knob", "label_light_kelvin_min",
                 "label_light_kelvin_max"} if temperature else
                set())
        hide = ({"light_kelvin_scale", "label_light_kelvin_min", "label_light_kelvin_max"}
                if brightness else
                {"light_brightness_scale", "label_light_brightness_ticks"} if temperature else
                {"light_brightness_scale", "label_light_brightness_ticks",
                 "light_kelvin_scale", "light_slider_knob", "label_light_kelvin_min",
                 "label_light_kelvin_max", "label_light_value",
                 "label_light_page"})
        active_dot = 0 if brightness else 1 if temperature else 2
        for cfg in widget_configs(widgets):
            wid = cfg.get("id")
            if wid in show:
                cfg["hidden"] = False
            elif wid in hide:
                cfg["hidden"] = True
            if wid == "label_light_mode":
                cfg["text"] = "Helligkeit" if brightness else "Kelvin" if temperature else "Farbe"
            elif wid == "label_light_value" and temperature:
                cfg["text"] = "3200K"
            elif str(wid).startswith("light_mode_dot_"):
                dot = int(str(wid).rsplit("_", 1)[-1])
                cfg["bg_color"] = 0xFFFFFF if dot == active_dot else 0x292B2E
            elif str(wid).startswith("light_color_preset_"):
                preset = int(str(wid).rsplit("_", 1)[-1])
                hue, sat = LIGHT_COLOR_PRESETS[preset]
                cfg["bg_color"] = hsv_int(hue, sat)
                cfg["border_width"] = 3 if preset == 1 else 0
                cfg["border_color"] = 0xFFFFFF
            elif wid == "light_color_swatch" and color:
                hue, sat = LIGHT_COLOR_PRESETS[1]
                cfg["bg_color"] = hsv_int(hue, sat)
        return widgets

    tiles = []
    for page in pages:
        pid = page.get("id", "?")
        entries = runtime_widgets(pid, page.get("widgets", []))
        widgets = "".join(render_widget(we, fonts, size) for we in entries)
        bg = color_css(page.get("bg_color"), "#000000")
        tiles.append(
            f'<div class="page"><h2>{pid}</h2>'
            f'<div class="screen" style="width:{size}px;height:{size}px;background:{bg}">'
            f'{widgets}</div></div>'
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
    html = f"""<!doctype html><html><head><meta charset="utf-8">
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
      padding:76px 390px 24px 24px}}
 #page-header{{position:fixed;z-index:30;top:0;left:0;right:0;height:52px;
      box-sizing:border-box;display:flex;align-items:center;justify-content:space-between;
      gap:16px;padding:0 20px;background:#202020;border-bottom:1px solid #3a3a3a;
      box-shadow:0 2px 12px #0006}}
 #page-header h1{{font-size:17px;font-weight:600;color:#eee;margin:0;white-space:nowrap}}
 #page-header-right{{display:flex;align-items:center;gap:20px;font-size:13px}}
 h1{{font-size:18px;font-weight:600;color:#ccc;margin:0 0 12px}}
 h2{{font-size:16px;font-weight:500;color:#9a9a9a;text-align:center}}
 .board{{margin-bottom:36px}}
 .board-row{{display:flex;gap:40px;flex-wrap:wrap}}
 .screen{{position:relative;background:#000;
      border-radius:50%;overflow:hidden;box-shadow:0 0 0 6px #333}}
 .w{{position:absolute;white-space:nowrap;line-height:1}}
 .w:hover{{outline:1px solid #0f0}}
 .w-hidden{{opacity:.35;outline:1px dotted #888;outline-offset:-1px}}
 body.hide-hidden .w-hidden{{display:none}}
 @keyframes preview-spin{{to{{transform:translate(-50%,-50%) rotate(360deg)}}}}
 .preview-spinner{{animation:preview-spin .9s linear infinite}}
 #status{{color:#6a6}}
 #hidden-toggle{{color:#aaa;display:flex;align-items:center;gap:6px;
      user-select:none;cursor:pointer}}
 #mdi-panel{{position:fixed;z-index:10;top:52px;right:0;width:350px;height:calc(100vh - 52px);
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
<header id="page-header">
  <h1>LVGL Preview &ndash; thermostat_480 / thermostat_240</h1>
  <div id="page-header-right">
    <div id="status">live \N{BLACK CIRCLE}</div>
    <label id="hidden-toggle"><input id="hidden-checkbox" type="checkbox"> Hidden anzeigen</label>
  </div>
</header>
<aside id="mdi-panel" aria-label="Material Design Icons">
  <h2>MDI Icons</h2>
  <input id="mdi-search" type="search" placeholder="Icons durchsuchen..." autocomplete="off">
  <div id="mdi-count">Icon-Bibliothek wird geladen...</div>
  <div id="mdi-results"><div id="mdi-grid"></div><div id="mdi-sentinel"></div></div>
</aside>
<main id="boards">{board_html}</main>
<script>
let VERSION = "{version}";
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

async function refreshBoards(v) {{
  // Swap only the rendered screens in place instead of reloading the page.
  // This keeps scroll position, the MDI panel state and the loaded fonts.
  const r = await fetch(`preview.html?t=${{Date.now()}}`, {{cache: "no-store"}});
  const doc = new DOMParser().parseFromString(await r.text(), "text/html");
  const next = doc.getElementById("boards");
  if (!next) {{
    // Unexpected document layout (e.g. an older watcher build): hard reload.
    location.reload();
    return;
  }}
  document.getElementById("boards").replaceChildren(...next.childNodes);
  VERSION = v;
}}

async function poll() {{
  try {{
    const r = await fetch(`preview_version.txt?t=${{Date.now()}}`, {{cache: "no-store"}});
    const v = (await r.text()).trim();
    if (v && v !== VERSION) {{
      await refreshBoards(v);
    }}
    document.getElementById("status").style.color = "#6a6";
  }} catch (e) {{
    document.getElementById("status").style.color = "#a66";
  }}
  setTimeout(poll, 700);
}}

const HIDDEN_TOGGLE_KEY = "lvglPreviewShowHidden";
const hiddenCheckbox = document.getElementById("hidden-checkbox");
// Default to showing hidden widgets when no preference was saved yet.
const showHidden = localStorage.getItem(HIDDEN_TOGGLE_KEY) !== "false";
hiddenCheckbox.checked = showHidden;
document.body.classList.toggle("hide-hidden", !showHidden);
hiddenCheckbox.addEventListener("change", () => {{
  document.body.classList.toggle("hide-hidden", !hiddenCheckbox.checked);
  localStorage.setItem(HIDDEN_TOGGLE_KEY, String(hiddenCheckbox.checked));
}});

loadMdiIcons();
poll();
</script></body></html>"""

    # Never expose a half-written HTML document to the browser. Publish the
    # completed page first and the version marker last, so a detected version
    # always points to a fully available preview.
    atomic_write(OUT_FILE, html)
    atomic_write(VERSION_FILE, version)
    print(f"[{time.strftime('%H:%M:%S')}] preview.html updated")


def atomic_write(path, content):
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    try:
        temporary.write_text(content, encoding="utf-8")
        os.replace(temporary, path)
    finally:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass  # Keep request logs out of the console.

    def end_headers(self):
        # This is a development-only server. Disable caching globally so a
        # browser can never combine a new version marker with stale HTML,
        # fonts or other preview assets.
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()


def serve():
    # Serve the project directory so ../assets/fonts/* is reachable.
    handler = partial(QuietHandler, directory=str(PROJECT))
    httpd = ThreadingHTTPServer(("127.0.0.1", PORT), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return httpd


def stop_existing_watchers():
    """Stop older instances of this preview watcher before binding the port."""
    if os.name != "nt":
        return

    # Restrict termination to Python processes running lvgl_preview.py. This
    # also catches older instances started with a relative script path. The
    # current process and unrelated Python applications are never touched.
    script_path = str(Path(__file__).resolve())
    environment = os.environ.copy()
    environment["LVGL_PREVIEW_CURRENT_PID"] = str(os.getpid())
    environment["LVGL_PREVIEW_SCRIPT"] = script_path
    powershell = r"""
$currentPid = [int]$env:LVGL_PREVIEW_CURRENT_PID
$scriptPath = $env:LVGL_PREVIEW_SCRIPT
$scriptPattern = [regex]::Escape($scriptPath)
$targets = @(Get-CimInstance Win32_Process | Where-Object {
    $_.ProcessId -ne $currentPid -and
    $_.Name -match '^python(w)?\.exe$' -and
    ($_.CommandLine -match $scriptPattern -or $_.CommandLine -match 'lvgl_preview\.py')
})
foreach ($target in $targets) {
    Stop-Process -Id $target.ProcessId -Force -ErrorAction SilentlyContinue
}
foreach ($target in $targets) {
    Wait-Process -Id $target.ProcessId -Timeout 3 -ErrorAction SilentlyContinue
    Write-Output $target.ProcessId
}
"""
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", powershell],
            check=False,
            capture_output=True,
            text=True,
            timeout=8,
            env=environment,
        )
        stopped = [pid for pid in result.stdout.splitlines() if pid.strip().isdigit()]
        if stopped:
            print(f"Stopped previous preview watcher(s): {', '.join(stopped)}")
    except (OSError, subprocess.SubprocessError) as error:
        print(f"Could not check for an older preview watcher: {error}")


def watch():
    stop_existing_watchers()
    generate()
    serve()
    url = f"http://localhost:{PORT}/tools/preview.html"
    print(f"Watcher running: {url}  (Ctrl+C stops it)")
    if "--no-browser" not in sys.argv:
        webbrowser.open(url)
    observed = watch_snapshot()
    render_at = None
    try:
        while True:
            time.sleep(0.1)
            current = watch_snapshot()
            if current != observed:
                observed = current
                # Debounce safe-write/formatting sequences from the editor.
                render_at = time.monotonic() + 0.35

            if render_at is not None and time.monotonic() >= render_at:
                stable = watch_snapshot()
                if stable != observed:
                    observed = stable
                    render_at = time.monotonic() + 0.35
                    continue
                try:
                    generate()
                    render_at = None
                except (OSError, UnicodeError, yaml.YAMLError) as e:
                    # Do not consume the event. A file can be temporarily
                    # incomplete while PhpStorm replaces or formats it.
                    print(f"[{time.strftime('%H:%M:%S')}] Preview retry: {e}")
                    render_at = time.monotonic() + 0.5
    except KeyboardInterrupt:
        print("\nWatcher stopped.")


def watch_snapshot():
    """Content-aware snapshot; also survives an editor's atomic file replace."""
    snapshot = {}
    for path in WATCH_FILES:
        try:
            data = path.read_bytes()
            stat = path.stat()
            digest = hashlib.blake2b(data, digest_size=8).digest()
            snapshot[path] = (stat.st_mtime_ns, stat.st_size, digest)
        except OSError:
            snapshot[path] = None
    return snapshot


if __name__ == "__main__":
    if "--once" in sys.argv:
        generate()
        webbrowser.open(OUT_FILE.as_uri())
    else:
        watch()
