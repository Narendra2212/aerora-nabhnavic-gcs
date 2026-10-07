# MAVLink Implementation Boundary

Where the protocol stops and Aerora begins. **No dialect file is created in Phase 4 and no identifier
is invented.**

## Four layers, kept distinct

The directive's required separation, with the owner of each.

| Layer | Content | Owner | Status |
| --- | --- | --- | --- |
| **MAVLink transport** | Framing, parsing, channels, signing, routing | Upstream, unchanged | Inherited |
| **MAVLink standard messages** | The 11 class A families Aerora uses unmodified | Upstream, unchanged | Inherited |
| **Aerora future dialect** | 9 candidate class C families, 5 class D gaps | Aerora, **does not exist** | `CONTRACT-ONLY` |
| **Aerora product semantics** | What a value means to painting | L2 profile, **does not exist** | `CONTRACT-ONLY` |

Collapsing the third and fourth is the mistake this table exists to prevent. A paint tank level is a
product semantic carried by a dialect field; the dialect does not know about paint.

## Current standard integration

| Element | Where | Aerora posture |
| --- | --- | --- |
| Protocol entry | `MAVLinkProtocol::messageReceived`, connected at `src/Vehicle/Vehicle.cc:123` | Unchanged |
| Per-vehicle dispatch | `Vehicle::_mavlinkMessageReceived` at `Vehicle.cc:525` | Unchanged |
| Channel budget | 15 channels | A resource budget, R-013, Q-015 |
| Signing | `src/MAVLink/Signing/` | Retained unchanged |
| Command send and acknowledgement | Matched by `(compId, command)`; `kMaxRetryCount` 3 | Unchanged; Aerora adds its own request identity |

## Dialect generation path

What exists in the build today, so the insertion point is known rather than guessed.

| Element | Evidence |
| --- | --- |
| The dialect is a pinned external dependency, fetched and generated at configure time | `.github/build-config.json` pins versions; `CMakeLists.txt` uses CPM with `CPM_SOURCE_CACHE` |
| Generated headers are a build target dependency | `CMakeLists.txt` declares `mavlink_enums`, `mavlink_instance_fields`, `generate_c_headers` as analysis-header dependencies |
| Message definitions are XML, generated to C headers | Standard MAVLink generator behaviour |

### Where an Aerora dialect would live

| Decision | Position | Status |
| --- | --- | --- |
| Repository | An Aerora-controlled dialect repository, **pinned to an immutable commit** | `AERORA-BUILD-REQ-008`; Q-003 for the fork strategy |
| Reference form | A pinned commit hash, never a branch | `AERORA-SEC-REQ-020` |
| Inclusion mechanism | The existing pinned-dependency mechanism, redirected by `CustomOverrides.cmake` | `IMPLEMENTATION-PLANNED` |
| Generation | The existing generator target, unchanged | Inherited |
| Aerora XML location | In the Aerora dialect repository, not in the overlay | Keeps the dialect versionable independently |

Two dependencies currently track moving references — R-011. Pinning them is a release gate, and the
dialect must be pinned from the first commit rather than retrofitted.

### What Phase 4 does not do

| Prohibited | Confirmed not done |
| --- | --- |
| Create a dialect XML file | **None created** |
| Allocate a message identifier | **None allocated** |
| Allocate a command identifier | **None allocated** |
| Define an enumeration member | **None defined** |
| Define a field name, type, unit or range | **None defined** |

## Message registration and routing

| Concern | Mechanism | Aerora use |
| --- | --- | --- |
| Inbound Aerora families reach fact groups | The dispatch loop at `Vehicle.cc:588-590` calls every group's `handleMessage` | **No registration needed.** Every group sees every message and filters by `msgid` |
| Inbound observation or veto | `FirmwarePlugin::adjustIncomingMavlinkMessage` at `Vehicle.cc:564-566` | Observation only |
| A second observation point | `QGCCorePlugin::mavlinkMessage` at `Vehicle.cc:569-571` | Available, unused |
| A vetoed message | `IF-MAV-027`, `IF-MAV-028` | **Must be logged** — `AERORA-MAV-REQ-010` |
| Outbound commands | The existing command queue and acknowledgement tracker | Unchanged; wrapped by the eight-state tracker |

The dispatch loop requiring no registration is worth stating plainly: Aerora fact groups need no
message-table entry, no subscription and no routing configuration. They receive everything and
ignore what they do not recognise. That is why the six fact-group **classes** are implementable
before the dialect exists — only the `handleMessage` bodies wait.

## Signing path

| Element | Aerora position | Status |
| --- | --- | --- |
| MAVLink 2 signing state machine | Retained unchanged | Inherited |
| Key auto-detection | Retained; it verifies the HMAC **before** configuring a channel | Inherited |
| Timestamp persistence | Retained | Inherited |
| Key provisioning | **Random raw keys**, never passphrase-derived | `AERORA-SEC-REQ-016` |
| Key storage at rest | **Unresolved** — F3.1-01 | `BLOCKED` |
| Unsigned-acceptance default | **Not set** — Q-010, Q-011 | `CONTRACT-ONLY` |

Detail in [12-security-implementation.md](12-security-implementation.md).

## Compatibility

| Rule | Rationale | Status |
| --- | --- | --- |
| Add fields using MAVLink extension fields, never by changing existing field meaning | Extension fields are ignored by older parsers; redefinition is silent corruption | `IMPLEMENTATION-PLANNED` |
| An unknown message is ignored without error | Standard behaviour; a newer vehicle must not break an older station | Inherited |
| An unknown enumeration value is presented raw and marked unrecognised | Rendering it as the nearest known value is the worst available behaviour, and the default if nobody specifies otherwise | `IMPLEMENTATION-PLANNED` |
| A dialect version the station does not recognise is flagged, not refused | The station is supervisory | `IMPLEMENTATION-PLANNED` |
| Never reuse an identifier, even for a withdrawn message | A field recording is forever | Process |

The compatibility matrix needs six values from `AERORA-BUILD-REQ-010`: build version, fork point,
dialect commit, settings schema version, applied configuration version, dependency versions. All six
are implementable today; none needs the dialect.

## Test boundary

| Test | Method | Needs the dialect? |
| --- | --- | --- |
| Standard family handling | `INTEGRATION` via MockLink | No |
| Fact group receives and ignores unknown messages | `UNIT` | No |
| Vetoed message is logged | `UNIT` | No |
| Unknown enumeration value presented raw | `UNIT` | No — a synthetic metadata entry suffices |
| Request identity recorded alongside protocol correlation | `UNIT` | No |
| Aerora family parse | `BENCH` | **Yes** |
| Dialect version reporting | `BENCH` | **Yes** |

Five of seven are writable before the dialect exists. The two that are not are `BENCH` precisely
because a mock the Aerora team writes, exercising a dialect the Aerora team writes, verifies only
that the two agree with each other.
