import {HubMode} from './HubMode';
/**
 * Gateway identity and operating mode
 */
interface HubConfig {
  /**
   * ESPHome device name (from `esphome.name` in YAML)
   * @example elero-gateway
   */
  'device': string;
  /**
   * Component version string
   * @example 1.2.3
   */
  'version': string;
  /**
   * Operating mode of the hub
   */
  'mode': HubMode;
  /**
   * Whether CRUD operations (upsert_device, remove_device) are supported
   */
  'crud': boolean;
  /**
   * Hub display name shown in Home Assistant (gateway device block) and in the web UI.
   * Defaults to the YAML-configured value (`elero_mqtt.device_name`); can be overridden
   * at runtime and persisted to NVS via `set_hub_config`.
   * @example Elero Gateway, Living Room Hub
   */
  'name': string;
}
export { HubConfig };