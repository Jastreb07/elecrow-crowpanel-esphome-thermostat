"""Smart Thermostat Knob integration."""

from homeassistant.config_entries import ConfigEntry
from homeassistant.const import CONF_NAME
from homeassistant.core import HomeAssistant

from .const import DATA_FORCE_RESTART, DOMAIN, PLATFORMS
from .mapping import async_sync_esphome_mapping


async def async_migrate_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Add a configurable name to entries created by version 1."""
    if entry.version == 1 and entry.minor_version < 3:
        data = dict(entry.data)
        if entry.minor_version < 2:
            data.update(entry.options)
        data.setdefault(CONF_NAME, entry.title or "Smart Thermostat Knob")
        hass.config_entries.async_update_entry(
            entry,
            data=data,
            options={},
            title=data[CONF_NAME],
            minor_version=3,
        )

    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Smart Thermostat Knob from a config entry."""
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    force_restart_entries = hass.data.setdefault(DOMAIN, {}).setdefault(
        DATA_FORCE_RESTART, set()
    )
    force_restart = entry.entry_id in force_restart_entries
    force_restart_entries.discard(entry.entry_id)
    await async_sync_esphome_mapping(hass, entry, force_restart=force_restart)
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    return await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
