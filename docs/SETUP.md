# SETUP – Einrichtung Schritt für Schritt

## 0. Warum die UI nicht per SquareLine-Studio-Automatisierung gebaut wurde

Dieser Agent hat keinen zuverlässigen, sicheren Weg, ein unbekanntes
Desktop-GUI-Programm (SquareLine Studio) blind per Mausklick-Automatisierung
zu bedienen – ein falscher Klick in einem Dialog kann Projekteinstellungen
oder Dateien beschädigen, ohne dass es sofort auffällt. Stattdessen wurde
die komplette Thermostat-Oberfläche **direkt als ESPHome-`lvgl:`-Konfiguration**
in [`thermostat.yaml`](thermostat.yaml) gebaut. Das ist funktional identisch
zu dem, was ein SquareLine-Export erzeugen würde, aber sofort lauffähig
und ohne manuellen Export-Schritt.

Der Ordner `squareline/crowpanel_thermostat_ui` wurde trotzdem angelegt.
Wenn du die Oberfläche visuell weiterentwickeln möchtest, folge den
manuellen Schritten unten.

---

## 1. Secrets anlegen

1. Datei `secrets.yaml.example` im Projektordner nach `secrets.yaml` kopieren.
2. Werte eintragen:
   - `wifi_ssid` / `wifi_password`: dein WLAN
   - `wifi_ap_password`: Passwort für den Fallback-Access-Point
   - `api_key`: 32-Byte-Base64-Schlüssel, z.B. erzeugen mit
     ```bash
     openssl rand -base64 32
     ```
   - `ota_password`: beliebiges sicheres Passwort für OTA-Updates
3. `secrets.yaml` **nicht** ins Git-Repository committen.

## 2. Pins prüfen

`thermostat.yaml` enthält an folgenden Stellen `# TODO: Pin prüfen`:

- `esp32.board` (exaktes Board evtl. abweichend von `esp32-s3-devkitc-1`)
- `spi:` (CLK/MOSI/MISO)
- `display:` (CS/DC/RESET, ggf. anderer Treiber als `ili9xxx`)
- `touchscreen:` (Interrupt/Reset, ggf. anderer Treiber als `gt911`)
- `binary_sensor: encoder_button` (Taster-Pin)
- `sensor: temperature_encoder` (Encoder-Phasen A/B)

Bitte jede Stelle gegen das offizielle Elecrow-Wiki/Schaltplan für das
"CrowPanel 2.1 HMI ESP32 Round Rotary Display" prüfen, bevor geflasht wird.

## 3. Kompilieren & Flashen

Mit der ESPHome-CLI (empfohlen: aktuelle Version mit LVGL-Unterstützung):

```bash
pip install esphome     # falls noch nicht installiert
cd "C:\Users\vdell\PhpstormProjects\esp\ESPHome\thermostat"
esphome compile thermostat.yaml
esphome run thermostat.yaml     # erstes Flashen per USB
```

Für spätere Updates funktioniert `esphome run thermostat.yaml` auch über
OTA (WLAN), sobald das Gerät einmal per USB geflasht wurde.

Alternativ über das ESPHome Home-Assistant-Add-on: Datei `thermostat.yaml`
in den Add-on-Konfigurationsordner kopieren/verlinken und dort
kompilieren/installieren.

## 4. In Home Assistant einbinden

1. Home Assistant → Einstellungen → Geräte & Dienste → Integration
   hinzufügen → "ESPHome".
2. Gerät sollte automatisch im Netzwerk gefunden werden (sonst IP manuell
   eingeben).
3. Beim Verbindungsaufbau nach dem API-Verschlüsselungscode (`api_key`
   aus `secrets.yaml`) fragen lassen und eintragen.
4. Fertig – keine weitere YAML-Konfiguration in Home Assistant nötig.

## 5. Manuelle SquareLine-Studio-Schritte (optional, für visuelles Redesign)

Falls du die UI später in SquareLine Studio 1.6.1 weiterbearbeiten willst:

1. SquareLine Studio 1.6.1 öffnen.
2. **Create New Project** wählen.
3. Im Dialog:
   - **Name:** `crowpanel_thermostat_ui`
   - **Board/Target:** "Custom" bzw. "ESP32" auswählen (kein exaktes
     CrowPanel-Preset vorhanden – generisches ESP32/Custom-Board nehmen)
   - **Resolution:** `480 x 480`
   - **Display shape:** falls verfügbar "Round"/"Circle" auswählen, sonst
     Standard-Rechteck belassen und optisch auf Kreisform layouten
   - **Color depth / LVGL-Version:** LVGL-Version passend zur ESPHome-
     LVGL-Komponente wählen (aktuell v8.x, siehe ESPHome-Release-Notes
     zur Zeit des Buildens prüfen)
4. **Project Settings → Project Root** auf folgenden Ordner setzen:
   ```text
   C:\Users\vdell\PhpstormProjects\esp\ESPHome\thermostat\squareline\crowpanel_thermostat_ui
   ```
5. Speichern (**File → Save Project**).
6. Screens gemäß [UI_CONCEPT.md](UI_CONCEPT.md) anlegen:
   `screen_thermostat`, `screen_rooms`, `screen_modes`, `screen_climate`.
7. Widgets exakt so benennen wie in UI_CONCEPT.md gelistet (z.B.
   `label_room_name`, `arc_temperature`, `button_hvac_mode` …) – die
   ESPHome-`lvgl:`-Konfiguration in `thermostat.yaml` verwendet exakt
   diese IDs, ein Abgleich/Redesign bleibt dadurch 1:1 nachvollziehbar.
8. Farben gemäß UI_CONCEPT.md als globale Styles anlegen (Hintergrund
   `#F5F5F7`, Karte `#FFFFFF`, Primärtext `#111111`, Sekundärtext
   `#777777`, Linien `#E5E5EA`).
9. Export:
   - **File → Export → Export UI Files** (oder Menüpunkt "Export" je
     nach SquareLine-Version)
   - Zielordner:
     ```text
     C:\Users\vdell\PhpstormProjects\esp\ESPHome\thermostat\squareline\export
     ```
10. Die exportierten `.c`/`.h`-Dateien beschreiben reine LVGL-Widgets.
    Für eine Integration in ESPHome müssten diese in eine eigene
    Firmware (PlatformIO/ESP-IDF mit `lvgl`-Bibliothek) übernommen werden
    – ESPHomes `lvgl:`-Komponente konsumiert keinen SquareLine-Export
    direkt, sondern erwartet die Widget-Definition in YAML (wie bereits
    in `thermostat.yaml` umgesetzt). Der Export ist daher primär für ein
    visuelles Vorschau-/Abgleich-Tool bzw. für den Wechsel auf eine
    eigene C++-Firmware gedacht.

## 6. Weitere Räume/Geräte ergänzen

1. In `thermostat.yaml` unter `substitutions:` neue Entity-ID ergänzen
   (z.B. `climate_bedroom: "climate.schlafzimmer_better_thermostat"`).
2. `current_room_index`-Bereich (aktuell 0/1) und die `switch`/`if`-
   Lambdas (`cycle_room`, `send_temperature_update`, `cycle_hvac_mode`,
   `toggle_preset`) um den neuen Index erweitern.
3. Neuen `sensor: platform: homeassistant` Eintrag für Ist-/Zieltemperatur
   des neuen Raums ergänzen.
4. Auf `screen_rooms` einen weiteren `button_room_<name>` Widget-Eintrag
   ergänzen (Layout ggf. anpassen, da nur Platz für 2 Buttons vorbereitet
   ist).
