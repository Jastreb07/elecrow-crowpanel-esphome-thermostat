"""Map one Smart Knob config sensor to its ESPHome text entity."""

from __future__ import annotations

import asyncio
import logging

from homeassistant.components.button import (
    DOMAIN as BUTTON_DOMAIN,
    ButtonDeviceClass,
)
from homeassistant.components.sensor import DOMAIN as SENSOR_DOMAIN
from homeassistant.components.text import DOMAIN as TEXT_DOMAIN
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import ATTR_DEVICE_CLASS, ATTR_ENTITY_ID, EntityCategory
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers import entity_registry as er

from .const import CONF_ESPHOME_CONFIG_ENTITY, DOMAIN

_LOGGER = logging.getLogger(__name__)


async def async_sync_esphome_mapping(
    hass: HomeAssistant,
    entry: ConfigEntry,
    *,
    force_restart: bool,
) -> None:
    """Write the config sensor ID to ESPHome and restart when required."""
    values = dict(entry.data)
    values.update(entry.options)
    target_entity_id = values.get(CONF_ESPHOME_CONFIG_ENTITY)
    if not target_entity_id:
        return

    registry = er.async_get(hass)
    source_entity_id = registry.async_get_entity_id(
        SENSOR_DOMAIN,
        DOMAIN,
        _sensor_unique_id(entry),
    )
    if source_entity_id is None:
        _LOGGER.warning(
            "Could not find the configuration sensor for Smart Knob %s",
            entry.title,
        )
        return

    target_state = hass.states.get(target_entity_id)
    mapping_changed = target_state is None or target_state.state != source_entity_id
    if mapping_changed:
        try:
            await hass.services.async_call(
                TEXT_DOMAIN,
                "set_value",
                {
                    ATTR_ENTITY_ID: target_entity_id,
                    "value": source_entity_id,
                },
                blocking=True,
            )
        except HomeAssistantError:
            _LOGGER.exception(
                "Could not map %s to ESPHome text entity %s",
                source_entity_id,
                target_entity_id,
            )
            return

    if not mapping_changed and not force_restart:
        return

    if mapping_changed:
        # Give ESPHome enough time to persist the template text before reboot.
        await asyncio.sleep(1)

    restart_entity_id = _find_restart_button(hass, registry, target_entity_id)
    if restart_entity_id is None:
        _LOGGER.warning(
            "Mapped %s, but found no restart button on the ESPHome device",
            target_entity_id,
        )
        return

    try:
        await hass.services.async_call(
            BUTTON_DOMAIN,
            "press",
            {ATTR_ENTITY_ID: restart_entity_id},
            blocking=True,
        )
    except HomeAssistantError:
        _LOGGER.exception(
            "Could not restart ESPHome device through %s",
            restart_entity_id,
        )


def _sensor_unique_id(entry: ConfigEntry) -> str:
    """Return the registry identity used by the config sensor."""
    if entry.unique_id == DOMAIN:
        return f"{DOMAIN}_config"
    return f"{DOMAIN}_{entry.entry_id}_config"


def _find_restart_button(
    hass: HomeAssistant,
    registry: er.EntityRegistry,
    target_entity_id: str,
) -> str | None:
    """Find the restart button belonging to the selected ESPHome device."""
    target_entry = registry.async_get(target_entity_id)
    if target_entry is None or target_entry.device_id is None:
        return None

    for candidate in er.async_entries_for_device(registry, target_entry.device_id):
        if (
            candidate.domain != BUTTON_DOMAIN
            or candidate.disabled_by is not None
            or candidate.entity_category != EntityCategory.DIAGNOSTIC
        ):
            continue
        state = hass.states.get(candidate.entity_id)
        if candidate.original_device_class == ButtonDeviceClass.RESTART or (
            state is not None
            and state.attributes.get(ATTR_DEVICE_CLASS) == ButtonDeviceClass.RESTART
        ):
            return candidate.entity_id

    return None
