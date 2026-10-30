# Design: flow meters on the west and east panels

**Status:** implemented in the sketch (`handleFlowEdge()`, `calculateFlowRates()`). The review questions at the end were still open when the code was written, so it follows the assumptions below. The Node-RED side is not done: the live flows are now in https://github.com/errkk/wsp-node-red.

## What we're adding

Each panel circuit (west and east) gets a flow meter with a passive switch that
closes once every litre and opens again shortly afterwards. (The design first
assumed one pulse every 10 litres; the meters actually pulse every litre.) The PLC counts
these pulses, works out a flow rate for each panel, and publishes it over MQTT
so the Node-RED dashboard can show it next to the other panel readings.

## Hardware

| Flow meter | PLC input | Arduino pin on the 19R+ |
| --- | --- | --- |
| West panel | I0.0 | `I0_0` = pin 2 (hardware interrupt INT4) |
| East panel | I0.1 | `I0_1` = pin 3 (hardware interrupt INT5) |

West on I0.0 and east on I0.1 follows the sketch's existing west-first ordering
(`INDEX_ROOF_WEST = 0`). Both inputs are free (I0.2 and I0.3, once used for chemistry, are also now unused).
Both are also the board's interrupt-capable inputs, so the PLC will not miss a
pulse even while the loop is busy, for example during an MQTT reconnect.

**Wiring.** The I0.x inputs are opto-isolated and need a voltage to switch
them on. The M-Duino guide rates the interrupt inputs for 5, 12 or 24 V DC,
needing at least 6 mA at 12 V. A passive switch on its own produces no voltage,
so each meter's switch is wired in series with an 1800 Ω resistor between a
12 V DC supply and the input:

```
+12 V ──── 1800 Ω ──── flow meter switch ──── I0.0  (west)
+12 V ──── 1800 Ω ──── flow meter switch ──── I0.1  (east)
0 V of the 12 V supply ──── GND terminal next to each of I0.0 and I0.1
```

This was confirmed working on site on 30 October 2026: both meters register
pulses through the 1800 Ω resistor. If an input ever stops registering, check
its status LED lights when the switch closes.

Wiring the switch between the input and its GND terminal with no supply in the
loop will never register a pulse. The input then reads HIGH while the switch is
closed and LOW while it is open.

## Counting pulses

Each input gets an interrupt on every change (`CHANGE`). The interrupt routine:

- **On opening** (input goes LOW): record the time the switch opened.
- **On closing** (input goes HIGH): count one pulse **only if the switch has
  been open for at least 200 ms**, and record the time of the pulse.

That one rule deals with both switch bounce and the stopped-while-closed case:

| Situation | What happens |
| --- | --- |
| Normal pulse | The switch has been open for many seconds, so the closing counts once. |
| Bounce as the switch closes | The bounce's brief openings last milliseconds, so the re-closings are ignored. |
| Flow stops with the switch closed | No edges, so nothing is counted, however long it stays closed. |
| Flow restarts and the switch opens | Opening never counts. Any bounce as it opens is ignored, because the switch hasn't been open for 200 ms. The next real closing, a litre later, counts normally. |
| Switch already closed when the PLC starts | Treated as "not yet opened", so the first pulse is counted only after the switch has opened and closed again. |

200 ms is well under the time between genuine pulses: at 20 L/min a pulse comes
every 3 s. The switch needs to stay open for at least 200 ms between pulses, so
very high flows (where a pulse comes every half second or so) would need that
figure reviewing. The pulse counts and times are shared with the
main loop with interrupts briefly paused while they are read, the standard
way to read multi-byte values safely on this processor.

## Working out the flow rate

The flow rate comes from the time between the last two pulses:

```
flow (L/min) = 1 litre × 60,000 / interval in ms
```

It is stored as an integer in **tenths of a litre per minute**, like the other
readings, so 20 L/min is published as `200`.

Because a pulse only comes every litre, the figure needs care when the
flow slows or stops:

- **Slowing or stopped flow.** If the time since the last pulse is already
  longer than the last interval, the flow must have dropped. The rate is then
  worked out from the time since the last pulse instead, so the displayed
  value falls steadily rather than sticking at the old figure.
- **Flow stopped.** If there has been no pulse for **5 minutes** (below
  0.2 L/min), the flow rate is reported as 0.
- **First pulse after a stop.** The interval would include the stopped time,
  so it is not used for the rate. The rate stays at 0 until the next pulse
  gives a real interval.

At 20 L/min a new pulse arrives every 3 s, so the figure is fresh each time it
is published every 5 s.

## MQTT and the dashboard

The flow rates are published with everything else on the 5-second tick, in a
new zone. No existing zone or index changes, so the current dashboard keeps
working unchanged.

| zone | index | Contents | Units |
| --- | --- | --- | --- |
| 5 | 0 | West panel flow rate | tenths of L/min |
| 5 | 1 | East panel flow rate | tenths of L/min |

Node-RED changes, using the existing pattern:

- **Zone Switch:** add a sixth output for zone 5.
- **Routing:** a new "East/West Flow Switch" on `payload.index`, followed by a
  "Convert to L/min" function (`/10`).
- **Pool Data — Right Now tab:** a flow gauge in each of the **West Panel and
  Pump** and **East Panel and Pump** groups.
- **Performance Graphs tab:** a flow-rate chart with a west and an east line.

The repo copy of the flows is an export from March 2021, and the live Node-RED
instance is the source of truth. I'll provide the new nodes as an importable
JSON snippet and update `node-red/README.md`. They then need importing into the
live instance and wiring to the existing Zone Switch there.

## Not included (possible follow-ups)

- **Power.** Roof power is calculated as `ΔT × 14`, which assumes a fixed
  20 L/min. It could use the measured flow instead, but that changes an
  existing dashboard figure, so it should be a separate change.
- **Total volume.** A running total of litres per panel could be published too.
  It would need a different unit or a daily reset to fit the dashboard's
  integer limit of 32,767.
- **No-flow alarm.** An alert when a pump is commanded on but no flow is
  measured would catch a stuck valve or failed pump.

## Questions for review

1. ~~Is the wiring above how the flow meters will be connected?~~ Answered:
   12 V through an 1800 Ω resistor and the switch into I0.0/I0.1, confirmed
   working on site.
2. Are the 200 ms minimum open time and the 5-minute "stopped" timeout sensible
   for these meters?
3. Is tenths of L/min in a new zone 5 acceptable for the dashboard?
4. Should any of the follow-ups be included now?
