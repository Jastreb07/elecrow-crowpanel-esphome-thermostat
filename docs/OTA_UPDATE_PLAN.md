# OTA-Update-Feature — Plan

Status: **Umgesetzt** (thermostat_helpers.h, thermostat_common.yaml,
translations/{en,de}.yaml, firmware/thermostat_{240,480}/ota-manifest.json,
build.ps1). `esphome config` validiert für beide Boards. Noch offen: echter
Geräte-Test (Update tatsächlich auslösen), sowie die "Weiterhin offen"-Punkte
unten (Flash-Headroom-Nachrechnung beim ersten echten Release).

## Ziel (aus der Anforderung)

Beide Boards (240x240 und 480x480) sollen:

1. Manuell auf neue Firmware prüfen können (Settings > System).
2. Automatisch neue Releases aus dem GitHub-Repo erkennen und als
   **High-Priority-Notify** anzeigen.
3. Vom Notify aus per Knopfdruck direkt zu **Settings > System > Update**
   springen.
4. Von dort das Update **ausführen** können.
5. Während des Updates den **Progress-Screen** für den Fortschritt nutzen.
6. Nach Abschluss `label_progress_title` als Hinweistext verwenden
   ("Update fertig" / "Update abgebrochen").
7. Während des Updates: Progress-Screen **nicht verlassbar**, und **keine
   andere Progress-Queue** darf währenddessen angezeigt werden.

## Bereits vorhandene Bausteine, die wir wiederverwenden

- **Notify-Queue hat bereits eine `priority`-Spalte** (0-2) und
  `parse_notify_queue()` sortiert schon "highest priority first, dann
  neueste zuerst" (`thermostat_helpers.h:570`, `QueuedNotify::priority`).
  Für den Update-Hinweis reicht ein Eintrag mit `priority=2` - keine neue
  Priorisierungslogik nötig.
- **Settings-Menü "instant action" Pattern**: Der Setup-Wizard-Eintrag
  (Typ 37) ist bereits ein Menüpunkt, der beim Drücken sofort eine Aktion
  auslöst statt einen Editor zu öffnen (`settings_group_entry_type_at(...)
  == 37` → `activate_wizard_screen` statt `menu_enter`,
  `thermostat_common.yaml:1314`). Der neue "Update"-Eintrag (Typ 38) folgt
  exakt demselben Muster.
- **Wizard-Preemption-Guard-Pattern**: Der gesamte Screen-Wechsel-Code ist
  bereits konsequent mit `ui_context != 11`-Guards durchzogen, damit
  nichts den Wizard-Screen wegreißen kann (`activate_home_screen`,
  `connection_state_changed`-Handler, `on_idle`, Timer/Progress/Notify
  Auto-Show, Doppelklick-Handler). Für das "Progress-Screen während Update
  nicht verlassbar" Requirement wenden wir **dasselbe Muster** an einer
  neuen Bedingung an (`update_in_progress`), statt etwas Neues zu erfinden.
- **`label_progress_title`** existiert bereits als eigenständiges Label
  unterhalb des Prozent-Werts, aktuell befüllt aus
  `QueuedProgress::title` (`refresh_progress`,
  `thermostat_common.yaml:2862`). Wird 1:1 als Hinweistext-Ziel genutzt.
- **Progress-Queue-Format** (`id=...;title=...;value=0..100;color=...;
  led_mode=...;value_blink=...`) wird bereits vom Gerät selbst geschrieben
  für lokal erzeugte Einträge (siehe Timer-Screen "Add"-Seite als
  Präzedenzfall: das Gerät schreibt `timer_queue` selbst, nicht nur HA).
  Der Update-Fortschritt wird genauso als **ein synthetischer,
  geräteseitig geschriebener Progress-Queue-Eintrag** dargestellt.

## Entschiedene technische Fragen

Beide vorher offenen Kernfragen sind jetzt geklärt (Nutzer-Entscheidung +
Verifikation direkt im installierten ESPHome-Paket, nicht geraten):

1. **Update-Quelle**: `raw.githubusercontent.com`, wie beim Browser-Flasher
   (`master`-Branch, siehe letzte Absprache zu `flash.md`).
2. **Wie lädt/flasht sich das Gerät selbst über HTTP?**
   ESPHomes eingebaute Komponenten-Kombination:
   `http_request:` + `ota: platform: http_request` (das eigentliche
   Flash-Schreiben/Verifizieren/Reboot, unverändert von ESPHome
   übernommen - kein Risiko durch Selbstbau) +
   `update: platform: http_request` (Versions-Check + Orchestrierung).
   Das Manifest-Schema dieser Komponente habe ich direkt im installierten
   ESPHome-Paket (2026.6.5) nachgelesen
   (`http_request/update/http_request_update.cpp`,
   `update/update_entity.h`) - es ist **bewusst anders** als das
   bestehende esp-web-tools-Manifest (`manifest.json`, Format mit
   `"parts": [{"path", "offset"}]`, für den Browser-Flasher). Deshalb wie
   vom Nutzer entschieden ein **zweites, separates Manifest**:
   `firmware/thermostat_XXX/ota-manifest.json`, Schema (verifiziert, kein
   Platzhalter):
   ```json
   {
     "name": "Smart Thermostat Knob (CrowPanel 1.28\" 240x240)",
     "version": "1.1.0",
     "builds": [
       {
         "chipFamily": "ESP32-S3",
         "ota": {
           "path": "https://raw.githubusercontent.com/Jastreb07/elecrow-crowpanel-esphome-thermostat/master/firmware/thermostat_240/firmware.ota.bin",
           "md5": "<32-stelliger MD5-Hex-Hash der .bin-Datei>",
           "summary": "optional: kurzer Changelog-Text",
           "release_url": "optional: Link zu den Release Notes"
         }
       }
     ]
   }
   ```
   `md5` ist **Pflichtfeld** (Manifest wird sonst als ungültig verworfen,
   siehe Quellcode) - muss bei jedem Release neu berechnet und ins
   Manifest eingetragen werden (Build-Skript-Schritt, kein manueller
   Handarbeit-Fallstrick). `.ota.bin` bewusst als eigene Datei benannt,
   nicht identisch mit `firmware.factory.bin` (dem esp-web-tools-Build) -
   OTA-Updates brauchen ein **Nicht-factory**-Image (kein Bootloader/
   Partitionstabelle enthalten, nur die App-Partition) - wird beim
   Umsetzen anhand des bestehenden Build-Skripts (`build.ps1`) geprüft,
   ob dieses Artefakt schon irgendwo erzeugt wird oder neu gebaut werden
   muss.
3. **Woher kommt der Fortschritt in Prozent?**
   Verifiziert: `UpdateEntity::update_info.progress` (`float`, 0.0-1.0,
   nur gültig wenn `.has_progress == true`) wird von der Komponente selbst
   während `OTAState::OTA_IN_PROGRESS` laufend aktualisiert und per
   `publish_state()` propagiert. Direkt per Lambda auslesbar
   (`id(fw_update).update_info.progress`) und in den synthetischen
   Progress-Queue-Eintrag schreibbar - kein eigenes Download/Flash-
   Orchestrieren nötig.
4. **Board-Identifikation**: `device_name` ist bereits pro Board gesetzt
   (`thermostat240`/`thermostat480`, siehe `thermostat_240.yaml:16`/
   `thermostat_480.yaml`), das mappt 1:1 auf die vorhandenen
   `firmware/thermostat_240/` bzw. `firmware/thermostat_480/`
   Verzeichnisse - keine neue Board-Erkennung nötig.

## Weiterhin offen (vor dem Umsetzen zu prüfen)

- **Flash-Headroom**: Die App-Partition ist mit `flash_size: 16MB` bereits
  vergrößert (letzter Fix wegen "program size greater than maximum
  allowed"). Ein OTA-Update braucht **zwei** funktionierende App-Slots
  gleichzeitig (aktuelle Firmware + eingehendes Update, bis die
  Boot-Bestätigung erfolgt) - mit 16MB Flash sollte das reichen, wird beim
  Umsetzen aber konkret nachgerechnet (aktuell 480-Board: 2.45MB
  Firmware-Größe je Slot).
- **`.ota.bin`-Artefakt**: muss geprüft/ergänzt werden, ob das bestehende
  Build-Skript (`build.ps1`) bereits ein Nicht-Factory-Image erzeugt oder
  ob das für die Update-Pipeline neu hinzukommt (siehe Hinweis oben bei
  Punkt 2).

## Verifiziertes YAML-Grundgerüst (`http_request`/`ota`/`update`)

```yaml
http_request:
  useragent: smart-thermostat-knob
  timeout: 10s

ota:
  - platform: http_request
    id: ota_http_request

update:
  - platform: http_request
    id: fw_update
    name: "${friendly_name} Update"
    source: https://raw.githubusercontent.com/Jastreb07/elecrow-crowpanel-esphome-thermostat/master/firmware/${device_name}/ota-manifest.json
    update_interval: 6h  # deckt den periodischen Auto-Check ab, kein
                         # eigener interval:-Block nötig
    on_update_available:
      then:
        - lambda: |-
            // x ist const UpdateInfo& - x.latest_version, x.summary, ...
            // -> "fw_update" Notify-Eintrag schreiben (priority=2)
```
`update.check: fw_update` (manueller "Jetzt prüfen"-Button) und
`update.perform: fw_update` (Update auslösen, siehe Bestätigungs-Flow)
existieren bereits als fertige ESPHome-Actions - kein eigenes
Check/Perform-Lambda nötig, nur die Anbindung an Notify/Settings/Progress.
`${device_name}` ist exakt der bestehende Substitution-Wert
(`thermostat240`/`thermostat480`) - identische Directory-Struktur wie das
esp-web-tools-Manifest, kein neues Naming-Schema.

## Geplanter Ablauf (State Machine)

```
[idle] --manueller Check ODER periodischer Interval-Check-->
  [checking] --HTTP GET manifest.json--> Version vergleichen
    --Version gleich--> zurück zu [idle], keine Notify
    --Version neuer--> Notify-Eintrag (priority=2, id="fw_update") anlegen
                        --> zurück zu [idle], Notify wird per bestehendem
                            Notify-Auto-Show-Mechanismus angezeigt

[Notify "Update verfügbar" sichtbar] --Knopfdruck-->
  Settings > System > Update (Sprung analog zum Wizard-Guide-Link-Muster,
  navigiert ui_context->2 (System-Gruppe)->3 (Update-Zeile ausgewählt))

[Settings > System > Update-Zeile] --Knopfdruck-->
  Bestätigung (2-Klick-Muster wie beim bestehenden Factory-Reset, um ein
  versehentliches Auslösen zu verhindern)
    --bestätigt--> [update_in_progress = true]
                    ui_context = 9 (Progress-Screen)
                    synthetischer Progress-Eintrag "fw_update" wird
                    geschrieben (value startet bei 0)
                    ESPHome update: http_request perform() wird
                    aufgerufen

[update_in_progress == true] (auf Progress-Screen "gefangen", siehe unten)
  --Fortschritt--> Progress-Eintrag "fw_update".value wird laufend
                    aktualisiert, Arc/Prozent-Label spiegeln es wie jeden
                    anderen Progress-Eintrag
  --Erfolg--> label_progress_title = "Update abgeschlossen" (Hinweistext,
              settings.item-artiger neuer i18n key), Gerät startet ohnehin
              neu (ESPHome OTA-Standardverhalten nach erfolgreichem
              Schreiben) - update_in_progress wird beim nächsten Boot
              automatisch false sein (Global mit restore_value: false)
  --Fehler/Abbruch--> label_progress_title = "Update abgebrochen",
                       update_in_progress = false, Progress-Eintrag bleibt
                       sichtbar bis Nutzer ihn per Knopfdruck verlässt
                       (activate_home_screen wieder erlaubt)
```

## Sperren des Progress-Screens während `update_in_progress`

Exakt das Wizard-Guard-Muster, angewendet auf eine neue Bedingung:

- `activate_home_screen`, `activate_timer_screen`, `activate_notify_screen`,
  alle `on_idle`-Timeouts (beide Boards), der `connection_state_changed`-
  Handler und der Doppelklick-Shortcut bekommen zusätzlich zu ihren
  bestehenden Guards ein `&& !id(update_in_progress)` (bzw. das
  Äquivalent für den Progress-Screen-spezifischen Fall: solange
  `update_in_progress` true ist, verhält sich `ui_context == 9`
  (Progress-Screen) wie der Wizard - nichts darf wegnavigieren).
- Swipe-Handler auf `screen_progress` werden während `update_in_progress`
  zu No-ops (kein Blättern zu anderen Progress-Einträgen - erfüllt "keine
  andere Progress-Queue darf angezeigt werden").
- `refresh_progress` zeigt während `update_in_progress` **ausschließlich**
  den `fw_update`-Eintrag an, unabhängig davon, ob HA gerade andere
  Progress-Einträge in die Queue geschrieben hat (die bleiben in der Queue
  erhalten, werden aber erst nach Update-Ende wieder normal durchblätterbar).

## Neue Globals (`thermostat_common.yaml`)

- `update_in_progress` (bool, `restore_value: false`) - Kernsperre.
- `update_available` (bool, `restore_value: false`) - ob der letzte Check
  eine neuere Version gefunden hat (steuert Sichtbarkeit/Text der
  Settings-Zeile, z.B. "Update verfügbar (1.1.0)" vs. "Auf dem neuesten
  Stand").
- `update_latest_version` (std::string, `restore_value: false`).

## Neuer Settings-Menüpunkt

- `thermostat_helpers.h`: `SETTINGS_MENU_COUNT` → 39, neuer Typ 38
  "Update" in `SETTINGS_GROUP_SYSTEM` aufgenommen, neuer i18n-Key
  `settings.item.update`, Icon: vorhandenes `MENU_ICONS[15]`
  (Firmware-Glyph, mdi-information-outline) wiederverwenden oder
  `mdi:update`/`mdi:cloud-download` falls bereits als Codepoint geladen -
  während der Umsetzung anhand `MENU_ICONS`/geladener Glyphen prüfen (kein
  neuer Font-Codepoint, siehe Speicher-Eintrag zum WOFF-Icon-Font-Crash-
  Risiko bei Font-Änderungen).
- Zentraler Klick-Router (`ui_context == 3`, group items):
  `settings_group_entry_type_at(...) == 38` Zweig analog zu Typ 37, aber
  mit 2-Klick-Bestätigung statt sofortiger Aktion (siehe State Machine
  oben) - genaues Bestätigungs-Widget-Design (eigener Dialog vs.
  Wiederverwendung des Timer-Cancel-Confirm-Musters) wird beim Umsetzen
  entschieden.

## Notify-Eintrag für "Update verfügbar"

- Fester `entry_id = "fw_update"` (kein HA-Zutun nötig), `priority = 2`,
  `title` = i18n-Key mit eingesetzter Versionsnummer (z.B. "Update 1.1.0
  verfügbar").
- Wird vom Check-Script geschrieben/entfernt (nicht von HA) - beim
  nächsten erfolgreichen Update oder wenn keine neuere Version mehr
  gefunden wird, wird der Eintrag wieder aus `notify_queue` entfernt
  (gleiches Rebuild-ohne-passende-ID-Muster wie
  `notify_remove_from_queue`).
- Knopfdruck auf diesen Notify-Eintrag (statt des generischen
  Cancel/Dismiss-Verhaltens) navigiert direkt zu Settings > System > Update
  - braucht eine Sonderfall-Prüfung im Notify-Klick-Handler analog zum
  Wizard-Typ-37-Muster (`if entry_id == "fw_update" then
  activate_update_menu else <normales Notify-Klick-Verhalten>`).

## Auto-Check

- Kein eigener `interval:`-Block nötig - `update: platform: http_request`
  übernimmt das bereits selbst über `update_interval: 6h` (siehe
  verifiziertes YAML-Grundgerüst oben). Die Komponente prüft außerdem
  automatisch gestaffelt kurz nach dem Boot (alle 10s, bis zu 6 Versuche),
  falls das Netzwerk beim ersten Versuch noch nicht bereit ist - kein
  zusätzliches "Warte nach Boot"-Timing selbst nachzubauen.
- Manueller Check: neuer Button in der Update-Zeile selbst, ruft
  `update.check: fw_update` auf (Knopfdruck → "prüfen" wenn kein Update
  bekannt ist, "installieren" wenn eins bekannt ist) - oder zwei getrennte
  Aktionen; UX-Detail wird beim Umsetzen anhand eines kurzen Mockups
  entschieden.

## Neue i18n-Keys (`translations/en.yaml`, `translations/de.yaml`)

Namespace `update`:
- `available`: "Update {0} available" / "Update {0} verfügbar"
- `up_to_date`: "Up to date" / "Auf dem neuestem Stand"
- `checking`: "Checking..." / "Prüfe..."
- `confirm`: "Install update?" / "Update installieren?"
- `installing`: "Installing update..." / "Update wird installiert..."
- `done`: "Update finished" / "Update abgeschlossen"
- `failed`: "Update failed" / "Update fehlgeschlagen"
- `cancelled`: "Update cancelled" / "Update abgebrochen"

Plus `settings.item.update`: "Update" / "Update".

## Testing/Verifikation (vor Freigabe an den Nutzer zum Flashen)

- `esphome config thermostat_240.yaml` / `thermostat_480.yaml` (wie immer
  nur `config`, nie `compile`/`run` - der Nutzer baut/flasht selbst).
- Manuelle Prüfung, dass ein absichtlich älteres `manifest.json` (lokal
  simuliert) korrekt **keinen** Notify erzeugt, und ein neueres korrekt
  einen mit `priority=2` erzeugt und ganz oben in der Notify-Queue landet.
- Durchspielen der Sperre: während `update_in_progress` versuchen, per
  Swipe/Idle-Timeout/Doppelklick/HA-"Show Screen"-Button wegzunavigieren -
  darf in keinem Fall funktionieren (identische Prüfung wie beim
  Wizard-Preemption-Fix).

## Nicht Teil dieses Plans (bewusst ausgeklammert)

- Rollback auf eine vorherige Version über die UI (ESPHome/ESP-IDF
  handhabt einen fehlgeschlagenen Boot nach OTA bereits selbst über die
  beiden App-Partitionen).
- Delta-/differenzielle Updates (immer volles Firmware-Image).
- Update-Kanal-Auswahl (stable/beta) - nur ein Kanal (aktuell `master`,
  siehe letzte Absprache zu `flash.md`).

## Umsetzungsreihenfolge (sobald freigegeben)

1. `firmware/thermostat_240/ota-manifest.json` +
   `firmware/thermostat_480/ota-manifest.json` anlegen (Platzhalter-MD5,
   wird bei echten Releases durch den tatsächlichen Hash ersetzt) -
   klärt dabei auch das offene `.ota.bin`-Artefakt-Thema.
2. `thermostat_helpers.h`: neuer Settings-Typ 38, `SETTINGS_MENU_COUNT`
   39, `SETTINGS_GROUP_SYSTEM` erweitert.
3. `translations/{en,de}.yaml`: neue `update.*`-Keys +
   `settings.item.update`.
4. `thermostat_common.yaml`: `http_request:`/`ota: platform:
   http_request`/`update: platform: http_request` (siehe verifiziertes
   YAML-Grundgerüst oben), neue Globals, Notify-Integration (Schreiben/
   Entfernen des `fw_update`-Eintrags + Klick-Sonderfall via
   `on_update_available`), Settings-Klick-Router-Zweig, Bestätigungs-Flow
   (`update.perform`), Progress-Screen-Sperre (alle betroffenen Guards),
   `label_progress_title`-Hinweistexte.
5. `thermostat_240.yaml` / `thermostat_480.yaml`: nur falls neue Widgets
   nötig sind (z.B. Bestätigungsdialog) - Wiederverwendung bestehender
   Progress-/Settings-Widgets wird bevorzugt.
6. `esphome config` für beide Boards, grep-Kontrolle auf neue IDs.
7. **Flash-Headroom-Check** (siehe "Weiterhin offen" oben) vor der ersten
   echten Test-Installation - ggf. Partitionsgrößen anpassen, falls zwei
   App-Slots + die restlichen Partitionen nicht in 16MB passen.
