# ESPHome ESP32-S3: Signierte Firmware, Secure Boot und GitHub-Releases

> Ziel: Für das Projekt **Smart Thermostat Knob** sollen bei jedem GitHub-Release zwei Firmwaredateien erzeugt werden:
>
> - `firmware.factory.bin` für die Erstinstallation über ESP Web Tools / Web-Flasher
> - `firmware.ota.bin` für selbstständige Updates direkt am Gerät über das Display
>
> Die Firmware soll kryptografisch signiert, gegen Downgrades geschützt und zuverlässig aktualisierbar sein.

---

## 1. Empfohlene Entscheidung

Für das Smart Thermostat Knob sollte zunächst folgende Sicherheitsarchitektur eingesetzt werden:

1. **ESP-IDF als Framework verwenden**
2. **RSA-3072-signierte OTA-Firmware aktivieren**
3. **ESPHome `signed_ota_verification` verwenden**
4. **OTA-Downgrade-Schutz aktivieren**
5. **OTA-Rollback aktiviert lassen**
6. **HTTPS mit aktiver Zertifikatsprüfung verwenden**
7. **MD5 weiterhin im ESPHome-Update-Manifest verwenden**
8. **Zusätzlich SHA-256-Prüfsummen in GitHub Releases veröffentlichen**
9. **Hardware Secure Boot noch nicht im öffentlichen Web-Flasher aktivieren**
10. Hardware Secure Boot später nur für fertig provisionierte Produktionsgeräte einsetzen

Die empfohlene erste Stufe ist somit:

```text
HTTPS
  + MD5-Übertragungsprüfung
  + RSA-3072-Firmwaresignatur
  + ESPHome OTA-Downgrade-Schutz
  + ESP-IDF OTA-Rollback
```

Diese Kombination schützt bereits sehr gut gegen manipulierte oder fremde OTA-Firmware, ohne die Geräte durch irreversible eFuse-Einstellungen unnötig schwer wiederherstellbar zu machen.

---

## 2. Unterschied zwischen MD5, Signatur und Secure Boot

Diese Verfahren erfüllen unterschiedliche Aufgaben und sollten nicht miteinander verwechselt werden.

| Schutzmechanismus | Aufgabe | Sicherheitswert |
|---|---|---|
| HTTPS | Schützt die Übertragung zum Server | Wichtig |
| MD5 | Erkennt Übertragungsfehler oder eine falsche Datei | Keine echte Authentifizierung |
| SHA-256-Dateihash | Ermöglicht Release- und Downloadkontrolle | Gute Integritätsprüfung |
| Digitale Firmware-Signatur | Beweist, dass die Firmware mit eurem privaten Schlüssel signiert wurde | Sehr wichtig |
| Signed OTA Verification | Akzeptiert nur korrekt signierte OTA-Firmware | Sehr wichtig |
| OTA-Downgrade-Schutz | Verhindert Installation älterer Firmwareversionen | Wichtig |
| OTA-Rollback | Startet nach einem fehlerhaften Update wieder die vorherige Firmware | Wichtig |
| Hardware Secure Boot | Prüft Bootloader und App bei jedem Start über eFuses | Höchste Schutzstufe |
| Flash Encryption | Verschlüsselt Firmware und Daten im externen Flash | Schutz bei physischem Zugriff |

### 2.1 MD5

MD5 beantwortet nur die Frage:

> Ist die heruntergeladene `firmware.ota.bin` bytegenau dieselbe Datei, für die der MD5-Wert erstellt wurde?

MD5 beweist nicht, dass die Firmware von euch stammt. Ein Angreifer, der sowohl die Firmware als auch den MD5-Wert austauschen kann, könnte für seine Datei einen neuen MD5-Wert veröffentlichen.

### 2.2 Digitale Firmware-Signatur

Eine Firmware-Signatur beantwortet die Frage:

> Wurde diese Firmware mit dem privaten Vexur-Signaturschlüssel signiert und ist sie seitdem unverändert?

Nur der private Schlüssel kann eine gültige Signatur erzeugen. Das Gerät benötigt zum Prüfen keinen privaten Schlüssel.

Beim ESP32-S3 sollte dafür **Secure Boot V2 mit RSA-3072** verwendet werden.

### 2.3 Signed OTA Verification ohne Hardware Secure Boot

ESPHome unterstützt signierte OTA-Updates, ohne Hardware Secure Boot in den eFuses zu aktivieren.

Dabei gilt:

- Die laufende Firmware enthält einen gültigen Signaturblock.
- Die darin enthaltene öffentliche Schlüsselinformation wird für das nächste Update verwendet.
- Eine neue OTA-Firmware wird nur akzeptiert, wenn sie mit demselben privaten Schlüssel signiert wurde.
- Eine unsignierte oder mit einem anderen Schlüssel signierte Firmware wird abgelehnt.
- Der Bootloader selbst wird beim normalen Start nicht über Hardware-eFuses abgesichert.
- Ein Angreifer mit physischem Schreibzugriff auf den Flash könnte diesen Schutz theoretisch umgehen.

Diese Variante ist für euer öffentliches ESPHome-Projekt der beste erste Schritt.

### 2.4 Hardware Secure Boot V2

Bei vollständigem Hardware Secure Boot V2 passiert zusätzlich:

1. Der Hash des vertrauenswürdigen öffentlichen Schlüssels wird dauerhaft in eine eFuse geschrieben.
2. Der ROM-Bootloader prüft bei jedem Start den signierten zweiten Bootloader.
3. Der zweite Bootloader prüft bei jedem Start die signierte Anwendung.
4. OTA-Firmware wird ebenfalls gegen den in den eFuses verankerten Schlüssel geprüft.
5. Nicht signierte oder mit einem fremden Schlüssel signierte Firmware kann nicht gestartet werden.

Das ist deutlich stärker, aber die Aktivierung ist dauerhaft und teilweise irreversibel.

---

## 3. Warum Hardware Secure Boot nicht sofort für den öffentlichen Web-Flasher empfohlen wird

Das Smart Thermostat Knob wird als ESPHome-Projekt mit öffentlicher `firmware.factory.bin` bereitgestellt. Nutzer sollen das Gerät über einen Browser installieren und bei Bedarf wiederherstellen können.

Hardware Secure Boot verursacht dabei mehrere praktische Einschränkungen:

- eFuses werden dauerhaft programmiert.
- Das Gerät wird an euren Signaturschlüssel gebunden.
- Ein Verlust des privaten Schlüssels kann zukünftige Updates unmöglich machen.
- Nicht von euch signierte ESPHome-Firmware kann nicht mehr gestartet werden.
- USB-OTG-, DFU-, JTAG- und UART-Wiederherstellung können je nach Konfiguration eingeschränkt oder deaktiviert werden.
- Eine falsche Secure-Boot-Konfiguration kann Geräte dauerhaft unbrauchbar machen.
- Die Aktivierung sollte nicht über ein normales OTA-Update erfolgen.
- Produktion, Schlüsselverwaltung und Recovery-Prozess müssen vorher vollständig getestet sein.

Für ein offenes ESPHome-Projekt ist es außerdem problematisch, wenn ein Nutzer nach der Installation keine eigene Firmware mehr aufspielen kann.

### Empfehlung

Es sollten zwei klar getrennte Betriebsmodelle vorgesehen werden:

#### Community- und Web-Flasher-Version

```text
Signed OTA Verification: Ja
Hardware Secure Boot: Nein
Flash Encryption: Nein
Serielle Wiederherstellung: Möglich
Eigene Firmware des Nutzers: Weiterhin möglich
```

#### Spätere geschlossene Produktionsversion

```text
Signed OTA Verification: Ja
Hardware Secure Boot V2: Ja
Flash Encryption: Empfohlen
Serielle Wiederherstellung: Stark eingeschränkt
Eigene Firmware des Nutzers: Nur mit freigegebenem Schlüssel
Provisionierung: Kontrolliert im Werk
```

---

## 4. Soll MD5 zusätzlich zur Signatur verwendet werden?

**Ja.**

MD5 bleibt für den ESPHome-HTTP-Updateprozess sinnvoll und wird im Managed-Update-Manifest erwartet.

Die Aufgabenverteilung sollte so aussehen:

```text
MD5:
- Schnelle Prüfung der heruntergeladenen OTA-Datei
- Erkennung unvollständiger oder beschädigter Downloads
- Bestandteil des ESPHome-Update-Manifests

RSA-3072-Signatur:
- Sicherheitsprüfung
- Authentifizierung des Herausgebers
- Ablehnung fremder oder manipulierter Firmware

SHA-256:
- Veröffentlichte Prüfsumme für Nutzer und CI
- Prüfung der GitHub-Release-Assets
- Auditierbare Integrität außerhalb des ESPHome-OTA-Protokolls
```

### Wichtige Regel

MD5 darf niemals als Ersatz für die digitale Signatur betrachtet werden.

---

## 5. Zielarchitektur

```text
Git-Tag v1.2.3
       |
       v
Geschützter GitHub-Actions-Release-Workflow
       |
       +--> ESPHome-Konfiguration validieren
       |
       +--> Firmware mit Produktionsschlüssel kompilieren und signieren
       |
       +--> firmware.factory.bin
       |
       +--> firmware.ota.bin
       |
       +--> RSA-Signatur der OTA-Datei prüfen
       |
       +--> MD5 und SHA-256 erzeugen
       |
       +--> manifest.json erzeugen
       |
       +--> GitHub Release zunächst als Entwurf erstellen
       |
       +--> Release-Assets hochladen
       |
       +--> manifest.json auf GitHub Pages veröffentlichen
       |
       +--> Release veröffentlichen
```

Am Gerät:

```text
Gerät lädt manifest.json über HTTPS
       |
       +--> Version vergleichen
       |
       +--> Update auf Display anzeigen
       |
       +--> Nutzer bestätigt Update
       |
       +--> firmware.ota.bin herunterladen
       |
       +--> MD5 prüfen
       |
       +--> RSA-3072-Signatur prüfen
       |
       +--> Versions-Downgrade prüfen
       |
       +--> In inaktive OTA-Partition schreiben
       |
       +--> Neustart
       |
       +--> Firmware als erfolgreich markieren
              oder bei Bootfehler automatisch zurückrollen
```

---

## 6. Empfohlene Repository-Struktur

```text
smart-thermostat-knob/
├── .github/
│   └── workflows/
│       ├── validate-firmware.yml
│       └── release-firmware.yml
│
├── firmware/
│   ├── smart-thermostat-knob.yaml
│   ├── packages/
│   ├── components/
│   └── secrets.example.yaml
│
├── scripts/
│   ├── prepare-release.sh
│   ├── generate-manifest.py
│   └── verify-release.sh
│
├── web/
│   └── firmware/
│       └── manifest.json
│
├── .gitignore
├── CHANGELOG.md
└── README.md
```

### `.gitignore`

```gitignore
# Private Schlüssel
*.pem
*.key
*.p12
*.pfx

# ESPHome-Geheimnisse
secrets.yaml

# ESPHome-Builddateien
.esphome/
.pioenvs/
.piolibdeps/
build/

# Lokale Release-Ausgabe
dist/
release/
```

---

## 7. Schlüsselstrategie

### 7.1 Empfohlene Schlüsselaufteilung

Es sollten mindestens zwei unterschiedliche Schlüssel verwendet werden:

```text
Development Signing Key
- Nur für lokale Entwicklungsgeräte
- Darf niemals für öffentliche Produktions-Releases verwendet werden

Production Signing Key
- Nur für offizielle GitHub-Releases
- Zugriff ausschließlich über geschützten Release-Workflow
```

Für Stable- und Beta-Releases desselben Produkts kann derselbe Produktionsschlüssel verwendet werden, wenn Geräte zwischen diesen Kanälen wechseln dürfen.

Unterschiedliche Schlüssel für Stable und Beta würden einen OTA-Wechsel zwischen den Kanälen verhindern, sofern kein besonderer Schlüsselwechselprozess vorhanden ist.

### 7.2 Schlüssel erzeugen

Der Produktionsschlüssel sollte einmalig auf einem vertrauenswürdigen Offline-System erzeugt werden:

```bash
espsecure generate-signing-key \
  --version 2 \
  --scheme rsa3072 \
  secure_boot_signing_key.pem
```

Je nach installierter Tool-Version kann der Befehl auch als `espsecure.py` verfügbar sein:

```bash
espsecure.py generate-signing-key \
  --version 2 \
  --scheme rsa3072 \
  secure_boot_signing_key.pem
```

### 7.3 Sicherheitsanforderungen an den Schlüssel

- Niemals in Git committen
- Niemals als GitHub-Release-Asset veröffentlichen
- Niemals in Docker-Images einbauen
- Niemals in Build-Artefakten archivieren
- Mindestens zwei verschlüsselte Offline-Backups an getrennten Orten anlegen
- Zugriff auf sehr wenige Personen beschränken
- Eine dokumentierte Notfall- und Wiederherstellungsprozedur pflegen
- Einen eigenen Schlüssel pro Produktfamilie erwägen
- Entwicklung und Produktion strikt trennen

### 7.4 GitHub-Verwaltung

Für den ersten Produktionsaufbau kann der PEM-Schlüssel Base64-kodiert als GitHub-Environment-Secret gespeichert werden.

Beispiel:

```bash
base64 -w 0 secure_boot_signing_key.pem
```

GitHub:

```text
Environment: production-firmware
Secret: ESPHOME_SIGNING_KEY_B64
```

Das Environment sollte folgende Regeln erhalten:

- Nur geschützte Tags oder Branches
- Manuelle Freigabe für Releases
- Nur ausgewählte Maintainer
- Kein Zugriff aus Pull Requests von Forks
- Kein Zugriff aus normalen Push-Workflows

Langfristig ist ein externer Signaturdienst oder ein HSM besser als ein exportierbarer PEM-Schlüssel.

---

## 8. ESPHome-Konfiguration

Die folgende Konfiguration zeigt den empfohlenen Sicherheitsblock. Hardware-, Display- und PSRAM-Einstellungen müssen an das konkrete CrowPanel angepasst bleiben.

```yaml
substitutions:
  project_version: "1.2.3"

esphome:
  name: smart-thermostat-knob
  friendly_name: Smart Thermostat Knob

  project:
    name: vexur.smart_thermostat_knob
    version: "${project_version}"

esp32:
  variant: esp32s3

  # Unbedingt an das tatsächlich verbaute Flash-Modul anpassen.
  flash_size: 16MB

  framework:
    type: esp-idf

    advanced:
      # Prüft alle zukünftigen OTA-Images kryptografisch.
      signed_ota_verification:
        signing_key: !secret firmware_signing_key_path
        signing_scheme: rsa3072

      # Lehnt ältere Firmwareversionen per OTA ab.
      enable_ota_downgrade_protection: true

      # Ist standardmäßig aktiviert, sollte aber explizit dokumentiert werden.
      enable_ota_rollback: true

http_request:
  verify_ssl: true
  follow_redirects: true
  redirect_limit: 5

  # GitHub-Redirects und längere Header können den Standardpuffer überschreiten.
  buffer_size_rx: 4096
  buffer_size_tx: 1024

  # Firmwaredownloads dürfen deutlich länger als normale API-Anfragen dauern.
  timeout: 30s

ota:
  # Selbstständiges Herunterladen der Release-Firmware.
  - platform: http_request

  # Optional zusätzlich für Wartung über ESPHome Device Builder.
  - platform: esphome
    password: !secret ota_password

update:
  - platform: http_request
    id: smart_knob_firmware_update
    name: Firmware Update

    # Manifest möglichst über GitHub Pages und nicht über
    # /releases/latest/download/manifest.json laden.
    source: >-
      https://OWNER.github.io/REPOSITORY/firmware/manifest.json

    update_interval: 6h
```

### `secrets.yaml` im Release-Workflow

```yaml
firmware_signing_key_path: "/tmp/esphome-production-signing-key.pem"
ota_password: "RELEASE-BUILD-PLACEHOLDER"
```

Der private Schlüssel selbst steht nicht in `secrets.yaml`. Dort steht nur der temporäre Dateipfad.

### Hinweis zum OTA-Passwort

Das Passwort der nativen ESPHome-OTA-Plattform schützt den Wartungszugang im lokalen Netzwerk. Es ersetzt nicht die Firmwaresignatur.

Wenn ausschließlich die HTTP-Updateplattform verwendet wird, kann die native ESPHome-OTA-Plattform entfallen. Für Entwicklung und Recovery ist sie jedoch häufig hilfreich.

---

## 9. Updateauslösung über das Display

Das Gerät sollte Updates nicht ungefragt während der normalen Bedienung installieren.

Empfohlener Ablauf:

1. Das Gerät prüft automatisch alle sechs Stunden auf Updates.
2. Bei einer neuen Version erscheint ein Update-Hinweis im Display.
3. Der Nutzer öffnet die Update-Seite.
4. Angezeigt werden:
   - installierte Version
   - verfügbare Version
   - Release-Zusammenfassung
   - Schaltfläche „Jetzt aktualisieren“
   - Schaltfläche „Später“
5. Der Nutzer bestätigt das Update.
6. Touch- und Drehknopfaktionen werden für die normale Gerätesteuerung gesperrt.
7. Das Display zeigt einen nicht determinierten Fortschrittsindikator.
8. Nach erfolgreichem Download startet das Gerät neu.
9. Die neue Firmware wird nach erfolgreichem Boot als gültig markiert.
10. Bei einem frühen Bootfehler erfolgt ein Rollback.

### Update prüfen

```yaml
button:
  - platform: template
    id: check_for_firmware_update
    name: Nach Firmware-Update suchen
    on_press:
      - update.check: smart_knob_firmware_update
```

### Update installieren

```yaml
button:
  - platform: template
    id: install_firmware_update
    name: Firmware installieren
    on_press:
      - if:
          condition:
            update.is_available: smart_knob_firmware_update
          then:
            # Hier zuerst den Update-Screen anzeigen und Eingaben sperren.
            - logger.log: "Firmware-Update wird gestartet"

            - update.perform:
                id: smart_knob_firmware_update
                force_update: false
          else:
            - logger.log: "Kein Firmware-Update verfügbar"
```

Die LVGL- oder Display-Schaltfläche kann dieselben Aktionen auslösen.

### Update-Hinweis automatisch anzeigen

```yaml
update:
  - platform: http_request
    id: smart_knob_firmware_update
    name: Firmware Update
    source: >-
      https://OWNER.github.io/REPOSITORY/firmware/manifest.json
    update_interval: 6h

    on_update_available:
      then:
        - logger.log: "Eine neue Firmwareversion ist verfügbar"
        # Hier den Update-Badge oder die Update-Seite aktivieren.
```

### Fortschrittsanzeige

Die normale ESPHome-Update-Entität liefert nicht zwingend einen genauen Prozentwert des Firmwaredownloads.

Daher sollte auf dem Display standardmäßig angezeigt werden:

```text
Firmware wird heruntergeladen …
Gerät nicht ausschalten
```

Dazu eignet sich:

- ein animierter Kreis
- eine pulsierende Statusanzeige
- eine indeterminierte Fortschrittsleiste

Ein Prozentwert sollte nur angezeigt werden, wenn er tatsächlich aus einem eigenen OTA-Callback oder einer eigenen Komponente stammt.

---

## 10. Das gemeinsame Web- und OTA-Manifest

ESPHome Managed Updates erwartet ein ESP-Web-Tools-kompatibles Manifest mit zusätzlichem `ota`-Block.

Beispiel für `manifest.json`:

```json
{
  "name": "Smart Thermostat Knob",
  "version": "1.2.3",
  "home_assistant_domain": "esphome",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        {
          "path": "https://github.com/OWNER/REPOSITORY/releases/download/v1.2.3/firmware.factory.bin",
          "offset": 0
        }
      ],
      "ota": {
        "md5": "0123456789abcdef0123456789abcdef",
        "path": "https://github.com/OWNER/REPOSITORY/releases/download/v1.2.3/firmware.ota.bin",
        "release_url": "https://github.com/OWNER/REPOSITORY/releases/tag/v1.2.3",
        "summary": "Stabilitätsverbesserungen und neue Funktionen."
      }
    }
  ]
}
```

### Warum das Manifest über GitHub Pages ausgeliefert werden sollte

Die URL

```text
https://github.com/OWNER/REPOSITORY/releases/latest/download/manifest.json
```

führt über Redirects zu sehr langen URLs. Diese können den Standard-HTTP-Puffer eines ESP32 überschreiten.

Empfohlen:

```text
https://OWNER.github.io/REPOSITORY/firmware/manifest.json
```

Die Binärdateien können weiterhin als versionierte GitHub-Release-Assets gespeichert werden.

### Wichtig

Das Manifest darf erst als aktuelle Version veröffentlicht werden, nachdem:

- beide Binärdateien im Release vorhanden sind,
- die Signaturprüfung erfolgreich war,
- die Hashes erzeugt wurden,
- ein Hardwaretest erfolgreich war.

Andernfalls können Geräte eine Version erkennen, deren Download noch nicht vollständig verfügbar ist.

---

## 11. Release-Dateien

Jedes GitHub-Release sollte mindestens folgende Dateien enthalten:

```text
firmware.factory.bin
firmware.ota.bin
firmware.ota.bin.md5
firmware.factory.bin.sha256
firmware.ota.bin.sha256
checksums.sha256
manifest.json
```

Optional:

```text
build-info.json
firmware.map
CHANGELOG.md
```

### MD5-Datei erzeugen

Die MD5-Datei für ESPHome sollte nur den 32-stelligen, kleingeschriebenen Hash enthalten:

```bash
md5sum firmware.ota.bin | awk '{print $1}' > firmware.ota.bin.md5
```

Prüfung:

```bash
test "$(wc -c < firmware.ota.bin.md5)" -ge 32
grep -Eq '^[0-9a-f]{32}$' firmware.ota.bin.md5
```

### SHA-256 erzeugen

```bash
sha256sum firmware.factory.bin > firmware.factory.bin.sha256
sha256sum firmware.ota.bin > firmware.ota.bin.sha256

sha256sum \
  firmware.factory.bin \
  firmware.ota.bin \
  > checksums.sha256
```

---

## 12. GitHub-Actions-Strategie

Es sollten zwei getrennte Workflows existieren.

### 12.1 Validierungsworkflow

Auslöser:

```text
Pull Request
Push auf main
Manueller Start
```

Aufgaben:

- YAML validieren
- Firmware mit temporärem Testschlüssel kompilieren
- Dateigrößen prüfen
- Build-Ausgaben archivieren
- Niemals den Produktionsschlüssel verwenden
- Niemals ein Release veröffentlichen

### 12.2 Produktions-Release-Workflow

Auslöser:

```text
Git-Tag vX.Y.Z
```

Aufgaben:

1. Tag und Projektversion vergleichen
2. Geschütztes GitHub Environment anfordern
3. Produktionsschlüssel temporär entschlüsseln
4. Firmware kompilieren und automatisch signieren
5. `firmware.factory.bin` und `firmware.ota.bin` einsammeln
6. OTA-Signatur kryptografisch prüfen
7. MD5 und SHA-256 erzeugen
8. Manifest erzeugen
9. Release als Draft anlegen
10. Assets hochladen
11. Hardware-/Smoke-Test durchführen
12. GitHub-Pages-Manifest aktualisieren
13. Release veröffentlichen
14. Temporären Schlüssel sicher löschen

---

## 13. Beispiel für einen Release-Workflow

> Dieses Beispiel ist eine Vorlage. `CONFIG_FILE`, Repositorypfade und die genaue ESPHome-Version müssen an das Projekt angepasst werden.

```yaml
name: Release Firmware

on:
  push:
    tags:
      - "v[0-9]+.[0-9]+.[0-9]+"

permissions:
  contents: write
  pages: write
  id-token: write

concurrency:
  group: firmware-release-${{ github.ref }}
  cancel-in-progress: false

jobs:
  build-and-release:
    runs-on: ubuntu-latest

    environment:
      name: production-firmware

    env:
      CONFIG_FILE: firmware/smart-thermostat-knob.yaml
      ESPHOME_VERSION: "2026.7.2"

    steps:
      - name: Repository auschecken
        uses: actions/checkout@v6
        with:
          fetch-depth: 0

      - name: Python einrichten
        uses: actions/setup-python@v6
        with:
          python-version: "3.12"
          cache: pip

      - name: ESPHome installieren
        run: |
          python -m pip install --upgrade pip
          python -m pip install "esphome==${ESPHOME_VERSION}"

      - name: Releaseversion bestimmen
        id: version
        shell: bash
        run: |
          set -euo pipefail

          TAG="${GITHUB_REF_NAME}"
          VERSION="${TAG#v}"

          if ! [[ "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            echo "Ungültige Version: ${VERSION}" >&2
            exit 1
          fi

          echo "tag=${TAG}" >> "${GITHUB_OUTPUT}"
          echo "version=${VERSION}" >> "${GITHUB_OUTPUT}"

      - name: Produktionsschlüssel bereitstellen
        shell: bash
        env:
          SIGNING_KEY_B64: ${{ secrets.ESPHOME_SIGNING_KEY_B64 }}
        run: |
          set -euo pipefail
          umask 077

          KEY_PATH="/tmp/esphome-production-signing-key.pem"
          printf '%s' "${SIGNING_KEY_B64}" | base64 --decode > "${KEY_PATH}"
          chmod 600 "${KEY_PATH}"

          grep -q "BEGIN.*PRIVATE KEY" "${KEY_PATH}"

          cat > firmware/secrets.yaml <<EOF
          firmware_signing_key_path: "${KEY_PATH}"
          ota_password: "${{ secrets.ESPHOME_OTA_PASSWORD }}"
          EOF

      - name: Projektversion für den Release setzen
        shell: bash
        run: |
          set -euo pipefail

          VERSION="${{ steps.version.outputs.version }}"

          # Besser ist ein eigenes Build-Script oder eine CI-Substitution.
          # Diese Ersetzung muss auf eure reale YAML-Struktur angepasst werden.
          sed -i \
            -E "s/project_version: \".*\"/project_version: \"${VERSION}\"/" \
            "${CONFIG_FILE}"

      - name: ESPHome-Konfiguration validieren
        run: |
          esphome config "${CONFIG_FILE}"

      - name: Firmware kompilieren und signieren
        run: |
          esphome compile "${CONFIG_FILE}"

      - name: Firmwaredateien einsammeln
        shell: bash
        run: |
          set -euo pipefail
          mkdir -p dist

          OTA_BIN="$(find . -type f -name 'firmware.ota.bin' -print -quit)"
          FACTORY_BIN="$(find . -type f -name 'firmware.factory.bin' -print -quit)"

          if [[ -z "${OTA_BIN}" || ! -f "${OTA_BIN}" ]]; then
            echo "firmware.ota.bin wurde nicht gefunden" >&2
            exit 1
          fi

          if [[ -z "${FACTORY_BIN}" || ! -f "${FACTORY_BIN}" ]]; then
            echo "firmware.factory.bin wurde nicht gefunden" >&2
            exit 1
          fi

          cp "${OTA_BIN}" dist/firmware.ota.bin
          cp "${FACTORY_BIN}" dist/firmware.factory.bin

          test -s dist/firmware.ota.bin
          test -s dist/firmware.factory.bin

      - name: OTA-Signatur prüfen
        shell: bash
        run: |
          set -euo pipefail

          espsecure verify-signature \
            --version 2 \
            --keyfile /tmp/esphome-production-signing-key.pem \
            dist/firmware.ota.bin

      - name: Prüfsummen erzeugen
        shell: bash
        working-directory: dist
        run: |
          set -euo pipefail

          md5sum firmware.ota.bin \
            | awk '{print $1}' \
            > firmware.ota.bin.md5

          grep -Eq '^[0-9a-f]{32}$' firmware.ota.bin.md5

          sha256sum firmware.factory.bin > firmware.factory.bin.sha256
          sha256sum firmware.ota.bin > firmware.ota.bin.sha256

          sha256sum \
            firmware.factory.bin \
            firmware.ota.bin \
            > checksums.sha256

      - name: Manifest erzeugen
        shell: bash
        env:
          VERSION: ${{ steps.version.outputs.version }}
          TAG: ${{ steps.version.outputs.tag }}
        run: |
          set -euo pipefail

          MD5="$(cat dist/firmware.ota.bin.md5)"
          REPOSITORY="${GITHUB_REPOSITORY}"

          cat > dist/manifest.json <<EOF
          {
            "name": "Smart Thermostat Knob",
            "version": "${VERSION}",
            "home_assistant_domain": "esphome",
            "new_install_prompt_erase": true,
            "builds": [
              {
                "chipFamily": "ESP32-S3",
                "parts": [
                  {
                    "path": "https://github.com/${REPOSITORY}/releases/download/${TAG}/firmware.factory.bin",
                    "offset": 0
                  }
                ],
                "ota": {
                  "md5": "${MD5}",
                  "path": "https://github.com/${REPOSITORY}/releases/download/${TAG}/firmware.ota.bin",
                  "release_url": "https://github.com/${REPOSITORY}/releases/tag/${TAG}",
                  "summary": "Siehe Release Notes auf GitHub."
                }
              }
            ]
          }
          EOF

          python -m json.tool dist/manifest.json > /dev/null

      - name: Release-Artefakte intern archivieren
        uses: actions/upload-artifact@v4
        with:
          name: firmware-${{ steps.version.outputs.version }}
          path: dist/
          if-no-files-found: error
          retention-days: 30

      - name: GitHub Release als Draft erstellen
        shell: bash
        env:
          GH_TOKEN: ${{ github.token }}
        run: |
          set -euo pipefail

          gh release create "${{ steps.version.outputs.tag }}" \
            dist/firmware.factory.bin \
            dist/firmware.ota.bin \
            dist/firmware.ota.bin.md5 \
            dist/firmware.factory.bin.sha256 \
            dist/firmware.ota.bin.sha256 \
            dist/checksums.sha256 \
            dist/manifest.json \
            --verify-tag \
            --draft \
            --generate-notes \
            --title "Smart Thermostat Knob ${{ steps.version.outputs.version }}"

      - name: GitHub-Pages-Dateien vorbereiten
        shell: bash
        run: |
          set -euo pipefail
          mkdir -p pages/firmware
          cp dist/manifest.json pages/firmware/manifest.json

      - name: Pages konfigurieren
        uses: actions/configure-pages@v5

      - name: Pages-Artefakt hochladen
        uses: actions/upload-pages-artifact@v4
        with:
          path: pages

      - name: Manifest auf GitHub Pages veröffentlichen
        id: deployment
        uses: actions/deploy-pages@v4

      - name: Release veröffentlichen
        shell: bash
        env:
          GH_TOKEN: ${{ github.token }}
        run: |
          set -euo pipefail
          gh release edit "${{ steps.version.outputs.tag }}" --draft=false

      - name: Schlüsseldateien entfernen
        if: always()
        shell: bash
        run: |
          rm -f /tmp/esphome-production-signing-key.pem
          rm -f firmware/secrets.yaml
```

### Verbesserung gegenüber einfacher Veröffentlichung

Für einen besonders sicheren Ablauf sollte das Release nicht sofort automatisch veröffentlicht werden.

Besser:

```text
Build
  -> Draft Release
  -> Testgerät aktualisieren
  -> Boot, Display, Touch und Home Assistant prüfen
  -> Manuelle Freigabe
  -> Manifest veröffentlichen
  -> Release veröffentlichen
```

Dafür können zwei Jobs und ein zweites geschütztes GitHub Environment eingesetzt werden.

---

## 14. Release-Qualitätsprüfungen

Der Workflow sollte bei folgenden Problemen abbrechen:

- YAML ungültig
- Projektversion stimmt nicht mit Git-Tag überein
- Produktionsschlüssel fehlt
- `firmware.factory.bin` fehlt
- `firmware.ota.bin` fehlt
- Binärdatei ist leer
- Signaturprüfung schlägt fehl
- MD5 hat nicht genau 32 Hex-Zeichen
- Manifest ist kein gültiges JSON
- Manifestversion stimmt nicht mit Git-Tag überein
- Manifest verweist auf andere Dateinamen
- Firmware überschreitet die OTA-Partitionsgröße
- Release-Assets sind nicht erreichbar
- Hardware-Smoke-Test schlägt fehl

### Sinnvolle zusätzliche Prüfungen

```bash
stat -c '%n %s Bytes' dist/*.bin
sha256sum --check dist/checksums.sha256
python -m json.tool dist/manifest.json
```

Optional kann nach dem Release ein separater Job die veröffentlichten Dateien erneut herunterladen und mit den lokalen SHA-256-Werten vergleichen.

---

## 15. Rollout der ersten signierten Version

Wenn bereits Geräte mit unsignierter Firmware im Einsatz sind, sollte die Umstellung kontrolliert erfolgen.

### Übergangsrelease

Die erste signierte Firmware:

- enthält bereits `signed_ota_verification`,
- ist selbst korrekt mit dem Produktionsschlüssel signiert,
- aktiviert den Downgrade-Schutz,
- behält OTA-Rollback bei,
- wird zunächst nur auf Testgeräten installiert.

Ein Gerät mit alter, noch nicht signaturprüfender Firmware kann dieses Übergangsrelease über den bisherigen OTA-Weg installieren. Nach dem Neustart akzeptiert das Gerät nur noch korrekt signierte Folgeversionen.

### Empfohlene Reihenfolge

1. Produktionsschlüssel erzeugen und sichern
2. Signierte Testfirmware erstellen
3. Factory-Installation auf mehreren ESP32-S3-Testgeräten durchführen
4. Signiertes OTA-Update auf eine neuere Testversion durchführen
5. Unsigned-OTA-Test durchführen und erwartete Ablehnung prüfen
6. OTA mit falschem Schlüssel testen und erwartete Ablehnung prüfen
7. Downgrade-Test durchführen und erwartete Ablehnung prüfen
8. Defekte Testfirmware erzeugen und Rollback prüfen
9. Erstes signiertes Übergangsrelease veröffentlichen
10. Nach erfolgreichem Feldtest Signaturpflicht dauerhaft beibehalten

---

## 16. Testmatrix vor Veröffentlichung

| Test | Erwartetes Ergebnis |
|---|---|
| Factory-Flash über Web-Flasher | Gerät startet korrekt |
| WLAN-Einrichtung | Funktioniert |
| Home-Assistant-Verbindung | Funktioniert |
| Display und Touch | Funktionieren |
| Drehknopf | Funktioniert |
| Signiertes OTA mit gleichem Schlüssel | Wird installiert |
| OTA mit verändertem Byte | Wird abgelehnt |
| OTA mit falschem MD5 | Wird abgelehnt |
| OTA ohne Signatur | Wird abgelehnt |
| OTA mit fremdem Schlüssel | Wird abgelehnt |
| OTA mit niedrigerer Version | Wird abgelehnt |
| OTA mit gleicher Version | Darf laut ESPHome erneut installiert werden |
| Absturz vor erfolgreichem Boot | Rollback auf vorherige Version |
| Ausfall während Download | Alte Firmware bleibt startfähig |
| Ausfall beim Schreiben | Bootloader startet gültige Partition |
| GitHub-Redirect | Download funktioniert |
| Abgelaufenes oder ungültiges TLS-Zertifikat | Verbindung wird abgelehnt |

---

## 17. Schlüsselverlust und Schlüsselkompromittierung

### 17.1 Schlüssel verloren, aber nicht gestohlen

Bei `signed_ota_verification` ohne Hardware Secure Boot können Geräte keine neue, anders signierte OTA-Firmware akzeptieren.

Mögliche Wiederherstellung:

- Gerät seriell neu flashen
- Neue Factory-Firmware mit neuem Schlüssel installieren
- Danach wieder normale signierte OTA-Updates verwenden

### 17.2 Schlüssel gestohlen

Ein Angreifer könnte gültig signierte Schadfirmware erzeugen.

Sofortmaßnahmen:

1. Release-Workflow deaktivieren
2. GitHub-Secrets entfernen
3. Zugriffsprotokolle prüfen
4. Nutzer informieren
5. Neuen Schlüssel erzeugen
6. Recovery-Firmware und physische Wiederherstellung planen

Bei Signed OTA Verification ohne Hardware Secure Boot ist ein sicherer Schlüsselwechsel über OTA nicht einfach möglich, weil die laufende Firmware das nächste Image gegen den bisherigen Schlüssel prüft.

Vollständiges Hardware Secure Boot V2 kann mehrere Schlüsselslots und kontrollierte Schlüsselrotation unterstützen, erfordert dafür aber eine deutlich aufwendigere Produktionsarchitektur.

---

## 18. Optional: NVS-Verschlüsselung

ESPHome unterstützt auf dem ESP32-S3 eine NVS-Verschlüsselung über einen HMAC-Schlüssel in einer eFuse.

Sie schützt beispielsweise:

- gespeicherte WLAN-Zugangsdaten
- API-Schlüssel
- ESPHome-Präferenzen
- andere im NVS gespeicherte Werte

Beispiel:

```yaml
esp32:
  framework:
    type: esp-idf
    advanced:
      nvs_encryption:
        key_id: 0
```

### Achtung

- Der Schlüssel wird dauerhaft in eine eFuse geschrieben.
- Die Wahl des Schlüsselslots ist dauerhaft.
- Aktivieren oder späteres Deaktivieren kann gespeicherte Einstellungen löschen.
- Für ein offenes Community-Gerät sollte dies nicht ohne ausführliche Tests standardmäßig aktiviert werden.

Empfehlung:

```text
Community-Web-Flasher:
NVS Encryption standardmäßig aus

Geschlossene Produktionsgeräte:
NVS Encryption nach Provisionierungs- und Recovery-Tests erwägen
```

---

## 19. Optionaler späterer Produktionsmodus mit Hardware Secure Boot

Hardware Secure Boot sollte als eigenes Teilprojekt behandelt werden.

### Voraussetzungen

- eigene Produktionshardware
- dokumentierte Flash- und eFuse-Konfiguration
- gesicherter Produktionsschlüssel
- mindestens ein Recovery-Verfahren
- Testgeräte, die dauerhaft geopfert werden dürfen
- Prüfung der USB-, UART- und JTAG-Auswirkungen
- definierter Flash-Encryption-Modus
- dokumentierte Schlüsselslots
- dokumentierte Key-Rotation
- automatisierte eFuse-Verifikation
- Seriennummern- und Produktionsprotokollierung

### Produktionsablauf

```text
1. ESP32-S3-Chipidentität und Revision prüfen
2. Flash vollständig löschen
3. Signierten Secure-Boot-Bootloader flashen
4. Partitionstabelle flashen
5. Signierte Factory-App flashen
6. Optional Flash Encryption vorbereiten
7. Gerät erstmals kontrolliert starten
8. Secure-Boot-eFuse wird aktiviert
9. eFuse-Status auslesen und protokollieren
10. Neustart durchführen
11. Bootloader- und App-Verifikation testen
12. Signiertes OTA-Testupdate installieren
13. Falsches Image testen und Ablehnung bestätigen
14. Gerät erst danach ausliefern
```

### Wichtiger Hinweis zum Web-Flasher

Eine öffentliche `firmware.factory.bin`, die beim ersten Start Hardware Secure Boot aktiviert, bindet das Gerät dauerhaft an euren Schlüssel.

Das sollte nur erfolgen, wenn Nutzer ausdrücklich darüber informiert werden und ein klarer Recovery- und Supportprozess vorhanden ist.

Für die normale Community-Firmware wird daher davon abgeraten.

---

## 20. Sicherheitsstufen für das Projekt

### Stufe 1 – sofort umsetzen

- ESP-IDF
- HTTPS
- TLS-Zertifikatsprüfung
- `signed_ota_verification`
- RSA-3072
- MD5 im Manifest
- SHA-256 in Releases
- OTA-Rollback
- OTA-Downgrade-Schutz
- geschützter GitHub-Release-Workflow
- Produktionsschlüssel nicht im Repository

### Stufe 2 – nach stabiler Einführung

- GitHub Environment mit manueller Freigabe
- separater Test- und Produktionsschlüssel
- Draft-Releases
- automatisierter Download- und Hash-Recheck
- Hardware-Smoke-Test vor Manifest-Promotion
- getrennte Stable- und Beta-Manifeste
- signierte Release-Metadaten
- Software Bill of Materials

### Stufe 3 – nur für geschlossene Produktionsgeräte

- Hardware Secure Boot V2
- Flash Encryption
- NVS Encryption
- HSM oder externer Signaturdienst
- kontrollierte eFuse-Provisionierung
- Schlüsselslots und Rotation
- Seriennummernbezogene Produktionsprotokolle
- eingeschränkter UART-Downloadmodus
- dokumentierte RMA- und Recovery-Prozesse

---

## 21. Empfohlene endgültige Entscheidung für Smart Thermostat Knob

Für die aktuell geplante Veröffentlichung über GitHub Releases und einen Web-Flasher:

```text
firmware.factory.bin
- signierte App enthalten
- für ESP Web Tools zusammengeführt
- kein Hardware Secure Boot per eFuse
- serielle Wiederherstellung bleibt möglich

firmware.ota.bin
- mit demselben RSA-3072-Produktionsschlüssel signiert
- MD5 im Manifest
- SHA-256 im Release
- ausschließlich über HTTPS
- Downgrade-Schutz aktiv
- Rollback aktiv

manifest.json
- über GitHub Pages
- verweist auf versionierte GitHub-Release-Assets
- enthält Versionsnummer und MD5
- wird erst nach erfolgreicher Releaseprüfung aktualisiert
```

Damit erhält das Projekt einen guten Schutz gegen manipulierte OTA-Updates, ohne Community-Nutzer oder Entwickler durch irreversible eFuse-Einstellungen auszusperren.

Hardware Secure Boot sollte erst eingeführt werden, wenn ihr eine eigene geschlossene Produktionsvariante mit kontrollierter Provisionierung und vollständigem Recovery-Konzept anbietet.

---

## 22. Offizielle Referenzen

- [ESPHome ESP32 Platform – Signed OTA Verification, Rollback, Downgrade Protection und NVS Encryption](https://esphome.io/components/esp32/)
- [ESPHome OTA Update via HTTP Request](https://esphome.io/components/ota/http_request/)
- [ESPHome Managed Updates via HTTP Request](https://esphome.io/components/update/http_request/)
- [ESPHome Update Core](https://esphome.io/components/update/)
- [ESPHome HTTP Request](https://esphome.io/components/http_request/)
- [ESPHome Security Best Practices](https://esphome.io/guides/security_best_practices/)
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/)
- [Espressif ESP32-S3 Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/secure-boot-v2.html)
- [Espressif Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/flash-encryption.html)
- [Espressif espsecure](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/espsecure/index.html)
- [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository)
- [GitHub Actions Artifacts](https://docs.github.com/en/actions/tutorials/store-and-share-data)
