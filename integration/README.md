# Smart Thermostat Knob Home Assistant integration

This custom integration creates one configuration sensor per physical Smart
Knob and exposes its selected climate, light, and cover entities in the same
JSON attribute format as the manual Template-Entity configuration.

## Manual installation

Copy this directory:

```text
integration/custom_components/smart_thermostat_knob
```

to the Home Assistant configuration directory:

```text
/homeassistant/custom_components/smart_thermostat_knob
```

Restart Home Assistant. Then open **Settings > Devices & services > Add
integration**, search for **Smart Thermostat Knob**, give the knob a recognizable
name, and select its climate, light, and cover entities. Repeat **Add
integration** for every additional Smart Knob.

Optionally select the knob's ESPHome text entity **Home Assistant Config
Entity**. The integration writes its own configuration sensor entity ID to that
text entity and automatically finds the restart button on the same ESPHome
device. The ESP restarts once after initial mapping and after every later
reconfiguration of this helper.

To rename a knob or change its assigned entities later, open that specific
integration entry and select **Reconfigure**. Other Smart Knob entries are not
affected.

At least one climate entity is required. Light and cover entities are
optional. The screen order follows the selector order, and display names come
from the Home Assistant friendly names. To change a displayed name, rename
the entity and reload this integration.

The directory `/homeassistant/www/community/` is for frontend resources. A
backend custom integration must be installed below `/homeassistant/custom_components/`.

Each entry creates its own sensor, with an entity ID derived from the chosen
name, for example `sensor.living_room_knob_config`. If that ID is already used,
Home Assistant adds a suffix. Set the corresponding ESPHome text entity **Home
Assistant Config Entity** to that knob's actual sensor entity ID. The value is
persisted by the ESP.
