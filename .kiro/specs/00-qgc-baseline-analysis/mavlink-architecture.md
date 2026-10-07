# MAVLink Architecture

How QGroundControl 5.2.0-dev acquires, parses, routes, signs, and sends MAVLink.

## Dialect acquisition

There are **no checked-in MAVLink message headers**. The entire C library is generated at
configure time by upstream mavgen, running inside a CPM-fetched copy of the mavlink repository.

`src/MAVLink/CMakeLists.txt:21-29`:

```cmake
CPMAddPackage(
    NAME mavlink
    GIT_REPOSITORY ${QGC_MAVLINK_GIT_REPO}
    GIT_TAG ${QGC_MAVLINK_GIT_TAG}
    CUSTOM_CACHE_KEY "${_mavlink_cache_key}"
    OPTIONS "MAVLINK_DIALECT ${QGC_MAVLINK_DIALECT}" "MAVLINK_VERSION ${QGC_MAVLINK_VERSION}"
    PATCHES "${_mavlink_patch}"
)
```

The dialect is passed *into* the upstream repository's own CMake, which invokes mavgen. Include
paths are `${mavlink_BINARY_DIR}/include/mavlink` and
`${mavlink_BINARY_DIR}/include/mavlink/${QGC_MAVLINK_DIALECT}`
(`src/MAVLink/CMakeLists.txt:60-62`). Message definition XML lives at
`${mavlink_SOURCE_DIR}/message_definitions/v1.0`.

`PATCHES` applies `src/MAVLink/mavlink-deterministic-mavgen.patch` to make mavgen output
deterministic. Because CPM's default source-cache key does not hash patch contents,
`qgc_cpm_patch_cache_key()` computes an explicit `CUSTOM_CACHE_KEY`
(`src/MAVLink/CMakeLists.txt:34-52`).

Controlling variables, all `CACHE STRING` and therefore overridable by a vendor overlay:

| Variable | Default |
| --- | --- |
| `QGC_MAVLINK_GIT_REPO` | `https://github.com/mavlink/mavlink.git` |
| `QGC_MAVLINK_GIT_TAG` | `c409cf690454db6d3e004bd14173bc6c7ff1e0ff` |
| `QGC_MAVLINK_DIALECT` | `all` |
| `QGC_MAVLINK_VERSION` | `2.0` |

The `all` dialect is a superset including common and ArduPilotMega. Corroborating evidence in
code: `src/Vehicle/Vehicle.cc` around line 710 handles `MAVLINK_MSG_ID_CAMERA_FEEDBACK`,
`LOG_ENTRY`, and `LOG_DATA` under a comment marking them as ArduPilot dialect messages.

### Adding an Aerora dialect

There is **no** dedicated custom-dialect CMake option and **no** vendor-XML drop-in directory.
A search for `CUSTOM_DIALECT` and equivalent injection mechanisms found only
`QGC_MAVLINK_DIALECT`. Two supported routes exist, neither requiring a core-file edit:

1. **Repoint the pin.** Set `QGC_MAVLINK_GIT_REPO` to an Aerora fork of the mavlink repository
   containing Aerora XML, `QGC_MAVLINK_GIT_TAG` to a pinned commit, and `QGC_MAVLINK_DIALECT` to
   the Aerora dialect name. Both QGC-side generators and the include path follow
   `QGC_MAVLINK_DIALECT`, so Aerora messages, enums, and QML enum bindings all come through
   automatically.
2. **Set those variables from `CustomOverrides.cmake`.** The overlay is included before
   `add_subdirectory(src)`, so overrides reach the MAVLink module.

Route 1 driven from route 2 is the recommended approach, recorded as `PROPOSED` in
[aerora-extension-map.md](aerora-extension-map.md).

### Generated artefacts

Three QGC-side generators run on top of the CPM headers.

| Generator | Output | Purpose |
| --- | --- | --- |
| `tools/generators/mavlink_enums.py` | `MAVLinkEnums.h`, `MAVLinkEnumsQml.{h,cc}` | Enum typedefs without pack/unpack code, plus `Q_NAMESPACE`/`Q_ENUM_NS` so every MAVLink enum is QML-visible |
| `tools/generators/mavlink_instance_fields.py` | `MAVLinkInstanceFields.h` | Message id to instance-field name map, parameterised by dialect |
| CMake resource step | `/json/component_metadata` | Bundles upstream `component_metadata/*.schema.json` so metadata validates against the pinned tag |

Library-level configuration sits in two small headers:

- `src/MAVLink/MAVLinkLib.h` — defines `MAVLINK_USE_MESSAGE_INFO`, includes `<mavlink.h>`.
  `MAVLINK_NO_SIGN_PACKET` and `MAVLINK_NO_SIGNATURE_CHECK` are present but **commented out**
  (lines 4-5), so signing is compiled in.
- `src/MAVLink/MAVLinkMessageType.h` — `MAVLINK_COMM_NUM_BUFFERS 16` (line 33),
  `MAVLINK_MAX_SIGNING_STREAMS` (34), the `mavlink_channel_t` enum `MAVLINK_COMM_0..15` (13-31),
  `MAVLINK_EXTERNAL_RX_STATUS` (38), `MAVLINK_GET_CHANNEL_STATUS` (44). This header deliberately
  avoids pulling the very large dialect header into moc.

## Protocol core

`MAVLinkProtocol` (`src/Comms/MAVLinkProtocol.{h,cc}`) is an application-static singleton
(`Q_APPLICATION_STATIC` at `.cc:29`). It is the byte-to-message parser, telemetry logger, loss
statistics accumulator, and forwarding point. It is **not** a router.

### Parse loop

`MAVLinkProtocol::receiveBytes(LinkInterface*, const QByteArray&)` at `.cc:102-152`, a slot fed
by `LinkInterface::bytesReceived`:

1. Re-resolve a `SharedLinkInterfacePtr` through
   `LinkManager::sharedLinkInterfacePointerForLink(link)` and bail if the link already died
   (105-109). This is the guard for queued data that arrives after disconnect.
2. Loop byte at a time: `mavlink_parse_char(mavlinkChannel, byte, &message, &status)` (line 115).
3. On `MAVLINK_FRAMING_OK` or `MAVLINK_FRAMING_BAD_SIGNATURE`, hand the frame to the per-link
   `SigningController::processFrame()`. A `true` return means a key was auto-detected, which
   triggers `resetSequenceTracking(link)` so the key-install gap is not counted as packet loss
   (117-123).
4. Drop MAVLink v1 frames except HEARTBEAT and RADIO_STATUS (131-140). The rationale is in the
   code: v1 and v2 share per-(sysid, compid) sequence counters, so counting v1 makes every v2
   message appear lost. RADIO_STATUS is exempt because SiK radios always frame it as v1.
5. Count loss, forward, log, then `_updateStatus()` which emits `messageReceived`.

### Per-link parse state

The `mavlink_status_t` local in `receiveBytes` is only an out-parameter copy. Real state is the
channel-indexed global array QGC exports: `mavlink_status_t
m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS]` and `mavlink_get_channel_status(uint8_t)` in
`src/MAVLink/QGCMAVLink.cc:48-58`, declared `extern` in `src/MAVLink/MAVLinkMessageType.h:38-46`.
Parse and signing state are therefore keyed purely by channel index.

`MAVLinkProtocol`'s own per-channel state (`MAVLinkProtocol.h:85-92`):
`_lastIndex[MAVLINK_COMM_NUM_BUFFERS][256][256]` (last sequence per channel/sysid/compid),
`_firstMessageSeen[...]`, `_totalReceiveCounter`, `_totalLossCounter`, `_runningLossPercent`.

### Channel allocation

`LinkManager::allocateMavlinkChannel()` (`src/Comms/LinkManager.cc:727-745`) scans
`0..MAVLINK_COMM_NUM_BUFFERS-1` against the bitmask `_mavlinkChannelsUsedBitMask`, which is
**initialised to `1`** (`LinkManager.h:144`). Channel 0 is permanently reserved, so **15 of 16
channels are allocatable**. On allocation it sets `MAVLINK_STATUS_FLAG_OUT_MAVLINK1` on the
channel status. Exhaustion returns `invalidMavlinkChannel()` =
`std::numeric_limits<uint8_t>::max()` and logs a warning.

Per-link hookup happens in `LinkManager::createConnectedLink`, which calls
`link->_allocateMavlinkChannel()` (`LinkManager.cc:188`) and frees it on failure or disconnect.
`LinkInterface::_allocateMavlinkChannel()` is virtual (`LinkInterface.h:85`) with a default of
one channel; `MockLink` overrides it to take two. The implementation
(`LinkInterface.cc:91-120`) calls `mavlink_set_proto_version(_mavlinkChannel, MAVLINK_VERSION)`
and constructs the `SigningController` for that channel.

One further channel is consumed by ArduPilot: `APMFirmwarePlugin::_reencodeMavlinkChannel()`
lazily allocates a dedicated channel for v1 re-encoding
(`src/FirmwarePlugin/APM/APMFirmwarePlugin.cc:1232-1256`).

**Aerora consequence.** The 15-channel budget is a hard ceiling on simultaneous links per QGC
instance. A multi-transport Aerora aircraft consuming several concurrent links per vehicle, across
a fleet, could approach it. Recorded in [risk-register.md](risk-register.md).

## Routing

**There is no central router class.** Dispatch is fan-out-and-filter.

| Step | Location |
| --- | --- |
| Transport emits `bytesReceived(LinkInterface*, const QByteArray&)` | `LinkInterface.h:71` |
| `LinkManager` wires it to the protocol singleton | `LinkManager.cc:196` |
| `MAVLinkProtocol::receiveBytes` parses | `MAVLinkProtocol.cc:102` |
| `_updateStatus()` emits `messageReceived(LinkInterface*, const mavlink_message_t&)` | `MAVLinkProtocol.h:49` |
| **Every** `Vehicle` is connected to that signal | `Vehicle.cc:123` |
| Each `Vehicle` self-filters on system id | `Vehicle.cc:525-533` |

The filter admits a message when `message.sysid == _systemID` or `sysid == 0`, with one
exception: `RADIO_STATUS` from a link the vehicle uses is let through, gated on
`_vehicleLinkManager->containsLink(link)`. Component id is **not** used for ownership filtering;
`_compID` is latched from the first HEARTBEAT only for loss statistics (`Vehicle.cc:538-546`),
and subsystems perform their own component-id checks.

### Vehicle creation

The other half of routing. `MAVLinkProtocol::_logData` decodes HEARTBEAT, HIGH_LATENCY, and
HIGH_LATENCY2 and emits `vehicleHeartbeatInfo(link, sysid, compid, autopilot, type)`
(`MAVLinkProtocol.h:46`, emitted at `.cc:250`, `258`, `265`).
`MultiVehicleManager::_vehicleHeartbeatInfo(...)` (`MultiVehicleManager.cc:68-130`, connected at
line 58) applies admission filters and constructs the `Vehicle` at `MultiVehicleManager.cc:115`.
Full detail in [vehicle-architecture.md](vehicle-architecture.md).

### Per-vehicle link bookkeeping

`Vehicle::_mavlinkMessageReceived` gives `VehicleLinkManager::mavlinkMessageReceived(link,
message)` first access, because it is responsible for adding newly observed links
(`Vehicle.cc:535`, implementation `VehicleLinkManager.cc:35`).

### Forwarding between links

Two independent paths in `MAVLinkProtocol`, both called from `receiveBytes` only when the
receiving link is not itself a forwarding target — `!linkPtr->linkConfiguration()->isForwarding()`
at `.cc:143-146`, which is the loop-prevention guard.

| Path | Gate | Target |
| --- | --- | --- |
| `_forward()` (`.cc:189-208`) | `MavlinkSettings::forwardMavlink()` | `LinkManager::mavlinkForwardingLink()` |
| `_forwardSupport()` (`.cc:210-227`) | `LinkManager::mavlinkSupportForwardingEnabled()` | `LinkManager::mavlinkForwardingSupportLink()` |

Both forward via `MAVLinkSigning::serializeUnsignedCopy(message)` — the signature is stripped and
the CRC recomputed, so a downstream signing-aware parser does not see `BAD_SIGNATURE` from a
foreign key. Both refuse to forward `SETUP_SIGNING` (`.cc:192`, `.cc:213`).

### Outbound link selection

Callers choose the link explicitly. The convention throughout the codebase is
`vehicleLinkManager()->primaryLink().lock()` followed by
`Vehicle::sendMessageOnLinkThreadSafe(LinkInterface*, mavlink_message_t)`
(`Vehicle.h:441`, implementation `Vehicle.cc:1392-1407`), which checks `link->isConnected()`,
gives `FirmwarePlugin::adjustOutgoingMavlinkMessageThreadSafe` a chance to mutate the message,
then calls the **single send chokepoint** `LinkInterface::sendMessageThreadSafe(mavlink_message_t&)`
(`LinkInterface.h:48`, implementation `LinkInterface.cc:148`). The code comment is explicit that
all outbound sends must route through there so signing cannot be bypassed.

The GCS heartbeat is the deliberate exception — it fans out to all connected, normal-latency
links via `link->sendMessageThreadSafe(message)` (`MultiVehicleManager.cc:297`).

## Message signing

MAVLink 2 message signing is fully implemented, with a per-link state machine, a key store, and
key auto-detection. Everything lives in `src/MAVLink/Signing/`, which has its own CMakeLists
added at `src/MAVLink/CMakeLists.txt:141`.

| File | Role |
| --- | --- |
| `MAVLinkSigning.{h,cc}` | Primitives: `SigningKey = std::array<uint8_t,32>`, signing epoch 2015-01-01 UTC, `currentSigningTimestampTicks()` in 10 microsecond ticks, `UnsignedAcceptancePolicy{Strict, Pending}`, `createSetupSigning`/`encodeSetupSigning`, `isMessageSigned`/`setMessageSigned`, `serializeUnsignedCopy`, `verifySignature`, `signMessage` |
| `SigningChannel.{h,cc}` | Per-channel `mavlink_signing_t` state behind a read-write lock |
| `SigningController.{h,cc}` | The per-link class, one per allocated channel |
| `MAVLinkSigningKeys.{h,cc}` | Key store, QML-exposed singleton |
| `SigningFailure.h`, `SigningStatus.h` | Failure reasons and state enum |
| `src/Vehicle/VehicleSigningController.cc` | Per-vehicle driver |

### SigningController

Owned by `LinkInterface` as `std::unique_ptr<SigningController> _signingController`, created in
`_allocateMavlinkChannel()` (`LinkInterface.cc:114-115`), exposed as `link->signing()`
(`LinkInterface.h:66-68`), and reset **before** the channel is freed so the final timestamp can be
flushed (`LinkInterface.cc:132`).

API: `tryBeginEnable(expectedSysId, keyName, keyBytes)` and `tryBeginDisable(expectedSysId)`, both
returning `std::optional<SigningFailure>`; `cancelPending`, `clearSigning`, `initSigningImmediate`,
`signOutgoing`, `processFrame(framingOk, message)`. Signals: `signingConfirmed`, `signingFailed`,
`keyAutoDetected`, `alertRaised`, `stateChanged`.

Constants: `kTimeout = 5s` for deferred vehicle confirmation (test-overridable),
`kBadSignatureAlertThreshold = 3` for burst alerting via an edge-triggered counter, and
`kWallClockRefreshInterval = 1s` for a timer that catches the signing timestamp up to wall clock.
That last one exists because libmavlink only bumps the timestamp per packet, so idle outbound
paths would otherwise sign with a stale value and be rejected by peers that pin signing timestamps
to wall clock — the code cites `mavlink/qgroundcontrol#14375`.

### Key storage

`MAVLinkSigningKeys` (`src/MAVLink/Signing/MAVLinkSigningKeys.h`) is the key bag.

| Property | Value |
| --- | --- |
| Derivation | PBKDF2-HMAC-SHA256 via `QPasswordDigestor` |
| Iterations | `kPbkdf2Iterations = 600000` — stated as the OWASP 2023+ floor |
| Salt | `kPbkdf2Salt = "QGroundControl-MAVLink-Signing-v1"` — **fixed, app-wide** |
| Key size | 32 bytes |
| Max keys | `kMaxKeys = 64` |
| Min passphrase | `kMinPassphraseLength = 8` |
| Persistence | `QSettings`, group `MAVLinkSigningKeys`, subgroups `keys/<name>` and `timestamps/<name>`, plus a names-only `manifest` |
| Flush interval | `kTimestampFlushIntervalMs = 5000` |

The fixed salt is deliberate: it keeps passphrase-to-key derivation deterministic across installs
so multiple GCS stations can share one vehicle. The security consequence is analysed in
[security-architecture.md](security-architecture.md).

`tryDetectKey(controller, message)` brute-forces every stored key against an incoming signature,
which is how auto-detection works. `generateRandomHexKey()` provides cryptographically random
keys, `addRawKey(name, hexKey)` imports them, and passphrase buffers are wiped with
`QGC::secureZero`.

### Protocol hygiene

Verified in code: `SETUP_SIGNING` is never forwarded (`MAVLinkProtocol.cc:192`, `213`) and never
written to telemetry logs (`.cc:232-235`, with a comment noting it contains the secret key).
Logged packets are signature-stripped via `serializeUnsignedCopy`.

### Per-vehicle driver

`VehicleSigningController` follows `primaryLinkChanged`, calls `tryBeginEnable`/`tryBeginDisable`,
and sends `SETUP_SIGNING` **twice** on the primary link for redundancy over a lossy link
(around lines 193-196), resetting `MAVLinkProtocol::resetSequenceTracking` on transitions.

## Command layer

`MavCommandQueue` (`src/Vehicle/MavCommandQueue.{h,cc}`) owns the COMMAND_LONG/COMMAND_INT send,
retry, and acknowledgement pipeline — one instance per `Vehicle` (`Vehicle.cc:262`).

### Public API

`sendCommand`, `sendCommandDelayed`, `sendCommandInt`, `sendCommandWithHandler`,
`sendCommandIntWithHandler`, `sendCommandWithLambdaFallback`,
`sendCommandIntWithLambdaFallback`, plus the full-control `sendWorker(...)`
(`MavCommandQueue.h:31-49`). `Vehicle::sendMavCommand*` are thin delegates
(`Vehicle.cc:2147-2194`).

### Queue entry and retry policy

`MavCommandListEntry_t` (`MavCommandQueue.h:80-101`) carries target component id, a commandInt
flag, frame, seven parameters, `showError`, an ack handler info struct, `maxTries`, `tryCount`, a
`QElapsedTimer`, and `ackTimeoutMSecs`.

| Constant | Value |
| --- | --- |
| `kMaxRetryCount` | 3 |
| `_ackTimeoutMSecsDefault` | 1200 ms |
| `_ackTimeoutMSecsHighLatency` | 120000 ms |

`maxTries = _shouldRetry(command) ? kMaxRetryCount : 1` (`.cc:350`) — there is an allow-list of
retryable commands in `_shouldRetry` (`.cc:219+`). The timeout is selected per link via
`sharedLink->linkConfiguration()->isHighLatency()` (`.cc:351`).

`_sendFromList(index)` (`.cc:360-452`) packs COMMAND_INT or COMMAND_LONG and ends at
`_vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg)`. A periodic
`_responseTimeoutCheck()` slot (`.cc:454-504`) resends entries whose ack window expired until
`maxTries`.

### Acknowledgement

`Vehicle::_handleCommandAck(mavlink_message_t&)` (`Vehicle.cc:2210+`, dispatched from the
`MAVLINK_MSG_ID_COMMAND_ACK` case at line 646) decodes the ack, logs it using
`MissionCommandTree::instance()->rawName()`, applies vehicle-level side effects that must fire
regardless of queue-match state (ROI enable/disable, `PREFLIGHT_STORAGE`, `FLASH_BOOTLOADER`),
then delegates to `_mavCmdQueue->handleCommandAck(message, ack)` at line 2247.

`MavCommandQueue::handleCommandAck` matches by `(message.compid, ack.command)` through
`findEntryIndex`, and on `MAV_RESULT_IN_PROGRESS` refreshes the entry's timer instead of
completing it. Terminal acks without a user-supplied result handler are reported through the
`commandResult(vehicleId, targetComponent, command, ackResult, failureCode)` signal.
`stop()` clears the queue without firing callbacks during vehicle teardown
(`Vehicle.cc:396`, `470`).

### Command metadata

`MissionCommandTree` builds a firmware-class by vehicle-class hierarchy of `MissionCommandList`
objects from JSON. Covered in [mission-architecture.md](mission-architecture.md), including the
important constraint that **override files cannot introduce new MAV_CMD entries**.

## Extension seams

Ordered by where they sit in the inbound path. All are reachable without editing core files
except where noted.

| Seam | Location | Contract |
| --- | --- | --- |
| `FirmwarePlugin::adjustIncomingMavlinkMessage(Vehicle*, mavlink_message_t*)` | `FirmwarePlugin.h:285`, called `Vehicle.cc:565` | Virtual, default `true`. Returning `false` skips the message. Pointer is mutable, so in-place rewrite is supported |
| `QGCCorePlugin::mavlinkMessage(Vehicle*, LinkInterface*, const mavlink_message_t&)` | `QGCCorePlugin.h:136`, called `Vehicle.cc:569` | Virtual, default `true`. Returning `false` stops QGC processing. **This is the designed vendor seam for custom messages** |
| `MAVLinkProtocol::messageReceived(LinkInterface*, const mavlink_message_t&)` | `MAVLinkProtocol.h:49` | Signal. Raw, all-traffic, pre-Vehicle-dispatch. Precedent consumer: `MAVLinkInspectorController.cc:46` |
| `MAVLinkProtocol::vehicleHeartbeatInfo(...)` | `MAVLinkProtocol.h:46-48` | Signal |
| `MAVLinkProtocol::mavlinkMessageStatus(...)` | `MAVLinkProtocol.h:52-53` | Signal, loss statistics |
| `Vehicle::mavlinkMessageReceived(const mavlink_message_t&)` | `Vehicle.h:756`, emitted `Vehicle.cc:757` | Signal, emitted **last**, after vehicle state is updated. Used by QGC's own subsystems |
| `FirmwarePlugin::adjustOutgoingMavlinkMessageThreadSafe(...)` | `FirmwarePlugin.h:296` | Outbound mutation hook, default no-op |
| `FirmwarePlugin::factGroups()` | `FirmwarePlugin.h` | Inject vendor FactGroups; `Vehicle.cc:585-589` runs `handleMessage` on each |
| `FirmwarePlugin::missionCommandOverrides(vehicleClass)` | `FirmwarePlugin.cc:95-115` | Supply vendor MAV_CMD metadata JSON |
| `QGC_MAVLINK_*` via `CustomOverrides.cmake` | `cmake/CustomOptions.cmake` | Dialect substitution |

### Recommended Aerora seam

For Aerora-specific messages — painting payload state, tether and power telemetry, GPU and
autonomy status — the recommended primary seam is `QGCCorePlugin::mavlinkMessage`, because it sees
all traffic for a vehicle, can veto processing, and requires no core edit. For messages that should
become bindable telemetry, `FirmwarePlugin::factGroups()` is the correct companion so values flow
through the Fact system rather than bespoke storage. Both are `PROPOSED`, pending ratification.

### What does not exist

Searched for and not found:

- No runtime or dynamic plugin loading for MAVLink handlers.
- No message-id to handler registration table or API.
- No `QPluginLoader`-based seam anywhere in the product.
- No per-message custom handler list in `MAVLinkProtocol`; the only seam there is the
  `messageReceived` signal.

`QGCCorePlugin` substitution is compile-time only. One custom plugin per binary.
