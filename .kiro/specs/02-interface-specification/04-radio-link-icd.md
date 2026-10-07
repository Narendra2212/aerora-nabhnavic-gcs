# Radio Link Interface Control Document

The serial-first chain from the Aerora radio to MAVLink, its lifecycle, and the one trap in it.

## The chain

```text
AERORA RADIO (ground side)
        │  RF to the aircraft radio — outside the GCS contract
        ▼
USB or UART
        │  electrical and logical — hardware contract
        ▼
OPERATING SYSTEM SERIAL DEVICE
        │  enumerated as a port
        ▼
QGC SerialLink
        │  existing transport, unmodified
        ▼
MAVLink parse
        │
        ▼
Vehicle model
```

Four links in the chain. **Three are evidenced by reading upstream source; the first is not
evidenced at all.** "Evidenced" here means `GCS-VERIFIED` — the upstream behaviour was read — not
that any Aerora interface has been executed or tested.

| Link | Evidence | Status |
| --- | --- | --- |
| Radio to USB or UART | `NOT-EVIDENCED` — the module is observed, its host presentation is not | `OPEN` |
| USB or UART to serial device | `NOT-EVIDENCED` for Aerora hardware; standard behaviour otherwise | `OPEN` |
| Serial device to QGC serial link | `GCS-VERIFIED` — no device-identity gate anywhere in the manual connect path | `CONFIRMED` |
| Serial link to MAVLink | `GCS-VERIFIED` | `CONFIRMED` |

ADR-002's entire transport strategy rests on the first two links holding. They are the second most
load-bearing unevidenced item in the programme, after payload state retention.

## Why serial-first

Three radios on different bands with vehicle-side arbitration looks like three new transport types.
It is not, because the vehicle arbitrates and the ground station only needs to carry MAVLink over
whatever presents itself.

Resolving this deferred six transport core edits: a link-type enumeration, a configuration class, a
link class, factory registration, settings interface and auto-connect handling. The deferral is
conditional on the radio presenting as a serial device.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Crossing | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-RAD-001` | Radio host presentation | Radio to host | Ground radio | Operating system | A serial device, enumerable as a port | `NOT-EVIDENCED` | `OPEN` |
| `IF-RAD-002` | Serial port configuration | GCS to device | GCS link configuration | Serial driver | Port name, baud, data bits, flow control, stop bits, parity, DTR behaviour | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-RAD-003` | USB device identity | Radio to host | Ground radio | Port enumeration and board identification | Vendor id, product id, descriptors | `NOT-EVIDENCED` | `OPEN` |
| `IF-RAD-004` | Byte stream | Bidirectional | Radio and GCS | MAVLink parser and serialiser | Opaque octets | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-RAD-005` | Link lifecycle | GCS internal | Link manager | Vehicle manager | Connect, connected, disconnect, error | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-RAD-006` | Auto-connect decision | GCS internal | Serial auto-connect | Link manager | Whether to create a link for a detected port | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-RAD-007` | GCS-local link observation | GCS internal | Link manager and protocol | LinkState module, operator display | Connected, bytes, packet loss, radio-link status where supplied | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-RAD-008` | Vehicle-reported link state | Vehicle to GCS | Flight controller arbitration | LinkState module | Active link, per-link validity, all-stale condition | `MODEL` | `PROPOSED` |
| `IF-RAD-009` | Reconnection | GCS internal | Link manager | Vehicle manager | Re-establishment after loss, and identity re-binding | `GCS-VERIFIED` transport; identity re-binding `OPEN` | `PROPOSED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-RAD-001` | If the radio does not present as a serial device, ADR-002 is invalid and the six deferred transport edits return | `AERORA-COMM-REQ-006` | R-003 | Q3-001 | `BENCH` |
| `IF-RAD-002` | Unsupported setting is tolerated by the driver where the platform allows; failure surfaces as a link error | `AERORA-COMM-REQ-006` | — | — | `INTEGRATION` |
| `IF-RAD-003` | A colliding identity causes misidentification; see the trap below | `AERORA-COMM-REQ-011`, `AERORA-COMM-REQ-012` | R-003 | Q3-001 | `BENCH` |
| `IF-RAD-004` | Corrupt or partial frames are discarded by the parser; no partial message is delivered | — | — | — | `UNIT` |
| `IF-RAD-005` | Loss degrades the vehicle to stale; no vehicle-directed action beyond existing failover | `AERORA-COMM-REQ-010` | — | — | `INTEGRATION` |
| `IF-RAD-006` | An unrecognised board is never auto-connected; the operator creates the link once and it persists | `AERORA-COMM-REQ-012` | R-003 | — | `INTEGRATION` |
| `IF-RAD-007` | Presented separately from vehicle-reported state and never promoted to it | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008` | — | Q-015 | `INTEGRATION` |
| `IF-RAD-008` | Stale vehicle-reported state is shown as stale; all-stale is distinct from GCS-observed loss | `AERORA-COMM-REQ-009` | — | Q-013 | `INTEGRATION` |
| `IF-RAD-009` | On reconnection the Aerora identity binding is re-established from telemetry, never carried over | `AERORA-VEH-REQ-003` | R-017 | Q-016 | `INTEGRATION` |

## Serial characteristics

| Characteristic | Position | Evidence |
| --- | --- | --- |
| Persisted settings | Port name, baud, data bits, flow control, stop bits, parity, DTR-force-low | `GCS-VERIFIED` |
| Runtime-only flags | USB-direct and SiK-radio markers; set only by auto-connect, never persisted | `GCS-VERIFIED` |
| Identity gate on connect | **None.** Nothing in the manual connect path consults vendor id, product id, board type or the board database | `GCS-VERIFIED` |
| Port enumeration filtering | **None.** Every port is listed; the board lookup is performed and its result discarded | `GCS-VERIFIED` |
| Aerora baud rate | **`UNKNOWN`** | — |
| Aerora framing, flow control, parity | **`UNKNOWN`** | — |

### Baud rate behaviour

Arbitrary baud is supported. The settings interface offers a custom entry whose validator accepts
any positive integer, and the value is passed to the driver unchanged — `GCS-VERIFIED`.

So a radio at a non-standard rate needs no code. What it needs is a documented rate, which does not
exist.

## Connection lifecycle

```text
port appears
     │
     ├── auto-connect evaluates the port
     │        │
     │        ├── recognised board and the class is permitted → link created automatically
     │        └── unrecognised board → no link created                    ← Aerora case
     │
     └── operator creates a link manually, once
              │
              ▼
         configuration persisted
              │
              ▼
         link opens on subsequent runs
```

The Aerora case is the second branch. An unrecognised board is never auto-connected, so the
operator creates the link once; because the configuration persists, it is a one-time action.

| Property | Position | Evidence |
| --- | --- | --- |
| Auto-connect for an unrecognised board | Refused | `GCS-VERIFIED` |
| Manual link creation | Permitted for any enumerated port | `GCS-VERIFIED` |
| Manual configuration persistence | Yes | `GCS-VERIFIED` |
| Port reservation | Held by whichever subsystem claims the port first | `GCS-VERIFIED` |

## The misidentification trap

`IF-RAD-003` is not merely unevidenced. It is actively hazardous, and the hazard is one Aerora may
not control.

QGroundControl's board-identification table maps the **stock Silicon Labs CP2102 vendor and product
identifiers** to a radio class, and also matches a common FTDI device description. Both are
`GCS-VERIFIED`.

| Step | Consequence |
| --- | --- |
| An Aerora radio uses a stock CP2102 or matching FTDI bridge | It is identified as a known radio class |
| That class is permitted for auto-connect | A link is created automatically, at a fixed baud |
| The automatic link takes the port reservation | The operator's own link cannot open |
| The operator sees a link that will not connect | The cause is three subsystems away from the symptom |

### Mitigations

| Mitigation | Owner | Within Aerora's control | Status |
| --- | --- | --- | --- |
| Use vendor and product identifiers that do not collide | Hardware | Only if Aerora specifies the bridge | `OPEN` |
| Disable the radio auto-connect class in the Aerora settings defaults | Ground station | **Entirely** | `PROPOSED` |

Both are adopted. The second should be treated as the primary mitigation precisely because it is the
one Aerora can guarantee — an off-the-shelf radio module may arrive with whatever bridge the vendor
chose.

Adding an Aerora entry to the board table was considered and rejected: it would be a core edit, and
the board-class vocabulary is a closed set with no Aerora-appropriate value.

## Platform constraints

| Platform | Serial support | Constraint | Evidence |
| --- | --- | --- | --- |
| Windows | Full | None identified | `GCS-VERIFIED` |
| Linux | Full | None identified | `GCS-VERIFIED` |
| macOS | Full | Device-node name filtering applies | `GCS-VERIFIED` |
| Android | Conditional | Enumeration depends on a USB device probe table; a custom identifier may require a core edit to the prober | `GCS-VERIFIED` |
| iOS | **Disabled by build configuration** | Serial links are compiled out | `GCS-VERIFIED` |

iOS is therefore a poor fit for a serial-first product. That is a platform-scope input, tracked as
Q-032, not a defect.

## Link quality and staleness

| Item | Source | Position |
| --- | --- | --- |
| Bytes sent and received | GCS-local | Displayed as a local observation |
| Packet loss | GCS-local, scoped to the first-heard component id | Displayed as a local observation |
| Radio signal quality | Vehicle or radio, where the radio status family is supplied | Displayed where present, absent otherwise |
| Radio-link treatment | Latched on the first radio-status frame | Affects file-transfer chunk sizing only |
| Active link | **Vehicle-reported only** | Never derived from local observation |
| All-stale | **Vehicle-reported only** | Distinct from GCS-observed loss |

The separation in the last two rows is the contract. A ground station that decided which link was
active would sometimes disagree with the aircraft, and the aircraft would be right.

## Degraded operation

| Condition | GCS behaviour | Vehicle authority changes |
| --- | --- | --- |
| Port disappears | Link error; vehicle goes stale | No |
| Link open, no MAVLink | Vehicle not created, or existing vehicle goes stale | No |
| Link intermittent | Values marked stale as thresholds elapse; loss counter rises | No |
| Vehicle reports all links stale | Shown as a distinct vehicle-reported condition | Vehicle-side only; the GCS takes no action |
| Port taken by another subsystem | Operator link fails to open; see the trap above | No |

The last column is uniform for a reason: no link condition causes the ground station to change
vehicle authority. Full degradation treatment in
[18-error-degradation-model.md](18-error-degradation-model.md).

## Conflicts recorded

None between sources. The radio contract's problem is that its first two links have no evidence at
all, which is a gap rather than a conflict.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Aerora radio vendor and product identifiers | No hardware decision exists; inventing them would be worse than useless because a wrong value reads as a specification |
| Aerora baud rate, framing, flow control | Not evidenced |
| RF characteristics, range, modulation, power | Outside the ground-station boundary entirely |
| Per-radio link identifiers | The vehicle reports them; the ground station does not define them |
| Link failover timing | Vehicle-side arbitration; the ground station mirrors it |
