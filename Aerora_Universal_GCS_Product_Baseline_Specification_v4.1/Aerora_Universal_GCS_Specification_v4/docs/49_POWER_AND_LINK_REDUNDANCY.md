# 49 — Power and Communication Redundancy

## Dual power concept

Aerora's product specification requires a dual-power UAV capable of operating from two distinct physical configurations:
- **tether power:** **480 V DC** supplied from the ground through the tether and converted onboard by a DC-DC stage to the aircraft 48 V power domain
- **onboard battery:** battery-powered operation for complex/remote areas

The GCS shall therefore expose a power-source/configuration model rather than a single battery-only model.

## Painting configuration coupling

| Configuration | Aircraft power | Paint source | Physical configuration |
|---|---|---|---|
| Tethered painting | Ground 480 V DC -> onboard DC-DC conversion -> 48 V | Ground through tether hose | Tether power cable + paint hose attached; onboard tank detached |
| Battery painting | Onboard battery | Onboard paint tank | Onboard tank attached; no tether paint supply required |

These are operating configurations, not yet a claim of automatic in-flight source switching. Any future source transition or hybrid operation requires separate engineering and verification.

## Required GCS power states

- tether connected/disconnected
- battery present/absent
- active source
- source transition
- battery state of charge
- voltage/current/power
- power-system fault
- estimated remaining energy where validated
- degraded/limited-power mode

## Important engineering distinction

The uploaded schematic has two connectors labelled `Theterd & Battery power input` and includes power regulation and `INA226` monitoring. The schematic does not, by itself, prove the final 480 V DC-to-48 V DC converter topology, automatic source arbitration, ideal-diode/ORing behavior, tether-loss detection or transition transient performance. These must be specified and verified. The current product requirement is therefore treated as two defined operating configurations rather than an assumed seamless in-flight power switchover.

## Communication redundancy

The current firmware model explicitly contains three communication paths:
- LoRa
- FHSS24
- RF433

The current model comments define a priority order of LoRa -> FHSS24 -> RF433 and an all-links-stale state.

The GCS shall display:
- link ID
- validity
- freshness
- quality/strength where available
- active link
- fallback state
- all-links-stale state
- command acknowledgement state

## Universal transport abstraction

The GCS application layer shall not depend directly on a particular radio. All transports should feed a common connection/vehicle protocol layer.

## Future communications

The architecture should permit additional approved transports such as Ethernet, Wi-Fi, cellular or secure tactical links without changing vehicle mission semantics.
