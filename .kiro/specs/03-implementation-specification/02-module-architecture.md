# Module Architecture

Seventeen product-independent platform modules, four product profiles, and the dependency rules that
keep the second product cheap.

## Module register

Identifier scheme `M-<Name>`. Every module owns a disjoint set of interfaces; no interface has two
owners.

| ID | Module | Owns | Interfaces | Depends on |
| --- | --- | --- | --- | --- |
| `M-Plugin` | `AeroraPlugin` | Overlay entry point, lifecycle, registration of everything else, QML engine hook, URL interceptor | `IF-SYS` 8 | L1 only |
| `M-Config` | `AeroraConfiguration` | Settings defaults, configuration versioning, applied-version reporting | — | `M-Plugin` |
| `M-Capability` | `AeroraCapability` | Capability model, the eight resolution rules, presence/availability split | `IF-FC-017`, `IF-MAV-026` | `M-Telemetry` |
| `M-Vehicle` | `AeroraVehicle` | Vehicle integration, parameter policy, flight-mode accessor policy | `IF-FC` 24, `IF-PAR` 10 | `M-Telemetry`, `M-Capability` |
| `M-Firmware` | `AeroraFirmwarePlugin` | Firmware plugin and factory, fact group provision | — | `M-Telemetry` |
| `M-Telemetry` | `AeroraTelemetry` | Fact group base, freshness stamping, staleness evaluation, metadata loading | `IF-TLM` 13 | L1 only |
| `M-Authority` | `AeroraAuthority` | Eight-state command tracker, corroboration, authority mode state | `IF-AUT` 12 | `M-Telemetry`, `M-Authz` |
| `M-Mission` | `AeroraMission` | Mission integration, complex items, plan authority, resume policy | `IF-MIS` 16 | `M-Capability` |
| `M-Payload` | `AeroraPayload` | Payload abstraction, payload state representation | `IF-PAY` 15 | `M-Telemetry`, `M-Capability` |
| `M-Power` | `AeroraPower` | Power and tether state representation | `IF-PWR` 12 | `M-Telemetry` |
| `M-LinkState` | `AeroraLinkState` | Vehicle-reported arbitration mirror, per-link local observation | `IF-RAD` 9 | `M-Telemetry` |
| `M-Autonomy` | `AeroraAutonomy` | Companion proposal representation. **Read-only, no outbound path** | `IF-GPU` 8 | `M-Telemetry` |
| `M-Identity` | `AeroraIdentity` | Vehicle identity binding, unidentified attribution | `IF-VID` 7 | `M-Telemetry` |
| `M-Security` | `AeroraSecurity` | Operator identity, authentication, signing key provisioning policy | `IF-SEC` 21 | `M-Config` |
| `M-Authz` | `AeroraAuthorization` | Role resolution, gated-operation set, authorisation decisions | — | `M-Security` |
| `M-Audit` | `AeroraAudit` | Record writer, correlation identity, local-first ordering, retention | `IF-LOG` 24 | `M-Security`, `M-Identity` |
| `M-Cloud` | `AeroraCloud` | Backend adapters, bounded queues, per-feature degradation | `IF-CLD` 16 | `M-Security`, `M-Audit` |
| `M-Ui` | `AeroraUi` | State-class rendering, seam content, startup self-check | `IF-UIS` 17 | All of the above |
| `M-Mavlink` | `AeroraMavlink` | Dialect boundary, message routing policy | `IF-MAV` 28 | L1 only |

Nineteen entries for the seventeen named in the directive: `M-Authz` is separated from `M-Security`
because F3.1-04 requires role resolution to be replaceable independently of authentication, and
`M-Capability` is separated from `M-Vehicle` because the capability model must serve payload,
power and autonomy equally.

## Dependency rules

The four Phase 2 rules, plus the one Phase 4 adds.

| # | Rule | Verification |
| --- | --- | --- |
| 1 | Presentation depends on domain only; domain never on presentation | `UNIT` — **no test assigned yet**, R3-001 |
| 2 | Domain reaches QGroundControl only through the extension layer | `UNIT` — **no test assigned yet**, R3-001 |
| 3 | Payload and mission are mutually independent | `UNIT` — **no test assigned yet**, R3-001 |
| 4 | Cloud depends on security, never the reverse | `UNIT` — test assigned under `AERORA-AWS-REQ-012` |
| 5 | **No L3 platform module references a product profile** | `STATIC` plus `UNIT` — new in Phase 4 |

R3-001 recorded that three of the four rules have no test assigned. Phase 4's response is in
[16-test-implementation.md](16-test-implementation.md): extend the existing
architectural-property test rather than add a new mechanism, because the mechanism already runs in
the default gate under `AERORA-TEST-REQ-006`.

Rule 3 is the one most likely to be broken by a plausible feature. "Run the paint pattern" couples
mission to payload in the obvious implementation. The permitted meeting points are exactly two:
mission items the pattern emits, and payload state the vehicle reports. Nothing else.

## Product profiles

```text
Aerora Universal GCS
│
├── L3 platform  — 19 modules, product-independent
│
└── L2 profiles
    ├── Painting        FIRST PRODUCT
    ├── Firefighting    not defined
    ├── Agriculture     not defined
    └── Inspection      not defined
```

### What a profile may contain

| Permitted | Prohibited |
| --- | --- |
| Its own fact group and metadata file | Adding a field to an L3 structure |
| Its own complex mission item and plan creator | Modifying the mission engine |
| Content at the tier-2 QML seams | Branching upstream action dispatch |
| Its own product panel | A product name anywhere in L3 |
| Declaring capabilities it consumes | Assuming a capability the vehicle has not reported |
| Reporting a product condition as payload state | Letting a product condition reach a flight command |

### The painting profile, as it stands

| Element | Content | Status |
| --- | --- | --- |
| Module | `M-ProfilePainting`, inside `M-Payload`'s layer-2 space | `IMPLEMENTATION-PLANNED` |
| Product fact group | `aeroraPaint` — six value names carried from Phase 2, **names only** | `CONTRACT-ONLY` |
| Product telemetry interface | `IF-TLM-004` | `CONTRACT-ONLY` |
| Complex mission item | A coverage pattern, geometry undefined | `CONTRACT-ONLY` |
| Map visuals, guided actions, panel | Tier-2 seam content | `IMPLEMENTATION-PLANNED` |
| Units, ranges, enumerations, actuator identities, command set, calibration | **Not specified** — Q-023, Q-043 | `CONTRACT-ONLY` |

**No painting functionality is authorised in Phase 4.** The profile is a directory with a declared
boundary and no behaviour. Pump, nozzle and gimbal control are explicitly out of scope.

### The firefighting, agriculture and inspection profiles

Not defined. No product definition exists for any of them, and writing speculative profiles would be
the universal-architecture mistake the capability model was built to avoid. The architecture's claim
is that adding one costs nothing in L3; that claim is `SPECIFIED-NOT-VERIFIED` and stays that way
until a second product exists.

## Overlay directory layout

Proposed, grounded in the verified overlay contract rather than an invented tree. Not created.

```text
AeroraGCS/                        fork of mavlink/qgroundcontrol
├── aerora/                       QGC_CUSTOM_DIR=aerora
│   ├── CMakeLists.txt            REQUIRED by the overlay contract
│   ├── cmake/
│   │   └── CustomOverrides.cmake REQUIRED by the overlay contract
│   ├── src/
│   │   ├── AeroraPlugin/         M-Plugin, M-Config
│   │   ├── Capability/           M-Capability
│   │   ├── Vehicle/              M-Vehicle, M-Firmware, M-Identity
│   │   ├── Telemetry/            M-Telemetry, FactGroups
│   │   ├── Authority/            M-Authority
│   │   ├── Mission/              M-Mission
│   │   ├── Payload/              M-Payload
│   │   ├── Power/                M-Power
│   │   ├── LinkState/            M-LinkState
│   │   ├── Autonomy/             M-Autonomy
│   │   ├── Security/             M-Security, M-Authz
│   │   ├── Audit/                M-Audit
│   │   ├── Cloud/                M-Cloud
│   │   ├── Mavlink/              M-Mavlink
│   │   └── Profiles/
│   │       └── Painting/         L2, first product
│   ├── qml/                      M-Ui, named QML modules
│   ├── res/
│   │   ├── json/                 Fact metadata, settings groups
│   │   └── override/             Tier-2 and tier-3 seam content
│   └── test/                     Aerora tests, upstream framework
├── src/                          UPSTREAM — not modified
├── custom-example/               UPSTREAM reference overlay — kept intact
├── .kiro/specs/                  Phases 1 to 4
└── tools/, cmake/, test/         UPSTREAM
```

| Property | Why |
| --- | --- |
| Overlay named `aerora`, not `custom` | Continuous integration proof-builds an overlay **without renaming** the directory, so `custom-example` stays intact as a working reference |
| Both contract files present | `CMakeLists.txt:65-72` raises `FATAL_ERROR` without them |
| Upstream and Aerora separated at the top level | The core-edit count stays verifiable by inspection, `AERORA-BUILD-REQ-006` |
| No `upstream/` subdirectory | The fork **is** the upstream tree. A nested copy would double the source and break the overlay contract's relative paths |

The directive's suggested tree included `upstream/`. Source evidence contradicts it: the overlay
contract resolves `${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}` and `add_subdirectory` from the real source
root, so Aerora must be a sibling inside the upstream tree, not a peer of it. The directive permits
this deviation where source evidence indicates a better structure.

## Module conventions

| Convention | Rule |
| --- | --- |
| Class prefix | `Aerora` on every public type, so a grep separates Aerora from upstream |
| Namespace | None. Upstream uses no namespaces; matching the surrounding style wins |
| Logging category | `aerora.<module>` via `QGC_LOGGING_CATEGORY`, never uncategorised |
| QML exposure | `QML_ELEMENT` / `QML_SINGLETON` / `QML_UNCREATABLE`, state via `Q_PROPERTY` |
| Defensive checks | Null-check every `Vehicle*`; no `Q_ASSERT` in production code |
| Fact access | Through the Fact system only; no parallel storage, no shadowing setting |
| Test base class | The nearest fitting upstream base; no new base class |

These follow `CODING_STYLE.md` and the enforced pre-commit hooks. Phase 4 adds no convention that
the repository does not already enforce.
