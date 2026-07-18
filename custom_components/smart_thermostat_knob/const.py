"""Constants for the Smart Thermostat Knob integration."""

from typing import Final

from homeassistant.const import Platform

DOMAIN: Final = "smart_thermostat_knob"

CONF_CLIMATES: Final = "climates"
CONF_LIGHTS: Final = "lights"
CONF_COVERS: Final = "covers"
CONF_ESPHOME_CONFIG_ENTITY: Final = "esphome_config_entity"

DATA_FORCE_RESTART: Final = "force_restart"

ATTR_CLIMATES: Final = "climates"
ATTR_LIGHTS: Final = "lights"
ATTR_COVERS: Final = "covers"

DEFAULT_SENSOR_ENTITY_ID: Final = "sensor.smart_knob_config"

PLATFORMS: Final[list[Platform]] = [Platform.SENSOR]
