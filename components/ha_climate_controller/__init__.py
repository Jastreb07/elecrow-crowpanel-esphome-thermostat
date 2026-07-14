import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME

CONF_CLIMATES = "climates"
CONF_ENTITY_ID = "entity_id"
CONF_CONFIG_ENTITY_ID = "config_entity_id"
CONF_CONFIG_ATTRIBUTE = "config_attribute"

DEPENDENCIES = ["api"]
AUTO_LOAD = ["json"]

ha_climate_ns = cg.esphome_ns.namespace("ha_climate_controller")
HAClimateController = ha_climate_ns.class_("HAClimateController", cg.Component)


def validate_climate_entity(value):
    value = cv.string_strict(value)
    if not value.startswith("climate.") or len(value) <= len("climate."):
        raise cv.Invalid("entity_id must use the Home Assistant climate domain (climate.*)")
    return value


CLIMATE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): validate_climate_entity,
        cv.Optional(CONF_NAME, default=""): cv.string_strict,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HAClimateController),
        cv.Optional(CONF_CONFIG_ENTITY_ID, default="sensor.smart_knob_config"): cv.entity_id,
        cv.Optional(CONF_CONFIG_ATTRIBUTE, default="climates"): cv.string_strict,
        cv.Optional(CONF_CLIMATES, default=[]): cv.ensure_list(CLIMATE_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_config_entity_id(config[CONF_CONFIG_ENTITY_ID]))
    cg.add(var.set_config_attribute(config[CONF_CONFIG_ATTRIBUTE]))
    for climate in config[CONF_CLIMATES]:
        cg.add(var.add_climate(climate[CONF_ENTITY_ID], climate[CONF_NAME]))
