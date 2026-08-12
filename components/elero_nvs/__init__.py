import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE

from ..elero import CONF_ELERO_ID, CONF_REGISTRY_ID, DeviceRegistry, OutputAdapter, elero_ns

DEPENDENCIES = ["elero"]
AUTO_LOAD = ["json", "cover", "light", "api"]
CODEOWNERS = ["@manuschillerdev"]

NvsAdapter = elero_ns.class_("NvsAdapter", cg.Component, OutputAdapter)

CONF_NVS_ADAPTER_ID = "nvs_adapter_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero_ns.class_("Elero")),
        cv.GenerateID(CONF_REGISTRY_ID): cv.use_id(DeviceRegistry),
        cv.GenerateID(CONF_NVS_ADAPTER_ID): cv.declare_id(NvsAdapter),
    }
)


async def to_code(config):
    registry = await cg.get_variable(config[CONF_REGISTRY_ID])

    cg.add(registry.set_nvs_enabled(True))

    if "elero_mqtt" not in CORE.loaded_integrations:
        cg.add(registry.set_hub_mode(cg.RawExpression("elero::HubMode::NATIVE")))

        cg.add_define("USE_COVER")
        cg.add_define("USE_LIGHT")
        cg.add_define("ESPHOME_ENTITY_COVER_COUNT", 48)
        cg.add_define("ESPHOME_ENTITY_LIGHT_COUNT", 48)

        adapter = cg.new_Pvariable(config[CONF_NVS_ADAPTER_ID])
        cg.add(adapter.set_registry(registry))
        cg.add(registry.add_adapter(adapter))
        await cg.register_component(adapter, config)
