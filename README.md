# Thermostat Knob – CrowPanel 2.1" HMI ESP32-S3 Rotary Display

Ein Home-Assistant-Controller für Heizungsthermostate und eine Klimaanlage,
gebaut für das **Elecrow CrowPanel 2.1" HMI ESP32-S3 Rotary Display**
(rundes 480×480 Touch-Display mit Dreh-/Druckencoder).

Optisch angelehnt an ein Apple-Home-artiges Dashboard: schwarz/weiß,
große Zieltemperatur, runder Temperatur-Ring, klare Buttons.

..\.venv\Scripts\esphome.exe run thermostat.yaml --device COM30

C:\Users\vdell\PhpstormProjects\esp\ESPHome\.venv\Scripts\python.exe C:\Users\vdell\PhpstormProjects\esp\ESPHome\thermostat\tools\lvgl_preview.py

## Was das Projekt macht

- Zeigt Ziel- und Ist-Temperatur eines Raums an (Wohnzimmer/Büro)
- Zieltemperatur per Drehknopf ändern (entprellt, siehe unten)
- HVAC-Modus wechseln (Heizen/Kühlen/Auto/Aus)
- Zwischen Räumen wechseln
- Optional eine separate Klimaanlage steuern
- Eco-/Komfort-Presets umschalten
- Zeigt den Verbindungsstatus zu Home Assistant an

Die komplette Steuerungslogik läuft direkt im ESPHome-Gerät und ruft
Home-Assistant-Services (`climate.set_temperature`,
`climate.set_hvac_mode`, `climate.set_preset_mode`) über die native
ESPHome-API auf – kein zusätzlicher Automatisierungs-Code in Home
Assistant nötig.

**Abgestimmt auf [KartoffelToby/better_thermostat](https://github.com/KartoffelToby/better_thermostat) Release 1.8.2**
(für `climate.wohnzimmer_better_thermostat` / `climate.buro_better_thermostat`):

- HVAC-Modi: better_thermostat kennt auf diesen Entitäten nur `heat`
  und `off` (kein eigenständiges `cool`/`auto`) – der Modus-Zyklus in
  `thermostat.yaml` und die Buttons auf `screen_modes` sind entsprechend
  angepasst (`button_mode_cool`/`button_mode_auto` sind bewusst
  deaktiviert, siehe [UI_CONCEPT.md](UI_CONCEPT.md)).
- Presets: `eco`/`comfort` sind reale, standardmäßig aktivierte
  better_thermostat-Presets (neben `none`/`home`/`sleep`/`away`/`boost`/
  `activity`, die aktuell nicht in der UI verdrahtet sind).
- `climate.klimaanlage_wohnzimmer` ist **keine** better_thermostat-
  Entität, sondern ein separates Klimagerät – dort gelten die
  üblichen Klimaanlagen-Modi (`cool`/`dry`/`fan_only`/`off`) unverändert.

## Hardware

- Elecrow CrowPanel 2.1" HMI, ESP32-S3, rundes 480×480 IPS-Touch-Display
- Eingebauter Dreh-/Druck-Encoder

**Wichtig:** Die genauen Pinbelegungen für Display, Touch-Controller und
Encoder sind in [`thermostat.yaml`](thermostat.yaml) mit
`# TODO: Pin prüfen` markiert. Sie wurden nicht gegen ein reales Board
verifiziert – bitte vor dem Flashen gegen das offizielle Elecrow-Wiki
für das "2.1 HMI ESP32 Round Rotary Display" prüfen und anpassen.

## Voraussetzungen

- [ESPHome](https://esphome.io/) (CLI oder Home Assistant Add-on),
  Version mit LVGL-Unterstützung (LVGL-Komponente wurde mit ESPHome
  2024.x+ eingeführt/erweitert – aktuelle Version empfohlen)
- Home Assistant mit den Ziel-Climate-Entitäten:
  - `climate.wohnzimmer_better_thermostat`
  - `climate.buro_better_thermostat`
  - optional: `climate.klimaanlage_wohnzimmer`
- USB-Kabel zum Flashen des CrowPanel

## Installation

1. Repository/Ordner öffnen:
   `C:\Users\vdell\PhpstormProjects\esp\ESPHome\thermostat`
2. `secrets.yaml.example` nach `secrets.yaml` kopieren und WLAN-Zugangsdaten,
   API-Key und OTA-Passwort eintragen (siehe [SETUP.md](SETUP.md)).
3. Pins in `thermostat.yaml` gegen das reale Board prüfen (alle
   `TODO: Pin prüfen`-Stellen).
4. Mit ESPHome kompilieren und flashen (siehe [SETUP.md](SETUP.md)).

## Home-Assistant-Konfiguration

Keine zusätzliche YAML-Konfiguration in Home Assistant nötig. Das Gerät
meldet sich über die ESPHome-API bei Home Assistant an (Integration
"ESPHome" erkennt es automatisch im Netzwerk oder wird manuell über die
IP-Adresse hinzugefügt). Home Assistant fragt beim ersten Verbinden nach
dem in `secrets.yaml` hinterlegten API-Verschlüsselungscode.

Die drei Climate-Entitäten müssen in Home Assistant bereits existieren
(z.B. über `better_thermostat` bzw. die jeweilige Klimaanlagen-Integration).

## Bedienung

| Aktion | Wirkung |
|---|---|
| Encoder drehen (rechts) | Zieltemperatur +0,5 °C |
| Encoder drehen (links) | Zieltemperatur -0,5 °C |
| Encoder kurz drücken | HVAC-Modus wechseln (Heizen → Kühlen → Auto → Aus) |
| Encoder lang drücken | Raum wechseln (Wohnzimmer ↔ Büro) |
| Encoder Doppelklick | Umschalten Thermostat ↔ Klimaanlage |
| Touch: Modus-Button | Öffnet Modus-Auswahl |
| Touch: Klima-Button | Wechselt zum Klima-Screen |
| Touch: Eco/Komfort-Button | Schaltet Preset um |

Temperaturänderungen werden **entprellt**: die UI reagiert sofort, an
Home Assistant wird aber erst 800 ms nach der letzten Drehung ein
`climate.set_temperature`-Call gesendet (siehe `send_temperature_update`
Skript in `thermostat.yaml`).

## Projektstruktur

```text
thermostat/
├── thermostat.yaml          # Vollständige ESPHome-Konfiguration (Startpunkt)
├── thermostat_helpers.h     # Kleine C++-Hilfsfunktion für HVAC-Modus-Labels
├── secrets.yaml.example     # Vorlage für WLAN/API-Zugangsdaten
├── README.md                # Diese Datei
├── SETUP.md                 # Schritt-für-Schritt-Anleitung (Flashen, SquareLine)
├── UI_CONCEPT.md            # Design-/Screen-/Bedienkonzept
├── squareline/
│   ├── crowpanel_thermostat_ui/   # SquareLine-Studio-Projekt (siehe SETUP.md)
│   └── export/                     # Zielordner für spätere LVGL-Exports
├── assets/
│   ├── icons/
│   └── fonts/
└── docs/
```

## Weiterentwicklung

- Die UI ist aktuell direkt als ESPHome-`lvgl:`-Konfiguration in
  `thermostat.yaml` abgebildet (kein SquareLine-Export nötig, siehe
  [SETUP.md](SETUP.md) für den Hintergrund). Wer die Oberfläche visuell
  in SquareLine Studio weiterentwickeln möchte, kann das Projekt in
  `squareline/crowpanel_thermostat_ui` öffnen und die Widget-Namen aus
  [UI_CONCEPT.md](UI_CONCEPT.md) 1:1 übernehmen.
- Für komplexere Firmware-Logik (z.B. mehr als 2 Räume, weitere
  Klimageräte) empfiehlt sich mittelfristig die Auslagerung der
  Hilfslogik aus `thermostat_helpers.h` in einen eigenen
  ESPHome-`external_components`- oder PlatformIO/ESP-IDF-Baustein.
- Weitere Räume: `substitutions:` erweitern (`climate_bedroom` etc.),
  `current_room_index`-Logik von 0/1 auf 0..N erweitern, zusätzlichen
  Button auf `screen_rooms` ergänzen.
