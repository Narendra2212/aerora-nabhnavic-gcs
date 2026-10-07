# Parameter Interface Control Document

Parameter ownership and lifecycle. The shortest ICD in the set, because this is the area where
QGroundControl's existing behaviour is adequate and Aerora adds least.

## Ownership

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| The vehicle owns parameter values | Required | `GCS-VERIFIED` | `CONFIRMED` |
| Every parameter reaches the ground station as a Fact | Required | `GCS-VERIFIED` | `CONFIRMED` |
| There is no Aerora parameter store, cache or shadow copy | Required | `GCS-DECIDED` | `CONFIRMED` |
| No Aerora code reads a parameter from anywhere but a Fact | Required | `GCS-DECIDED` | `CONFIRMED` |
| An Aerora setting may not shadow or cache a vehicle parameter | Required | `GCS-DECIDED` | `PROPOSED` |

The first architectural principle, and it is not negotiable. Phase 1 found the parameter subsystem
to be one of QGroundControl's better-engineered areas, including bulk download by file transfer,
caching and metadata, so Aerora inherits rather than replaces it.

### Settings are not parameters

The distinction is ownership, not mechanism — both are Facts.

| Kind | Lives on | Authority |
| --- | --- | --- |
| Vehicle parameter | The vehicle | The vehicle |
| Ground-station setting | The station | The station |

Where the two would overlap, the vehicle parameter is authoritative and the setting **does not
exist**. A setting that shadows a parameter creates two answers to one question.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Crossing | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-PAR-001` | Parameter discovery | Vehicle to GCS | Flight controller | Parameter manager | The parameter set, by list or bulk transfer | Per protocol | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-PAR-002` | Parameter download | Vehicle to GCS | Flight controller | Parameter manager, Facts | Names, values, types, indices | Per protocol | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-PAR-003` | Parameter write | GCS to vehicle | Operator via GCS | Flight controller | Name, value, type | **Value readback** | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-PAR-004` | Parameter change notification | Vehicle to GCS | Flight controller | Parameter manager, Facts | A changed value | None | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-PAR-005` | Parameter metadata | GCS internal | Aerora metadata file | Fact system | Units, bounds, enumerations, descriptions, translations | Build time | Ground station | `GCS-VERIFIED` | `PROPOSED` |
| `IF-PAR-006` | Parameter validation | GCS internal | Fact metadata | Operator input path | Bounds and enumeration checks before send | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-PAR-007` | Parameter cache | GCS internal | Parameter manager | Itself | Cached parameter set, keyed with a validity check | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-PAR-008` | Parameter synchronisation state | GCS internal | Parameter manager | Operator display | Whether the set is complete and current | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-PAR-009` | Payload configuration as parameters | Bidirectional | Payload controller | Parameter manager, Facts | Payload-owned configuration, **only where parameter-backed** | Readback | Payload controller | `NOT-EVIDENCED` | `OPEN` |
| `IF-PAR-010` | Parameter reset to defaults | GCS to vehicle | Operator via GCS | Flight controller | A reset command | Required | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-PAR-001` | Discovery failure surfaced; **a partial set is never presented as complete** | `AERORA-GCS-REQ-012` | — | — | `INTEGRATION` |
| `IF-PAR-002` | Missing parameters leave their Facts absent, not defaulted | `AERORA-GCS-REQ-026` | — | — | `INTEGRATION` |
| `IF-PAR-003` | A write without readback is **not treated as applied**; the operator sees it as pending then failed | `AERORA-BUILD-REQ-011` | — | — | `INTEGRATION` |
| `IF-PAR-004` | An unsolicited change is accepted and reflected; the ground station does not resist it | `AERORA-GCS-REQ-029` | — | — | `INTEGRATION` |
| `IF-PAR-005` | A parameter without metadata is presented raw, with no units and no bounds, **and marked as having no metadata** | `AERORA-GCS-REQ-026` | — | — | `UNIT` |
| `IF-PAR-006` | Out-of-bounds input is refused before send, not after | `AERORA-GCS-REQ-026` | — | — | `UNIT` |
| `IF-PAR-007` | A stale cache is detected by the protocol's own validity check; a mismatch forces a reload | — | — | — | `INTEGRATION` |
| `IF-PAR-008` | Incomplete synchronisation is **visible**; dependent setup pages are not presented | `AERORA-GCS-REQ-013` | — | — | `INTEGRATION` |
| `IF-PAR-009` | Preferably not used — see below | `AERORA-PAYLOAD-REQ-006` | — | Q-023, Q3-006 | `INTEGRATION` |
| `IF-PAR-010` | A reset is an audit event; the resulting parameter set is reloaded, not assumed | `AERORA-COMPLY-REQ-002` | — | — | `INTEGRATION` |

## The component-id constraint

Parameter handling across QGroundControl assumes the autopilot is **component 1**, in both the
parameter manager and the fact panel controller. All `GCS-VERIFIED`.

| Consequence | Position |
| --- | --- |
| Flight-controller parameters must live on component 1 | **A firmware obligation**, `AERORA-GCS-REQ-012` |
| An architecture placing flight-relevant parameters elsewhere would fight the framework at many points | Avoided |
| Payload and companion parameters on other components | Possible, but see below |

### Payload configuration is preferably not a MAVLink parameter

| Reason | Detail |
| --- | --- |
| The subsystem assumes component 1 | Parameters on another component are second-class throughout |
| Payload configuration changes on a different cadence | A payload change would invalidate a flight-parameter cache |
| It keeps the downward boundary clean | A payload configuration error cannot present as a flight-controller parameter error |

Preferred: payload configuration is payload-controller-owned, reported and set through the payload
interface. `IF-PAR-009` exists for the case where it genuinely must be a parameter, and in that case
it is a Fact like any other — the all-parameters-are-Facts principle has no payload exemption.

Whether the payload controller is a distinct component at all is contested; see Q3-006.

## Validation

| Layer | Checks | Where |
| --- | --- | --- |
| Metadata bounds and enumerations | Before send | Ground station |
| Type correctness | Before send | Ground station |
| Vehicle-side validation | On write | Vehicle |
| Readback confirmation | After write | Ground station |

The ground station validates **against metadata**, which is advisory: the vehicle is the authority
and may reject a value the metadata permitted. A rejection is surfaced, not retried.

The firmware model contains a parameter CRC validation skeleton — `MODEL`, a placeholder. If
vehicle-side parameter validation becomes a reported condition, it is a diagnostics item; the
ground station does not duplicate it.

## Persistence and versioning

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| Parameter values persist on the vehicle | Vehicle responsibility | `GCS-VERIFIED` | `CONFIRMED` |
| The ground station persists a cache, not a source of truth | Required | `GCS-VERIFIED` | `CONFIRMED` |
| Parameter sets are saved to file for comparison and restore | Supported | `GCS-VERIFIED` | `CONFIRMED` |
| A saved set is tied to a vehicle identity | Required | `GCS-DECIDED` | `PROPOSED` |
| Parameter metadata is versioned with the build | Required | `GCS-VERIFIED` | `PROPOSED` |

A saved parameter set keyed on the MAVLink system id rather than the Aerora identity would be
restorable onto the wrong aircraft. That is the same failure `AERORA-GCS-REQ-014` prevents for every
other artifact that outlives a connection.

## Rollback

| Property | Position |
| --- | --- |
| Vehicle-side transactional parameter write | **Not supported** by the standard protocol |
| Ground-station rollback | A re-write of the previous value, which is not a rollback and is not presented as one |
| Reset to defaults | Supported as a vehicle command |
| Restore from a saved set | Supported, and is the honest form of rollback |

Calling a re-write a rollback would imply atomicity the protocol does not provide. If a write
succeeded and a dependent write failed, the vehicle is in a mixed state and the ground station says
so.

## Aerora parameters

**No Aerora parameter names are specified.** None is evidenced.

| Item | Status | Resolved by |
| --- | --- | --- |
| Aerora parameter names | `UNKNOWN` | Firmware |
| Aerora parameter metadata file | `PROPOSED` as a mechanism; empty of content | Firmware, then metadata authoring |
| Which Aerora behaviours are parameter-configurable | `UNKNOWN` | Firmware |
| Parameter name remapping across firmware versions | **Not used** — Aerora has no legacy parameter names to remap | — |

The metadata file is a build-time overlay input and the mechanism is `GCS-VERIFIED`. Its content
awaits the firmware.

## Conflicts recorded

None between sources. The parameter interface is the best-settled area in Phase 3: the mechanism is
verified, the ownership is unambiguous, and the only gap is content that belongs to the firmware.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Aerora parameter names, types, bounds, defaults | Not evidenced; a firmware deliverable |
| Which parameters are payload-owned | Q-023 and Q3-006 |
| Parameter rate limits during bulk transfer | Governed by the existing protocol implementation |
| Vehicle-side validation semantics | `MODEL` placeholder |
