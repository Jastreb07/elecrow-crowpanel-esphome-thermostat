import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME

CONF_CLIMATES = "climates"
CONF_ENTITY_ID = "entity_id"

DEPENDENCIES = ["api"]

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
        cv.Required(CONF_CLIMATES): cv.All(cv.ensure_list(CLIMATE_SCHEMA), cv.Length(min=1)),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for climate in config[CONF_CLIMATES]:
        cg.add(var.add_climate(climate[CONF_ENTITY_ID], climate[CONF_NAME]))
