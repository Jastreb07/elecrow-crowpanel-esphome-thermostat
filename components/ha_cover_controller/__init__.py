import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME

CONF_COVERS = "covers"
CONF_ENTITY_ID = "entity_id"
CONF_CONFIG_ENTITY_ID = "config_entity_id"
CONF_CONFIG_ATTRIBUTE = "config_attribute"

DEPENDENCIES = ["api"]
AUTO_LOAD = ["json"]

ha_cover_ns = cg.esphome_ns.namespace("ha_cover_controller")
HACoverController = ha_cover_ns.class_("HACoverController", cg.Component)


def validate_cover_entity(value):
    value = cv.string_strict(value)
    if not value.startswith("cover.") or len(value) <= len("cover."):
        raise cv.Invalid("entity_id must use the Home Assistant cover domain (cover.*)")
    return value


COVER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): validate_cover_entity,
        cv.Required(CONF_NAME): cv.string_strict,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HACoverController),
        cv.Optional(CONF_CONFIG_ENTITY_ID, default="sensor.smart_knob_config"): cv.entity_id,
        cv.Optional(CONF_CONFIG_ATTRIBUTE, default="covers"): cv.string_strict,
        cv.Optional(CONF_COVERS, default=[]): cv.ensure_list(COVER_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_config_entity_id(config[CONF_CONFIG_ENTITY_ID]))
    cg.add(var.set_config_attribute(config[CONF_CONFIG_ATTRIBUTE]))
    for cover in config[CONF_COVERS]:
        cg.add(var.add_cover(cover[CONF_ENTITY_ID], cover[CONF_NAME]))
