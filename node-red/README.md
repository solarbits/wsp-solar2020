# Node-RED flows

Flow exports for the remote monitoring dashboard. Import them through the
Node-RED editor (Menu → Import). They are exports, not a deployed
configuration — the running instance is the source of truth.

| File | Notes |
| --- | --- |
| `2021-03-03_dashboard-flow.json` | Full dashboard: MQTT input, JSON parsing, routing by `zone`/`index`, charts, gauges and Pushover alerts (87 nodes). |
| `2021-01-03_chart-node-export.json` | Partial export — a single east-panel temperature chart node. Superseded by the file above. |

## Wire format

The PLC publishes each value individually to topic `IP65`:

```json
{"zone": 0, "index": 4, "value": 285}
```

`zone` selects the kind of reading and `index` selects which one within that
kind, matching the `INDEX_*` constants in the sketch.

| zone | Contents | Units |
| --- | --- | --- |
| 0 | Sensor readings (9) | temperatures in tenths of °C, pressures in millibars |
| 1 | Pump states (2) | 0 = off, 1 = on |
| 2 | Roof states (2) | 0 low pressure, 1 pool too hot, 2 cool roof, 3 heat pool, 4 await sunshine |
| 3 | Roof powers (2) | decawatts |
| 4 | Chemistry (2) | 0 = pH ×100, 1 = chlorine in hundredths of ppm |

Changing the zone/index numbering or the topic in the sketch breaks these flows.
