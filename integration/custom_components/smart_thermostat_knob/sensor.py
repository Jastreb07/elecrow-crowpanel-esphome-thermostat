"""Configuration sensor for Smart Thermostat Knob displays."""

from __future__ import annotations

import json

from homeassistant.components.sensor import SensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import ATTR_FRIENDLY_NAME, CONF_NAME
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import (
    ATTR_CLIMATES,
    ATTR_COVERS,
    ATTR_LIGHTS,
    CONF_CLIMATES,
    CONF_COVERS,
    CONF_LIGHTS,
    DOMAIN,
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the Smart Knob configuration sensor."""
    async_add_entities([SmartKnobConfigSensor(hass, entry)])


class SmartKnobConfigSensor(SensorEntity):
    """Expose selected HA entities in the format consumed by ESPHome."""

    _attr_has_entity_name = True
    _attr_icon = "mdi:tune-variant"
    _attr_should_poll = False
    _unrecorded_attributes = frozenset({ATTR_CLIMATES, ATTR_LIGHTS, ATTR_COVERS})

    def __init__(self, hass: HomeAssistant, entry: ConfigEntry) -> None:
        """Initialize the configuration sensor."""
        self._hass = hass
        self._entry = entry
        values = dict(entry.data)
        values.update(entry.options)
        knob_name = values.get(CONF_NAME, entry.title)
        self._attr_name = f"{knob_name} Config"
        self._attr_suggested_object_id = f"{knob_name} config"

        # Preserve the original sensor registry identity for installations
        # created before multiple config entries were supported.
        if entry.unique_id == DOMAIN:
            self._attr_unique_id = f"{DOMAIN}_config"
        else:
            self._attr_unique_id = f"{DOMAIN}_{entry.entry_id}_config"

    @property
    def native_value(self) -> str:
        """Return a small stable sensor state."""
        return "ready"

    @property
    def extra_state_attributes(self) -> dict[str, str]:
        """Return ESP-compatible JSON strings for all controller types."""
        values = dict(self._entry.data)
        values.update(self._entry.options)
        return {
            ATTR_CLIMATES: self._json_attribute(values.get(CONF_CLIMATES, [])),
            ATTR_LIGHTS: self._json_attribute(values.get(CONF_LIGHTS, [])),
            ATTR_COVERS: self._json_attribute(values.get(CONF_COVERS, [])),
        }

    def _json_attribute(self, entity_ids: list[str]) -> str:
        """Serialize IDs and their current HA display names."""
        items: list[dict[str, str]] = []
        for entity_id in entity_ids:
            state = self._hass.states.get(entity_id)
            name = (
                state.attributes.get(ATTR_FRIENDLY_NAME)
                if state is not None
                else None
            )
            items.append(
                {
                    "entity_id": entity_id,
                    "name": str(name or self._fallback_name(entity_id)),
                }
            )

        # The prefix guarantees that Home Assistant keeps this as a string.
        # ESPHome strips it before parsing the JSON array.
        return "json:" + json.dumps(
            items,
            ensure_ascii=False,
            separators=(",", ":"),
        )

    @staticmethod
    def _fallback_name(entity_id: str) -> str:
        """Generate a readable name if the entity is currently unavailable."""
        object_id = entity_id.partition(".")[2] or entity_id
        return object_id.replace("_", " ").title()
