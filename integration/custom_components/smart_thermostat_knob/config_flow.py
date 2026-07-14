"""Config flow for Smart Thermostat Knob."""

from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.components.climate import DOMAIN as CLIMATE_DOMAIN
from homeassistant.components.light import DOMAIN as LIGHT_DOMAIN
from homeassistant.components.text import DOMAIN as TEXT_DOMAIN
from homeassistant.config_entries import ConfigFlowResult
from homeassistant.const import CONF_NAME
from homeassistant.helpers import selector

from .const import (
    CONF_CLIMATES,
    CONF_ESPHOME_CONFIG_ENTITY,
    CONF_LIGHTS,
    DATA_FORCE_RESTART,
    DOMAIN,
)


def _config_schema(values: dict[str, Any]) -> vol.Schema:
    """Build the entity selection schema with persisted defaults."""
    fields: dict[Any, Any] = {
        vol.Required(
            CONF_NAME,
            default=values.get(CONF_NAME, "Smart Thermostat Knob"),
        ): vol.All(str, vol.Strip, vol.Length(min=1, max=64)),
        vol.Required(
            CONF_CLIMATES,
            default=values.get(CONF_CLIMATES, []),
        ): selector.EntitySelector(
            selector.EntitySelectorConfig(
                domain=CLIMATE_DOMAIN,
                multiple=True,
            )
        ),
        vol.Optional(
            CONF_LIGHTS,
            default=values.get(CONF_LIGHTS, []),
        ): selector.EntitySelector(
            selector.EntitySelectorConfig(
                domain=LIGHT_DOMAIN,
                multiple=True,
            )
        ),
    }

    selected_esphome_entity = values.get(CONF_ESPHOME_CONFIG_ENTITY)
    esphome_key = (
        vol.Optional(
            CONF_ESPHOME_CONFIG_ENTITY,
            default=selected_esphome_entity,
        )
        if selected_esphome_entity
        else vol.Optional(CONF_ESPHOME_CONFIG_ENTITY)
    )
    fields[esphome_key] = selector.EntitySelector(
        selector.EntitySelectorConfig(
            domain=TEXT_DOMAIN,
            integration="esphome",
            multiple=False,
        )
    )

    return vol.Schema(fields)


def _has_climate(user_input: dict[str, Any]) -> bool:
    """Return whether the required thermostat page was selected."""
    return bool(user_input.get(CONF_CLIMATES))


class SmartThermostatKnobConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle the Smart Thermostat Knob config flow."""

    VERSION = 1
    MINOR_VERSION = 3

    @staticmethod
    def async_get_options_flow(
        config_entry: config_entries.ConfigEntry,
    ) -> SmartThermostatKnobOptionsFlow:
        """Open the editable Helper form from Home Assistant's Helpers UI."""
        return SmartThermostatKnobOptionsFlow()

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Create a Smart Thermostat Knob configuration."""
        errors: dict[str, str] = {}

        if user_input is not None:
            if _has_climate(user_input):
                user_input = _normalized_input(user_input)
                return self.async_create_entry(
                    title=user_input[CONF_NAME],
                    data=user_input,
                )
            errors["base"] = "no_climate"

        return self.async_show_form(
            step_id="user",
            data_schema=_config_schema(user_input or {}),
            errors=errors,
        )

    async def async_step_reconfigure(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Edit the name and entities of one Smart Knob."""
        errors: dict[str, str] = {}
        entry = self._get_reconfigure_entry()

        if user_input is not None:
            if _has_climate(user_input):
                user_input = _normalized_input(user_input)
                self.hass.data.setdefault(DOMAIN, {}).setdefault(
                    DATA_FORCE_RESTART, set()
                ).add(entry.entry_id)
                self.hass.config_entries.async_update_entry(
                    entry,
                    data=user_input,
                    options={},
                    title=user_input[CONF_NAME],
                )
                await self.hass.config_entries.async_reload(entry.entry_id)
                return self.async_abort(reason="reconfigure_successful")
            errors["base"] = "no_climate"

        values = dict(entry.data)
        values.update(entry.options)
        return self.async_show_form(
            step_id="reconfigure",
            data_schema=_config_schema(user_input or values),
            errors=errors,
        )


class SmartThermostatKnobOptionsFlow(config_entries.OptionsFlowWithReload):
    """Edit an existing Smart Thermostat Knob helper."""

    async def async_step_init(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Show the persisted name and entity selections."""
        errors: dict[str, str] = {}

        if user_input is not None:
            if _has_climate(user_input):
                user_input = _normalized_input(user_input)
                self.hass.data.setdefault(DOMAIN, {}).setdefault(
                    DATA_FORCE_RESTART, set()
                ).add(self.config_entry.entry_id)
                self.hass.config_entries.async_update_entry(
                    self.config_entry,
                    title=user_input[CONF_NAME],
                )
                return self.async_create_entry(data=user_input)
            errors["base"] = "no_climate"

        values = dict(self.config_entry.data)
        values.update(self.config_entry.options)
        return self.async_show_form(
            step_id="init",
            data_schema=_config_schema(user_input or values),
            errors=errors,
        )


def _normalized_input(user_input: dict[str, Any]) -> dict[str, Any]:
    """Drop an empty optional ESPHome mapping from config entry data."""
    normalized = dict(user_input)
    if not normalized.get(CONF_ESPHOME_CONFIG_ENTITY):
        normalized.pop(CONF_ESPHOME_CONFIG_ENTITY, None)
    return normalized
