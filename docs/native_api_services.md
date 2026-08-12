# Native Home Assistant API services

When `elero_nvs` is used without `elero_mqtt`, the NVS adapter registers native ESPHome API services for saved enabled covers. Enable custom native services in the ESPHome node configuration:

```yaml
api:
  custom_services: true
```

After compiling and flashing, Home Assistant exposes services named after the ESPHome node:

- `esphome.<node>_elero_long_up`
- `esphome.<node>_elero_long_down`
- `esphome.<node>_elero_intermediate`
- `esphome.<node>_elero_tilt`
- `esphome.<node>_elero_check`

Each service accepts `address`, the cover destination address in decimal. For example, `0x123456` is `1193046`:

```yaml
action: esphome.elero_gateway_elero_intermediate
data:
  address: 1193046
```

Long Up (`0x21`), Long Down (`0x41`), and Intermediate (`0x44`) use the cover's existing `CommandSender`, then queue a status CHECK. Tilt and Check use the existing typed registry commands.
