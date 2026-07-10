# Auftrag: Elecrow CrowPanel 2,1" HMI-ESP32-Rotary-Display als Home-Assistant-Thermostat-/Klima-Controller entwickeln

## Ziel

Wir möchten für das **Elecrow CrowPanel 2,1" HMI-ESP32-Rotary-Display mit ESP32-S3R8, rundem IPS-Display, Touch und Drehencoder** eine professionelle Smart-Home-Oberfläche bauen.

Das Gerät soll später in Home Assistant eingebunden werden und folgende Geräte steuern:

* Heizungs-Thermostat über Home Assistant `climate` Entitäten
* Klimaanlage über Home Assistant `climate` Entität
* Raumwechsel zwischen mehreren Räumen
* Zieltemperatur per Drehknopf ändern
* Modus per Knopf oder Touch wechseln
* Statusinformationen anzeigen

Die UI soll in **SquareLine Studio 1.6.1** designed werden und anschließend als **LVGL UI** in ein ESP32-Projekt integriert werden.

---

## Grundidee

Das CrowPanel soll als moderner Wand-/Tisch-Controller funktionieren.

Bedienung:

* Drehencoder nach rechts: Zieltemperatur +0,5 °C
* Drehencoder nach links: Zieltemperatur -0,5 °C
* Drehencoder drücken: Modus wechseln
* Langer Druck: Raum wechseln
* Touch-Bedienung: Raum, Modus, Eco/Komfort auswählen
* Display zeigt aktuelle Temperatur, Zieltemperatur, Modus und Status

---

## Home-Assistant-Entitäten

Folgende Entitäten sollen vorbereitet werden:

```yaml
climate.wohnzimmer_better_thermostat
climate.buro_better_thermostat
```

Die Struktur soll aber so gebaut werden, dass später weitere Räume ergänzt werden können.

Beispielhafte Raumstruktur:

```cpp
Wohnzimmer:
  climate_entity: climate.wohnzimmer_better_thermostat

Büro:
  climate_entity: climate.buro_better_thermostat
```

Optional später:

```yaml
climate.schlafzimmer_better_thermostat
climate.kinderzimmer_better_thermostat
climate.klimaanlage_wohnzimmer
```

---

## Technische Zielarchitektur

Bitte das Projekt sauber trennen:

```text
project/
├── esphome/
│   └── crowpanel_thermostat.yaml
│
├── firmware/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── ha_controller.cpp
│   │   ├── ha_controller.h
│   │   ├── ui_controller.cpp
│   │   ├── ui_controller.h
│   │   ├── rotary_controller.cpp
│   │   └── rotary_controller.h
│   │
│   └── lib/
│
├── squareline/
│   └── crowpanel_thermostat_ui/
│
└── docs/
    ├── README.md
    ├── SETUP.md
    └── UI_CONCEPT.md
```

Wenn ESPHome mit LVGL direkt stabil möglich ist, soll bevorzugt eine **ESPHome-Lösung** gebaut werden.

Wenn die SquareLine-LVGL-Integration mit reinem ESPHome zu eingeschränkt ist, soll alternativ eine **PlatformIO/Arduino oder ESP-IDF Firmware** vorbereitet werden. Trotzdem soll die Home-Assistant-Integration sauber dokumentiert werden.

---

## UI-Design-Vorgaben

Design-Stil:

* modern
* rundes UI passend zum runden Display
* schwarz/weiß
* Apple-Home-App-ähnlich
* große Zahlen
* wenig Text
* klare Bedienung
* keine überladenen Menüs

Display:

* rund
* 480 × 480 px
* zentrale Temperaturanzeige
* kreisförmiger Temperatur-Ring
* Status oben
* Raumname oben/mittig
* Modus unten

### Hauptscreen

Elemente:

```text
Oben:
  Raumname, z. B. Wohnzimmer

Mitte:
  große Zieltemperatur, z. B. 22.5°
  darunter aktuelle Temperatur, z. B. Ist: 21.8°

Kreisring:
  zeigt Zieltemperatur visuell an

Unten:
  Modus: Heizen / Kühlen / Aus / Auto
  Eco / Komfort Button
```

Beispiel:

```text
        Wohnzimmer

          22.5°
       Ist: 21.8°

    [runder Temperatur-Ring]

      Heizen    Komfort
```

---

## Screens

Es sollen mindestens diese Screens erstellt werden:

### 1. Home/Thermostat Screen

Standardansicht.

Funktionen:

* aktuelle Raumtemperatur anzeigen
* Zieltemperatur anzeigen
* Heiz-/Kühlmodus anzeigen
* Status anzeigen: Heizen aktiv, Kühlen aktiv, Aus
* Drehencoder ändert Zieltemperatur

---

### 2. Raumauswahl Screen

Liste oder Karussell mit Räumen:

* Wohnzimmer
* Büro
* später erweiterbar

Bedienung:

* per Touch Raum auswählen
* per langem Druck zwischen Räumen wechseln

---

### 3. Modus Screen

Modusauswahl:

* Heizen
* Kühlen
* Auto
* Aus
* Eco
* Komfort

Nicht verfügbare Modi sollen entweder ausgeblendet oder deaktiviert dargestellt werden.

---

### 4. Klima Screen

Für Klimaanlage:

* Zieltemperatur
* aktueller Modus
* Lüftermodus optional
* Power On/Off
* Kühlen/Entfeuchten/Lüften optional

---

## Rotary-Encoder-Logik

Der Drehencoder ist die wichtigste Bedienung.

Funktionen:

```text
Drehen rechts:
  Zieltemperatur +0,5 °C

Drehen links:
  Zieltemperatur -0,5 °C

Kurzer Druck:
  Modus wechseln

Langer Druck:
  Raum wechseln

Doppelklick optional:
  zwischen Heizung und Klima wechseln
```

Die Temperaturänderung soll nicht bei jedem einzelnen Encoder-Puls sofort hektisch an Home Assistant gesendet werden.

Besser:

* UI sofort aktualisieren
* neuen Zielwert intern merken
* nach 500–1000 ms ohne weitere Drehung an Home Assistant senden

Dadurch werden unnötige Home-Assistant-Service-Calls vermieden.

---

## Home-Assistant-Steuerung

Für Temperaturänderung:

```yaml
service: climate.set_temperature
data:
  entity_id: climate.wohnzimmer_better_thermostat
  temperature: 22.5
```

Für HVAC-Modus:

```yaml
service: climate.set_hvac_mode
data:
  entity_id: climate.wohnzimmer_better_thermostat
  hvac_mode: heat
```

Mögliche Modi:

```text
heat
cool
auto
off
dry
fan_only
```

Bitte beachten:

Nicht jede Entität unterstützt alle Modi. Der Code soll sauber damit umgehen.

---

## Datenmodell

Bitte ein klares Datenmodell verwenden:

```cpp
struct ClimateRoom {
  const char* name;
  const char* climate_entity;
  float current_temperature;
  float target_temperature;
  const char* hvac_mode;
  bool is_available;
};
```

Oder eine vergleichbare saubere Struktur.

Ziel:

* Räume leicht erweiterbar
* UI unabhängig von konkreten Entitäten
* Home-Assistant-Kommunikation getrennt von UI-Logik

---

## SquareLine Studio

In SquareLine Studio soll ein Projekt für 480 × 480 px erstellt werden.

Vorgaben:

* LVGL Version passend zur verwendeten Firmware prüfen
* Projektname: `crowpanel_thermostat_ui`
* Auflösung: 480 × 480
* runde Displayfläche beachten
* keine wichtigen Elemente ganz am Rand platzieren
* Touch-Ziele groß genug machen
* alle UI-Elemente sinnvoll benennen

Beispiel-Namen:

```text
screen_thermostat
screen_rooms
screen_modes
screen_climate

label_room_name
label_target_temp
label_current_temp
label_hvac_mode
arc_temperature
button_mode
button_eco
button_room_next
```

Die exportierten SquareLine-Dateien sollen sauber in das Firmware-Projekt übernommen werden.

---

## UI-Aktualisierung

Bitte zentrale Funktionen erstellen:

```cpp
void ui_set_room_name(const char* room);
void ui_set_target_temperature(float temp);
void ui_set_current_temperature(float temp);
void ui_set_hvac_mode(const char* mode);
void ui_set_heating_active(bool active);
void ui_show_connection_state(bool connected);
```

Keine Home-Assistant-Logik direkt in SquareLine-Event-Code schreiben.

SquareLine Events sollen nur Funktionen im `ui_controller` oder `ha_controller` aufrufen.

---

## Verbindungsstatus

Das Display soll anzeigen, ob es verbunden ist:

```text
WLAN verbunden
Home Assistant verbunden
Home Assistant getrennt
Entität nicht verfügbar
```

Bei Verbindungsfehler:

* keine Abstürze
* UI bleibt bedienbar
* Status wird sichtbar angezeigt

---

## Design-Details

Farben:

```text
Hintergrund: #F5F5F7 oder #FFFFFF
Text: #111111
Sekundärtext: #777777
Kartenfläche: #FFFFFF
Dezente Linien: #E5E5EA
Aktiv: Schwarz oder dunkles Grau
Warnung/Fehler: dezent, nicht grell
```

Optional dunkles Theme vorbereiten:

```text
Hintergrund: #000000
Kartenfläche: #111111
Text: #FFFFFF
Sekundärtext: #A0A0A0
```

Wichtig:

* keine bunten Bastel-UI-Farben
* keine zu kleinen Buttons
* keine unruhigen Animationen
* alles muss auf 480 × 480 px gut lesbar sein

---

## Qualitätsanforderungen

Bitte besonders beachten:

* sauberer, wartbarer Code
* klare Trennung zwischen UI, Home Assistant und Encoder
* keine hart verstreuten Entity-IDs im Code
* Entity-IDs zentral konfigurierbar machen
* Home-Assistant-Service-Calls entprellen
* Display darf nicht flackern
* keine dauernden unnötigen UI-Redraws
* Fehlerzustände sauber behandeln
* Dokumentation schreiben

---

## Dokumentation

Bitte folgende Dateien erstellen:

### `README.md`

Inhalt:

* Was das Projekt macht
* Hardware
* Voraussetzungen
* Installation
* Flash-Anleitung
* Home-Assistant-Konfiguration
* Bedienung

### `SETUP.md`

Inhalt:

* SquareLine Studio Projekt öffnen
* UI exportieren
* Firmware bauen
* ESP32 flashen
* WLAN/API konfigurieren

### `UI_CONCEPT.md`

Inhalt:

* Screens
* Bedienlogik
* Farben
* Komponenten
* spätere Erweiterungen

---

## Ergebnis

Am Ende soll ein lauffähiges Grundprojekt entstehen, mit dem das CrowPanel:

* in Home Assistant eingebunden wird
* Zieltemperatur ändern kann
* aktuelle Werte anzeigen kann
* zwischen Wohnzimmer und Büro wechseln kann
* Heiz-/Klimamodus wechseln kann
* eine moderne runde UI besitzt
* über SquareLine Studio weiter bearbeitet werden kann

Bitte nicht nur kleine Code-Snippets liefern, sondern vollständige Dateien mit klarer Projektstruktur.
