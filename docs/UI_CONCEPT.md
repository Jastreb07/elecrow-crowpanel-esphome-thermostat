# UI-Konzept – Thermostat/Klima-Knob

Design-Ziel: modern, ruhig, Apple-Home-App-ähnlich. Kein "Bastel-UI".
Großzügige Touch-Ziele, wenig Text, hoher Kontrast, passend für ein
rundes 480×480-Display.

## Farben

| Rolle | Wert |
|---|---|
| Hintergrund | `#F5F5F7` |
| Karte/Button | `#FFFFFF` |
| Primärtext | `#111111` |
| Sekundärtext | `#777777` |
| Linien | `#E5E5EA` |
| Aktiver Zustand | `#111111` |
| Inaktiver Zustand | `#B0B0B0` |

Keine bunten Akzentfarben, keine Verläufe, keine unruhigen Animationen.
Schatten (falls von LVGL-Version unterstützt) nur sehr dezent auf
Buttons/Karten, nie auf Text.

## Screens

### `screen_thermostat` (Hauptscreen)

```text
        Wohnzimmer
  Home Assistant verbunden

          22.5°
       Ist: 21.8°
   [runder Temperatur-Ring]

  [Heizen] [Klima] [Eco]
```

Widgets:

| ID | Typ | Zweck |
|---|---|---|
| `label_room_name` | Label | Aktueller Raum, z.B. "Wohnzimmer" |
| `label_connection_state` | Label | "Home Assistant verbunden"/"getrennt" |
| `arc_temperature` | Arc | Visualisiert Zieltemperatur als Ring (0–100 % zwischen Min/Max) |
| `label_target_temperature` | Label | Große Zieltemperatur, z.B. "22.5°" |
| `label_current_temperature` | Label | Ist-Temperatur, z.B. "Ist: 21.8°" |
| `button_hvac_mode` / `label_hvac_mode` | Button/Label | Zeigt & wechselt HVAC-Modus |
| `button_climate` / `label_climate` | Button/Label | Wechselt zu `screen_climate` |
| `button_preset` / `label_preset` | Button/Label | Schaltet Eco/Komfort um |

Bedienung auf diesem Screen:

- Encoder drehen → `arc_temperature` + `label_target_temperature` sofort
  aktualisiert, `climate.set_temperature` folgt entprellt (siehe README).
- Encoder kurz drücken → `button_hvac_mode`-Funktion (Modus-Zyklus).
- Encoder lang drücken → Raumwechsel (Wohnzimmer ↔ Büro).
- Encoder Doppelklick → wechselt zu `screen_climate` (Klimaanlage).
- Touch auf `button_hvac_mode` → öffnet effektiv denselben Zyklus wie
  Kurzdruck (für Screens mit mehr Auswahl: `screen_modes`, siehe unten).

### `screen_rooms` (Raumauswahl)

```text
       Raum wählen

     [ Wohnzimmer ]
     [    Büro    ]
```

Widgets: `button_room_living`/`label_room_living`,
`button_room_office`/`label_room_office`. Tippen wechselt sofort den
aktiven Raum und springt zurück zu `screen_thermostat`.

### `screen_modes` (Modusauswahl)

```text
       Modus wählen

  [Heizen]      [Kühlen]
  [ Auto ]      [  Aus  ]

  [ Eco ]       [Komfort]
```

Widgets: `button_mode_heat`, `button_mode_cool`, `button_mode_auto`,
`button_mode_off`, `button_preset_eco`, `button_preset_comfort`.
`button_mode_heat`/`button_mode_off`/`button_preset_eco`/
`button_preset_comfort` rufen direkt den passenden
`climate.set_hvac_mode`/`climate.set_preset_mode`-Call für den aktuell
aktiven Raum auf und kehren zu `screen_thermostat` zurück.

**Abgestimmt auf better_thermostat 1.8.2:** `button_mode_cool` und
`button_mode_auto` sind bewusst **deaktiviert** (grau, kein `on_click`).
better_thermostat-Entitäten (`climate.wohnzimmer_better_thermostat`,
`climate.buro_better_thermostat`) melden als `hvac_modes` ausschließlich
`heat` und `off` (bzw. `heat_cool` statt `heat`, falls am BT-Gerät ein
`cooler_entity_id` konfiguriert ist – hier nicht der Fall). Ein
`climate.set_hvac_mode`-Call mit `cool`/`auto` würde von der Entität
abgelehnt. Die Buttons bleiben laut Auftrag sichtbar (Layout-Vorgabe),
sind aber inert. Quelle: `custom_components/better_thermostat/climate.py`,
Zeile ~439 (`self._hvac_list = [HVACMode.HEAT, HVACMode.OFF]`).

Preset-Modi bei better_thermostat 1.8.2 (siehe
`custom_components/better_thermostat/utils/preset_manager.py`): `none`,
`home`, `eco`, `comfort`, `sleep`, `away`, `boost`, `activity`. Dieses
Projekt nutzt nur `eco`/`comfort` (laut UI-Vorgabe) – beide sind gültige,
in better_thermostat standardmäßig aktivierte Presets. Die übrigen
Presets (`home`/`away`/`boost`/`sleep`/`activity`) könnten bei Bedarf als
weitere Buttons ergänzt werden (siehe "Spätere Erweiterungen").

### `screen_climate` (Klimaanlage)

```text
        Klimaanlage

          21.0°
   [runder Temperatur-Ring]

 [Ein/Aus] [Kühlen] [Entf.] [Lüften]
```

Widgets: `label_climate_target_temperature`, `arc_climate_temperature`,
`button_climate_power`, `button_climate_cool`, `button_climate_dry`,
`button_climate_fan`. Encoder-Drehung wirkt hier auf dieselbe
`target_temperature`-Globalvariable wie auf dem Hauptscreen, gesendet an
`climate.klimaanlage_wohnzimmer` statt an die Thermostat-Entität
(gesteuert über `control_mode`).

## Bedienlogik – Zusammenfassung

```text
Encoder rechts drehen  -> Zieltemperatur +0.5 °C (lokal sofort, HA entprellt)
Encoder links drehen   -> Zieltemperatur -0.5 °C
Encoder kurz drücken   -> HVAC-Modus durchschalten (heat <-> off; nur im Thermostat-Modus)
Encoder lang drücken   -> Raum wechseln (Wohnzimmer <-> Büro)
Encoder Doppelklick    -> Steuerungsmodus wechseln (Thermostat <-> Klima)
```

Der Kurzdruck-Zyklus ist bewusst nur 2-stufig (heat/off), da
better_thermostat 1.8.2 keine weiteren Modi auf den Thermostat-Entitäten
anbietet (siehe Abschnitt `screen_modes` oben). Im Klima-Steuerungsmodus
(nach Doppelklick) hat der Kurzdruck keine Funktion – die Klimaanlage
wird über die eigenen Buttons auf `screen_climate` bedient.

Entprellung: jede Encoder-Drehung aktualisiert sofort UI + interne
Zielvariable. Erst wenn 800 ms lang keine weitere Drehung erfolgt, wird
ein einzelner `climate.set_temperature`-Call an Home Assistant gesendet
(Skript `send_temperature_update`, `mode: restart`).

## Komponenten-Namensschema (1:1 zwischen ESPHome-LVGL und SquareLine)

Alle IDs sind bewusst so gewählt, dass ein SquareLine-Studio-Redesign
(siehe SETUP.md, Abschnitt 5) exakt dieselben Namen verwenden kann:

```text
screen_thermostat, screen_rooms, screen_modes, screen_climate

label_room_name, label_connection_state
arc_temperature, label_target_temperature, label_current_temperature
button_hvac_mode, label_hvac_mode
button_climate, label_climate
button_preset, label_preset

button_room_living, label_room_living
button_room_office, label_room_office

button_mode_heat, button_mode_cool, button_mode_auto, button_mode_off
button_preset_eco, button_preset_comfort

label_climate_target_temperature, arc_climate_temperature
button_climate_power, button_climate_cool, button_climate_dry, button_climate_fan
```

## Spätere Erweiterungen

- Weitere Räume (Schlafzimmer, Kinderzimmer): siehe SETUP.md Abschnitt 6.
- Lüftermodus-Stufen für Klimaanlage (fan_mode) als weiterer Button/Slider.
- Anzeige nicht unterstützter HVAC-Modi als ausgegraut statt versteckt.
- Dunkles Theme (Farben bereits im Auftrag vorbereitet: Hintergrund
  `#000000`, Karte `#111111`, Text `#FFFFFF`, Sekundärtext `#A0A0A0`) –
  könnte als zweite `lvgl: pages:`-Variante oder per Theme-Umschalt-Button
  ergänzt werden.
