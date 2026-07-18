"""Generates the Smart Thermostat Knob logo/icon as PNGs.

Draws a stylized rotary knob with a setpoint-ring arc, matching the
on-device LVGL UI's color scheme (dark purple background, bright purple
accent, white indicator). Rendered at high resolution with supersampling
for clean anti-aliasing, then downsampled to the target sizes.

Usage: python tools/generate_logo.py
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw

BG = (36, 27, 61, 255)  # 0x241B3D
ACCENT = (110, 79, 189, 255)  # 0x6E4FBD
ACCENT_BRIGHT = (156, 127, 240, 255)  # 0x9C7FF0
WHITE = (240, 237, 251, 255)  # 0xF0EDFB
TRACK = (74, 52, 128, 255)  # 0x4A3480, unlit ring track

SUPERSAMPLE = 4


def draw_logo(size: int, *, transparent_bg: bool) -> Image.Image:
    ss = size * SUPERSAMPLE
    bg = (0, 0, 0, 0) if transparent_bg else BG
    img = Image.new("RGBA", (ss, ss), bg)
    draw = ImageDraw.Draw(img)

    cx = cy = ss / 2
    outer_r = ss * 0.46
    ring_r = ss * 0.40
    ring_width = ss * 0.07
    knob_r = ss * 0.27

    if not transparent_bg:
        draw.ellipse(
            [cx - outer_r, cy - outer_r, cx + outer_r, cy + outer_r],
            fill=BG,
        )

    # Setpoint-ring track (unlit) - a near-full circle, gap at the bottom
    # like the on-device thermostat ring.
    ring_bbox = [cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r]
    draw.arc(ring_bbox, start=125, end=55, fill=TRACK, width=int(ring_width))

    # Lit portion of the ring (progress arc), bright purple.
    draw.arc(ring_bbox, start=125, end=280, fill=ACCENT_BRIGHT, width=int(ring_width))

    # Central knob body.
    draw.ellipse(
        [cx - knob_r, cy - knob_r, cx + knob_r, cy + knob_r],
        fill=ACCENT,
    )
    # Subtle highlight for a bit of dimensionality.
    hl_r = knob_r * 0.92
    draw.ellipse(
        [cx - hl_r, cy - hl_r * 1.02, cx + hl_r, cy + hl_r * 0.7],
        fill=None,
        outline=ACCENT_BRIGHT,
        width=int(ss * 0.006),
    )

    # Indicator notch pointing at the current angle (~280deg, matching the
    # lit ring's end) - like the physical encoder's position marker.
    angle_deg = 280
    angle = math.radians(angle_deg)
    notch_len = knob_r * 0.55
    notch_w = ss * 0.025
    nx = cx + math.cos(angle) * (knob_r - notch_len * 0.15)
    ny = cy + math.sin(angle) * (knob_r - notch_len * 0.15)
    perp = angle + math.pi / 2
    dx, dy = math.cos(perp) * notch_w, math.sin(perp) * notch_w
    tip_x = cx + math.cos(angle) * (knob_r - notch_len)
    tip_y = cy + math.sin(angle) * (knob_r - notch_len)
    draw.polygon(
        [
            (nx + dx, ny + dy),
            (nx - dx, ny - dy),
            (tip_x, tip_y),
        ],
        fill=WHITE,
    )

    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    targets = [
        (root / "custom_components" / "smart_thermostat_knob" / "logo.png", 256, False),
        (root / "custom_components" / "smart_thermostat_knob" / "logo@2x.png", 512, False),
        (root / "custom_components" / "smart_thermostat_knob" / "icon.png", 256, True),
        (root / "custom_components" / "smart_thermostat_knob" / "icon@2x.png", 512, True),
        (root / "web-flasher" / "images" / "logo.png", 512, False),
        (root / "web-flasher" / "images" / "favicon.png", 64, False),
    ]
    for path, size, transparent in targets:
        path.parent.mkdir(parents=True, exist_ok=True)
        draw_logo(size, transparent_bg=transparent).save(path)
        print(f"wrote {path} ({size}x{size}, transparent={transparent})")


if __name__ == "__main__":
    main()
