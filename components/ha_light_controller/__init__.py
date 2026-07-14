import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME

CONF_LIGHTS = "lights"
CONF_ENTITY_ID = "entity_id"
CONF_CONFIG_ENTITY_ID = "config_entity_id"
CONF_CONFIG_ATTRIBUTE = "config_attribute"

DEPENDENCIES = ["api"]
AUTO_LOAD = ["json"]

ha_light_ns = cg.esphome_ns.namespace("ha_light_controller")
HALightController = ha_light_ns.class_("HALightController", cg.Component)


def validate_light_entity(value):
    value = cv.string_strict(value)
    if not value.startswith("light.") or len(value) <= len("light."):
        raise cv.Invalid("entity_id must use the Home Assistant light domain (light.*)")
    return value


LIGHT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): validate_light_entity,
        cv.Required(CONF_NAME): cv.string_strict,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HALightController),
        cv.Optional(CONF_CONFIG_ENTITY_ID, default="sensor.smart_knob_config"): cv.entity_id,
        cv.Optional(CONF_CONFIG_ATTRIBUTE, default="lights"): cv.string_strict,
        cv.Optional(CONF_LIGHTS, default=[]): cv.ensure_list(LIGHT_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_config_entity_id(config[CONF_CONFIG_ENTITY_ID]))
    cg.add(var.set_config_attribute(config[CONF_CONFIG_ATTRIBUTE]))
    for light in config[CONF_LIGHTS]:
        cg.add(var.add_light(light[CONF_ENTITY_ID], light[CONF_NAME]))
