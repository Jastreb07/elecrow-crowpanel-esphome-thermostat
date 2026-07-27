# GitHub Releases statt raw.githubusercontent.com — Plan

Status: **Entwurf, offene Punkte geklärt, wartet auf finale Freigabe zur
Umsetzung.**

## Ziel (aus der Anforderung)

1. Web-Flasher (`web-flasher/docs/flash.md`) und OTA-Update
   (`thermostat_common.yaml`'s `update: source:`) sollen ihre Firmware/
   Manifeste nicht mehr von `raw.githubusercontent.com/.../master/firmware/...`
   beziehen, sondern von einem **GitHub Release**.
2. `build.ps1` wird **nicht mehr manuell ausgeführt**, um zu releasen.
   Stattdessen: **Merge nach `master` → automatischer GitHub-Actions-
   Workflow**, der selbst baut, die OTA-Datei erzeugt und ein Release
   veröffentlicht.
3. **Beide** Manifeste (`manifest.json` für den Browser-Flasher,
   `ota-manifest.json` für den Geräte-Updater) müssen den MD5-Standard
   prüfen/bereitstellen. Der MD5 wird vom Workflow selbst erzeugt (nicht
   mehr lokal per `build.ps1`).

## Verifizierte Fakten (nicht geraten - direkt recherchiert)

1. **esp-web-tools-Manifest hat kein MD5/Checksum-Feld.** Direkt aus der
   offiziellen Doku geprüft: `name`, `version`, `home_assistant_domain`,
   `funding_url`, `new_install_prompt_erase`,
   `new_install_improv_wait_time`, `builds[].chipFamily`, `builds[].parts[]
   .{path,offset}`, `builds[].improv` - **keine** Prüfsumme irgendwo im
   Schema dokumentiert. Der MD5-Wunsch für `manifest.json` kann also nicht
   heißen "ins Manifest-Schema einbauen" (das unterstützt esp-web-tools
   schlicht nicht) - stattdessen: **eine separate Checksums-Datei** als
   zusätzliches Release-Asset (siehe unten), die jeder manuell/extern
   prüfen kann, auch wenn esp-web-tools selbst sie nicht konsumiert.
2. **`ota-manifest.json` (ESPHomes `update: platform: http_request`) hat
   bereits ein Pflicht-MD5-Feld** (`builds[].ota.md5`, siehe
   `docs/OTA_UPDATE_PLAN.md` - dort schon direkt aus
   `http_request_update.cpp` verifiziert) - das wird tatsächlich vom ESP32
   beim Flashen geprüft, nicht nur informativ.
3. **`http_request:`-Komponente folgt Redirects standardmäßig**
   (`follow_redirects: true`, `redirect_limit: 3`, direkt aus
   `esphome/components/http_request/__init__.py` der installierten
   ESPHome-Version gelesen). Das ist die Voraussetzung dafür, dass GitHubs
   stabile `.../releases/latest/download/<asset>`-URL (ein 302-Redirect
   zur eigentlichen CDN-Datei) sowohl vom Browser (`esp-web-tools`, nutzt
   `fetch()`, folgt Redirects ebenfalls per Default) als auch vom ESP32
   selbst klaglos funktioniert.
4. **Offizielle GitHub Action `esphome/build-action` existiert** (nicht
   selbstgebaut) und erzeugt pro Board-YAML: `{name}.factory.bin`,
   `{name}.ota.bin`, `{name}.elf`, sowie ein `manifest.json`
   (esp-web-tools-Format, wahlweise `complete-manifest: true/false`).
   Erzeugt aber **keine Checksums** - das braucht einen eigenen
   Workflow-Schritt. Inputs u.a. `yaml-file`, `version` (ESPHome-Version,
   Default `latest`), `release-summary`, `release-url`,
   `complete-manifest`, `substitutions`. Benötigt ESPHome **2026.7.0**
   oder neuer, Action-Release `v7.0.0` - das lokal installierte ESPHome
   wurde deshalb bereits von 2026.6.5 auf **2026.7.2** aktualisiert
   (`pip install --upgrade esphome`), beide Boards validieren mit der
   neuen Version weiterhin sauber. CI installiert ihre eigene
   ESPHome-Version unabhängig davon - die lokale Aktualisierung war rein
   dafür, dass lokales Testen/Validieren und die Action-Anforderung nicht
   auseinanderlaufen.
   Quelle: [esphome/build-action README](https://github.com/esphome/build-action/blob/main/README.md).

## Entscheidung: stabile "latest"-URLs statt versionierter Pfade

Das Gerät selbst (die kompilierte `update: source:`-URL in
`thermostat_common.yaml`) und `flash.md`'s Buttons dürfen **niemals** eine
versionierte URL enthalten (z.B. `.../download/v1.2.0/manifest.json`) -
sie müssten sonst bei jedem Release neu kompiliert/deployed werden, was
den ganzen Sinn eines Auto-Updates zunichtemacht. Stattdessen:

```
https://github.com/<owner>/<repo>/releases/latest/download/<asset-name>
```

GitHub löst das automatisch per Redirect auf das neueste Release auf -
funktioniert ohne API-Aufruf, ohne Versions-Tracking im Code, und wurde
oben unter Punkt 3 als mit ESPHomes `http_request:` kompatibel bestätigt.

## Asset-Namensschema (pro Release, 4 Dateien je Board + 1 gemeinsame)

GitHub-Release-Assets müssen innerhalb eines Release eindeutige Namen
haben (beide Boards landen im selben Release) - daher Board-Präfix:

- `thermostat_240-firmware.factory.bin` / `thermostat_480-firmware.factory.bin`
  (esp-web-tools, wie bisher)
- `thermostat_240-firmware.ota.bin` / `thermostat_480-firmware.ota.bin`
  (ESPHome-Self-Update, wie bisher)
- `thermostat_240-manifest.json` / `thermostat_480-manifest.json`
- `thermostat_240-ota-manifest.json` / `thermostat_480-ota-manifest.json`
- `checksums.txt` (ein gemeinsames File mit **SHA256** für alle vier
  `.bin`-Dateien, GNU-`sha256sum`-Format - erfüllt "Prüfsumme vorhanden"
  für `manifest.json`/esp-web-tools zumindest als extern nachprüfbare
  Referenz, da es im Manifest-Schema selbst nicht geht, siehe Punkt 1
  oben. `ota-manifest.json` trägt sein eigenes, tatsächlich vom Gerät
  geprüftes MD5 bereits separat im `builds[].ota.md5`-Feld - keine
  Dopplung mit `checksums.txt` nötig.)

## Geplanter Workflow (`.github/workflows/release-firmware.yml`)

```yaml
name: Release firmware

on:
  push:
    branches: [master]

jobs:
  release:
    runs-on: ubuntu-latest
    permissions:
      contents: write  # to create the release + upload assets
    steps:
      - uses: actions/checkout@v4

      - name: Read version
        id: version
        run: echo "version=$(cat firmware/version.txt)" >> "$GITHUB_OUTPUT"

      - name: Skip if this version was already released
        id: check
        run: |
          if gh release view "v${{ steps.version.outputs.version }}" >/dev/null 2>&1; then
            echo "exists=true" >> "$GITHUB_OUTPUT"
          else
            echo "exists=false" >> "$GITHUB_OUTPUT"
          fi
        env:
          GH_TOKEN: ${{ github.token }}

      - name: Write secrets.yaml
        if: steps.check.outputs.exists == 'false'
        run: |
          cat > secrets.yaml <<EOF
          wifi_ssid: "placeholder"
          wifi_password: "placeholder"
          wifi_ap_password: "placeholder"
          ota_password: "${{ secrets.OTA_PASSWORD }}"
          EOF

      - name: Build thermostat_240
        if: steps.check.outputs.exists == 'false'
        uses: esphome/build-action@v7.0.0
        id: build-240
        with:
          yaml-file: thermostat_240.yaml
          complete-manifest: false
          release-summary: "Smart Thermostat Knob ${{ steps.version.outputs.version }}"
          release-url: "https://github.com/${{ github.repository }}/releases/tag/v${{ steps.version.outputs.version }}"

      - name: Build thermostat_480
        if: steps.check.outputs.exists == 'false'
        uses: esphome/build-action@v7.0.0
        id: build-480
        with:
          yaml-file: thermostat_480.yaml
          complete-manifest: false
          release-summary: "Smart Thermostat Knob ${{ steps.version.outputs.version }}"
          release-url: "https://github.com/${{ github.repository }}/releases/tag/v${{ steps.version.outputs.version }}"

      - name: Assemble release assets
        if: steps.check.outputs.exists == 'false'
        run: |
          # Rename with board prefix, compute SHA256 checksums, generate
          # ota-manifest.json (ESPHome's own schema, not produced by
          # build-action) with the real md5, patch manifest.json's parts[]
          # .path to the "latest/download" URL now that we know the final
          # asset filename.
          # (exact script TBD during implementation - see "Offene Punkte")
          sha256sum release-assets/*.bin > release-assets/checksums.txt

      - name: Extract release notes
        if: steps.check.outputs.exists == 'false'
        id: notes
        run: |
          # Pulls the section for this version out of CHANGELOG.md (exact
          # extraction approach - e.g. awk between "## <version>" headers -
          # fixed during implementation).

      - name: Create GitHub Release
        if: steps.check.outputs.exists == 'false'
        uses: softprops/action-gh-release@v2
        with:
          tag_name: "v${{ steps.version.outputs.version }}"
          name: "v${{ steps.version.outputs.version }}"
          body_path: release-notes.md
          files: release-assets/*
```

(Skizze - siehe "Offene Punkte" unten für Details, die während der
Umsetzung geklärt/getestet werden müssen.)

## Versionierung: `build.ps1` bleibt der interaktive Versions-Bumper

`build.ps1`s Patch/Minor/Major-Abfrage bleibt erhalten, verliert aber ihre
komplette Compile-/Export-/Manifest-Schreib-Logik - sie tut jetzt nur noch
genau eine Sache: Version fragen, `firmware/version.txt` schreiben.

- Liest die aktuelle Version aus `firmware/version.txt` (statt bisher aus
  `firmware/thermostat_240|480/manifest.json`, die es ja nicht mehr lokal
  gibt/geben muss).
- Fragt wie bisher `[1] Patch [2] Minor [3] Major`.
- Schreibt die neue Version in `firmware/version.txt` (einzige Datei,
  einzige Wahrheit - beide Boards teilen sich weiterhin eine
  Versionsnummer, wie schon beim alten Skript).
- Kein `esphome compile` mehr, kein Firmware-Export, kein
  Manifest-Schreiben - das übernimmt jetzt ausschließlich der
  GitHub-Actions-Workflow.
- Der Entwickler committet `firmware/version.txt` (per PR), der Merge nach
  `master` löst dann den Workflow aus.

Der Workflow selbst:
- liest `firmware/version.txt`,
- prüft per `gh release view` ob `v<version>` schon existiert,
- baut/released nur wenn nicht - jeder Merge nach `master`, der
  `firmware/version.txt` nicht geändert hat, ist dadurch ein no-op (kein
  Doppel-Release bei unrelated Merges).

## `secrets.yaml` in CI

`secrets.yaml` ist gitignored (nur `secrets.yaml.example` liegt im Repo).
Für den CI-Build:
- `wifi_ssid`/`wifi_password`/`wifi_ap_password`: reine Platzhalter - das
  ausgelieferte Gerät bekommt sein WLAN ohnehin erst nach dem Flashen über
  Improv Serial gesetzt (`improv_serial:`-Komponente), die
  einkompilierten Werte werden nie benutzt.
- `ota_password`: kommt aus einem **GitHub Actions Secret**
  (`secrets.OTA_PASSWORD`) statt einem öffentlichen Platzhalter - dieses
  Passwort schützt die *lokale* ESPHome-OTA-Empfangskomponente
  (`ota: platform: esphome`, fürs Debuggen im eigenen Netz), nicht den
  neuen Self-Update-Mechanismus; ein für alle Nutzer identischer, aus dem
  öffentlichen Repo ablesbarer Platzhalter wäre hier keine echte Hürde.

## Änderungen an bestehenden Dateien

- **`web-flasher/docs/flash.md`**: beide `esp-web-install-button
  manifest="..."` auf
  `https://github.com/Jastreb07/elecrow-crowpanel-esphome-thermostat/releases/latest/download/thermostat_240-manifest.json`
  (bzw. `_480-`) ändern.
- **`thermostat_common.yaml`**: `update: - platform: http_request source:`
  auf
  `https://github.com/Jastreb07/elecrow-crowpanel-esphome-thermostat/releases/latest/download/${release_asset_board}-ota-manifest.json`
  ändern. Neue Substitution `release_asset_board` je Board (`thermostat_240`
  in `thermostat_240.yaml`, `thermostat_480` in `thermostat_480.yaml`) -
  `${device_name}` bleibt unverändert (`thermostat240`/`thermostat480`,
  ohne Unterstrich, wird an vielen anderen Stellen schon verwendet), die
  neue Substitution ist ausschließlich für die Release-Asset-Namen (mit
  Unterstrich, konsistent zum bisherigen `firmware/thermostat_240/`-
  Verzeichnisnamen).
- **`firmware/thermostat_240/`, `firmware/thermostat_480/`**: komplett aus
  dem Repo entfernen (inkl. `.bin`/`manifest.json`/`ota-manifest.json`) -
  die Binaries leben jetzt ausschließlich als Release-Assets, nicht mehr
  im Git-Verlauf (verhindert, dass jedes Release die Repo-Größe dauerhaft
  aufbläht). Das `firmware/`-Verzeichnis selbst bleibt bestehen - es
  enthält danach nur noch `version.txt`.
- **`build.ps1`**: auf einen reinen Versions-Bumper reduziert - siehe
  "Versionierung" oben. Kein `esphome compile`, kein Firmware-Export, kein
  Manifest-Schreiben mehr darin.
- **Neu**: `firmware/version.txt` (initial die aktuell schon in den
  Manifesten stehende Version, z.B. `1.0.0`).
- **Neu**: `CHANGELOG.md` (Repo-Root, manuell gepflegt - Quelle für die
  Release-Notizen, siehe unten).
- **Neu**: `.github/workflows/release-firmware.yml`.

## Entschiedene Punkte (vorher offen)

- **Asset-Namensschema**: neue Substitution `release_asset_board` pro
  Board (siehe "Änderungen an bestehenden Dateien" oben), Assets behalten
  den Unterstrich (`thermostat_240-...`).
- **ESPHome-Version**: lokal auf 2026.7.2 aktualisiert, erfüllt die
  Anforderung von `esphome/build-action@v7.0.0` (ESPHome 2026.7.0+).
- **Checksums**: nur SHA256 (`checksums.txt`, GNU-`sha256sum`-Format),
  kein MD5 darin - `ota-manifest.json`s eigenes MD5-Feld deckt den
  tatsächlich geprüften Anwendungsfall bereits ab.
- **`build.ps1`-Zukunft**: bleibt bestehen, reduziert auf den reinen
  Versions-Bumper (schreibt `firmware/version.txt`), siehe
  "Versionierung" oben.
- **Release-Notizen**: aus `CHANGELOG.md` extrahiert, nicht automatisch
  generiert.

## Noch offener Punkt (vor der Umsetzung zu testen)

**`complete-manifest` von `esphome/build-action`**: unklar aus der Doku,
ob `release-url`/`release-summary` allein reichen, damit das erzeugte
`manifest.json` bereits auf die finalen `releases/latest/download/...`-
Asset-URLs zeigt, oder ob wir `builds[].parts[].path` nach dem Build
selbst per `jq`/Skript nachbearbeiten müssen (mit `complete-manifest:
false` + eigenem Patch-Schritt ist das so oder so nötig, da wir den
finalen Dateinamen erst nach dem Board-Präfix-Rename kennen). Muss beim
ersten Workflow-Testlauf verifiziert werden - einziger Punkt, der sich
nicht vorab per Quellcode-Lektüre klären lässt, sondern einen echten
Testlauf braucht.

## Testing/Verifikation (vor dem ersten echten Release)

- Workflow zunächst mit `workflow_dispatch:` (manueller Trigger)
  zusätzlich zum `push: branches: [master]` ausstatten, um ihn ohne einen
  echten Merge testen zu können.
- Ersten Testlauf **gegen ein Test-Repo oder einen Draft-Release** fahren,
  bevor er scharf auf `master` steht - besonders um den verbliebenen
  offenen Punkt oben (`complete-manifest`-Verhalten) gefahrlos zu
  verifizieren.
- Nach dem ersten echten Release: manuell `esp-web-install-button` auf
  `flash.md` gegen das Release testen, und `update.check` auf einem
  Testgerät gegen das `ota-manifest.json` prüfen (zeigt es die neue
  Version an, lädt/flasht es korrekt).

## Umsetzungsreihenfolge (sobald freigegeben)

1. `firmware/version.txt` anlegen (initial die aktuelle Version).
2. `CHANGELOG.md` anlegen (initialer Eintrag für die aktuelle Version).
3. `build.ps1` auf den reinen Versions-Bumper reduzieren (liest/schreibt
   `firmware/version.txt`, kein Compile/Export/Manifest-Schreiben mehr).
4. `thermostat_240.yaml`/`thermostat_480.yaml`: neue Substitution
   `release_asset_board` ergänzen.
5. `.github/workflows/release-firmware.yml` schreiben (inkl. SHA256-
   Checksums- und ota-manifest.json-Generierungsschritt, CHANGELOG.md-
   Extraktion für die Release-Notizen).
6. `GH Actions Secret OTA_PASSWORD` im Repo anlegen (Nutzer-Aktion, ich
   kann das nicht selbst einrichten).
7. `web-flasher/docs/flash.md` + `thermostat_common.yaml`'s `update:
   source:` auf die neuen `releases/latest/download/...`-URLs umstellen.
8. `firmware/thermostat_240/`, `firmware/thermostat_480/` (Binaries +
   Manifeste) aus dem Repo entfernen - `firmware/version.txt` bleibt.
9. Workflow testweise laufen lassen (`workflow_dispatch`, siehe Testing
   oben), dann erst auf `master`-Push scharf schalten.
10. `esphome config` für beide Boards nach der URL-Umstellung erneut
    validieren.
