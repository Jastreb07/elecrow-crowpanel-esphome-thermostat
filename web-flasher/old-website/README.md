# Web Flasher

A static, [ESP Web Tools](https://esphome.github.io/esp-web-tools/)-based
flashing page for both boards, served at
`https://smart-knob.vexur-software.com`. It flashes firmware straight from
the browser (Chrome/Edge, Web Serial) and offers an Improv Wi-Fi
"configure network" step right after installing, the same way
esphome.github.io/esp-web-tools works for other devices.

The footer links to Impressum, Datenschutz, and AGB on
`vexur-software.com` (`/impressum`, `/datenschutz`, `/agb` in
`index.html`) so this legally has an operator imprint, as required for a
public-facing site under German law. Those three paths are a placeholder
guess — check they resolve on vexur-software.com and adjust the `<a href>`
values in `index.html` if the real paths differ.

This directory is plain static HTML/JS (`esp-web-tools` is loaded from a
CDN) — no build step of its own. `manifest_240.json` and `manifest_480.json`
point their flashable parts at `raw.githubusercontent.com` URLs, so the
firmware bytes are always pulled live from this GitHub repo regardless of
where `index.html` itself is hosted. Nothing under `web-flasher/firmware/`
needs to be uploaded anywhere separately — pushing it to `master` is the
whole deployment step for firmware.

`web-flasher/images/` holds the two board product photos shown on the
cards (copies of `images/Elecrow-CrowPanel-240x240.png` and
`-480x480.png` from the repo root) so the page is self-contained — both the
local preview and the deployed site load them from `images/` relative to
`index.html`, with no dependency on GitHub raw URLs for these.

## 1. Build and export firmware

```powershell
.\build.ps1
```

This compiles both boards (`esphome compile thermostat_240.yaml` /
`thermostat_480.yaml`) and copies each board's `firmware.factory.bin` —
ESPHome/PlatformIO's already-merged bootloader + partition table + OTA
marker + application image, flashable in one piece starting at offset `0` —
from ESPHome's build cache into `web-flasher/firmware/thermostat_240/` and
`web-flasher/firmware/thermostat_480/`:

```text
.esphome/build/thermostat240/.pioenvs/thermostat240/firmware.factory.bin
  -> web-flasher/firmware/thermostat_240/firmware.factory.bin
.esphome/build/thermostat480/.pioenvs/thermostat480/firmware.factory.bin
  -> web-flasher/firmware/thermostat_480/firmware.factory.bin
```

Note `esphome.build_path` in the board YAMLs does **not** relocate this —
`esphome compile` always uses the default, hidden `.esphome/build/...`
cache directory regardless of that setting in the ESPHome version this
project uses.

If PlatformIO named this file differently for your ESPHome/IDF version,
`build.ps1` stops with an error naming the missing file — update the path in
`build.ps1` and the matching URL in `manifest_240.json` /
`manifest_480.json` to whatever is actually in
`.esphome/build/<device>/.pioenvs/<device>/`.

## 2. Commit and push

```bash
git add web-flasher/firmware
git commit -m "chore(web-flasher): update firmware binaries"
git push
```

The repo's `.gitignore` blocks `*.bin` everywhere except under
`web-flasher/firmware/` (see the `!web-flasher/firmware/**/*.bin`
exception). Once this lands on `master`, the manifests'
`raw.githubusercontent.com` URLs serve the new binaries immediately — no
separate publish step needed for a firmware-only update (see step 4).

## 3. Preview the page locally

Web Serial requires a secure context, so opening `index.html` directly
(`file://`) will not work. Serve it over plain HTTP on localhost instead:

```powershell
cd web-flasher
python -m http.server 8080
```

Open `http://localhost:8080` in Chrome or Edge. It fetches firmware from
GitHub even when previewed locally, so this only reflects binaries you've
already pushed to `master`.

## 4. Publish the page

`https://smart-knob.vexur-software.com` is hosted on our own web space, not
GitHub Pages. Because firmware comes straight from GitHub (see above),
publishing is just copying static files — no build step, no GitHub Pages
setup, no DNS changes tied to GitHub:

1. Upload `index.html`, `manifest_240.json`, `manifest_480.json`, and the
   `images/` folder from this directory to the web space (FTP, your
   existing deploy pipeline, whatever you already use for
   vexur-software.com).
2. Do **not** upload `web-flasher/firmware/` — the manifests fetch those
   binaries from `raw.githubusercontent.com` regardless of where
   `index.html` lives, so there's nothing to sync there.
3. Repeat step 1 whenever `index.html` or a manifest changes. A
   firmware-only update (new binaries pushed to `master`) needs no
   re-upload at all — the page picks it up automatically on next load.
