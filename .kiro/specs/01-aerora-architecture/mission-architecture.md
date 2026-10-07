# Mission Architecture

Mission planning, upload, execution monitoring and resume for Aerora, with the full analysis of the
resume filter that Phase 1 recorded as R-002 and the plan-file authority decision that follows from
it.

## Summary of the position

| Decision | Statement |
| --- | --- |
| Plan authority | The saved Aerora plan file is authoritative. The vehicle mission store is a derived artifact |
| Payload state across resume | **Preferred:** retained by the payload controller, not replayed by the ground station. **Evidence status: `NOT-EVIDENCED`** — a vehicle-side verification requirement, not an established behaviour |
| QGroundControl core | Unmodified. The resume filter is not patched |
| R-002 severity | Reduced from `CRITICAL` to `HIGH`, bounded to the replay prefix |

Recorded as ADR-005.

## The resume path, as it actually works

### Trace

```text
GuidedActionsController.qml                    operator triggers resume
        │
        ▼
MissionController::resumeMission(index)         MissionController.cc:1854-1860
        │   index from resumeMissionIndex       MissionController.cc:1735-1751
        ▼
MissionManager::generateResumeMission(resumeIndex)   MissionManager.cc:71-211
        │
        ├── if inProgress(): return                  :77-80    silent, qCDebug only
        ├── scan whole mission for MAV_CMD_DO_JUMP   :82-88    refuse if found
        ├── read _missionItems[resumeIndex]          :94       unguarded index
        ├── build includedResumeCommands             :110-126  14 commands, LOCAL variable
        ├── filter loop                              :134      see below
        ├── de-duplicate the replay prefix           :145-194
        ├── renumber densely 0..N-1                  :196-202
        └── write the result to the vehicle          :204-210
                │
                ▼
PlanManager::_writeMissionItemsWorker               PlanManager.cc:38-57
        │
        ▼
PlanManager::_finishTransaction                     PlanManager.cc:809-867
```

### The filter

The retention test at `MissionManager.cc:134` keeps an item if any of four conditions holds:

| Condition | Effect |
| --- | --- |
| `i == 0 && addHomePosition` | Keeps the home position |
| `i >= resumeIndex` | Keeps **everything** at or after the resume point |
| `includedResumeCommands.contains(command)` | Keeps a listed command from before the resume point |
| the item is a takeoff command | Keeps takeoff |

There is no `else`. An unlisted command before the resume index is dropped with no log line, no
warning, no counter and no operator notification.

### What the list actually is

`includedResumeCommands` is a plain local automatic variable built at `MissionManager.cc:110-126`.
It is not static, not const, not a member, and not reachable from outside the function. It contains
fourteen commands, all camera, region-of-interest or speed related.

The header's documentation comment says the generated mission includes "all the CMD_DO items",
which the code does not do. The comment is stale, and the discrepancy is informative: it reads like
a list that was exhaustive when written and was never revisited. It is an **allow-list that has
fallen behind**, not a deny policy that excludes vendor commands by design.

### The de-duplication tail proves the intent

`MissionManager.cc:145-194` reduces the replay prefix, and the reduction rules are the clearest
evidence of what the whole mechanism is for:

| Rule | Interpretation |
| --- | --- |
| Latest-wins for camera mode and region of interest | Only the final state matters, not the history |
| One shared slot across five camera start/stop commands | They are mutually exclusive states of one camera |
| Image capture with a non-zero third parameter is deleted, as a non-time-triggered command | A one-shot action is not state and must not be re-fired |
| Commands not matched by any rule replay every occurrence | No rule was written for them |

This is **state restoration**, not command replay. The mechanism exists to leave the camera and the
speed where they were, because QGroundControl assumes the vehicle retains nothing across a mission
replacement.

## Why there is no seam

Four facts, each independently sufficient:

| Fact | Location |
| --- | --- |
| `generateResumeMission` is not virtual | `MissionManager.h:31` |
| `MissionManager` declares no virtual members at all | `MissionManager.h` |
| `Vehicle` constructs the concrete `MissionManager` | `Vehicle.cc:243` |
| No `QGCCorePlugin` or `FirmwarePlugin` hook exists on this path | Searched; none |

So there is no subclass substitution, no factory, and no plugin hook. Changing the filter means
editing QGroundControl core and carrying that edit against upstream indefinitely, on a path that
has no test coverage for vendor commands because upstream has no vendor commands.

## Severity, corrected

Phase 1 recorded R-002 as `CRITICAL`. That was too high, and the correction comes from the second
filter condition.

| Case | Outcome |
| --- | --- |
| Aerora command **at or after** the resume index | Survives untouched — `i >= resumeIndex` |
| Aerora command **before** the resume index | Dropped silently |

Only the replay prefix is affected. The mission from the resume point onward is reproduced
faithfully, parameters untouched: items are deep-copied and only `setIsCurrentItem` and
`setSequenceNumber` are mutated. The exposure is therefore "payload state established earlier in
the mission is lost", not "the resumed mission is wrong".

That is still serious — it is why the risk stays `HIGH` and is gated at `G-ARCH` in
[phase-2-risk-gates.md](phase-2-risk-gates.md) — but it is a bounded, well-understood exposure with
a clean architectural answer.

## The proposed resolution

**Preferred architecture: the payload controller retains payload state across a mission
interruption.** If it does, the replay prefix has nothing Aerora-specific to carry and the gap
closes without touching QGroundControl.

```text
QGC assumption:         vehicle retains nothing   →  GCS must replay state
Aerora preferred model: payload controller retains state  →  nothing to replay
Evidence status:        UNVERIFIED — see the caveat below
```

### This is a requirement on the vehicle, not an established fact

Phase 2 asserted that the retention behaviour was already true of Aerora and that QGroundControl's
premise was therefore "false for Aerora". **Phase 2.1 withdraws that assertion.** It was a
derivation presented as a finding, and it does not survive checking:

| Checked | Result |
| --- | --- |
| Does the v4.1 baseline state that payload state is retained across a mission interruption? | **No.** Baseline document 10 contains no statement about retention, persistence, non-volatile storage, or survival across resume |
| Does the baseline assign payload-local safety and interlocks to the payload controller? | Yes — baseline 13. That is an **ownership** statement, not a retention statement |
| Does the baseline require payload state to be reported as telemetry? | Yes — baselines 07 and 11 |
| Does the firmware model show a payload state machine with persistence? | **No.** The payload elements observed are `Payload_Interface`, `Painting_System` and `Payload_Actuator_Drivers`, with placeholders for the actuator behaviours |
| Has any bench or flight evidence been produced? | **None observed** |

Ownership does not imply retention. A payload controller can own payload state and still lose it on
a power cycle, a reset, a mode change, or a mission replacement. Whether it retains it is a firmware
design decision that has not been made, let alone verified.

Baseline 13 states the discipline directly: where the firmware model contains a placeholder, the
item is unresolved engineering work rather than a verified capability. The retention claim falls
under that rule.

### What this requires of the system

| Requirement | Owner | Evidence status |
| --- | --- | --- |
| Payload state survives a mission replacement | Payload controller firmware | **`NOT-EVIDENCED`** — must be designed, specified and bench-verified |
| The retained state set is enumerated, so "retained" has a defined meaning | Payload controller firmware | `NOT-EVIDENCED` — depends on Q-023 |
| Payload state is reported, so the GCS can show what it is after resume | Payload controller firmware | `BASELINE-ASSERTED` — reporting is required; content is open |
| Aerora mission items that set payload state declare their resume behaviour | Dialect, per `AERORA-MAV-REQ-008` | `NOT-EVIDENCED` — dialect not yet written |
| The operator is told that resume reproduces the mission from the resume point, not the full history | GCS presentation | `GCS-DECIDED` — within Aerora's control |

Only the last row is within the ground station's control. The first four are vehicle-side
obligations that Phase 2.1 records in the
[hardware and firmware evidence matrix](hardware-firmware-evidence-matrix.md) as items requiring
evidence before R-002 can be considered mitigated.

### If retention is not implemented

The architecture must not assume its preferred answer will be adopted. If the payload controller
does not retain state, the exposure returns, and the options are:

| Option | Assessment |
| --- | --- |
| Constrain Aerora plans so payload-state commands never precede a plausible resume point | A planning restriction, verifiable in the plan editor. No core edit. **Preferred fallback** |
| Have the GCS warn, at resume time, that payload state set before the resume point will not be re-established | Presentation only. Honest, and cheap. **Adopt regardless** |
| Re-open the core-edit option for the resume filter | Still rejected — the path has no seam and no upstream test coverage |
| Replay payload state from the GCS | Still rejected — puts authoritative state in the ground station |

The two rejections are unchanged by this reclassification. What changes is that the preferred
resolution now has a named fallback and an evidence gate, instead of resting on an assumption.

The last row is a user-interface obligation that falls out of an architectural decision, and it is
easy to forget. An operator who believes resume restores everything will be surprised exactly once,
at the worst moment.

### Alternatives rejected

| Rejected | Reason |
| --- | --- |
| Patch `includedResumeCommands` to add Aerora commands | A core edit on a seamless path, carried forever, untested upstream |
| Encode payload state in the parameters of an allow-listed command | Makes the wire protocol dishonest and the saved plan unreadable |
| Model payload state in the GCS and replay it from there | Puts authoritative state in the ground station |
| Contribute the fix upstream and wait | Upstream has no vendor commands; the change has no upstream motivation |

An upstream contribution adding a log line when an item is dropped **is** worth offering — it costs
nothing and makes the behaviour diagnosable for everyone. It is listed as optional in
[risk-gates.md](risk-gates.md) and Aerora does not depend on it.

## Plan file authority

### The decision

The saved Aerora plan file is the authoritative record of mission intent. The vehicle mission store
is a derived artifact that may be replaced, truncated or renumbered by operations outside Aerora's
control.

### Why the vehicle store cannot be authoritative

| Fact | Consequence |
| --- | --- |
| `MissionItem` has no vendor-extension slot — only command, frame, seven parameters, autoContinue, sequence and jump id | Aerora metadata cannot round-trip through the vehicle |
| Complex items are flattened to primitive items once, on upload, by `MissionController::_convertToMissionItems` (`MissionController.cc:205-233`) | A downloaded mission is the flattened form; the original structure is gone |
| Resume replaces the entire vehicle mission and resets the current index, so the resume button disappears afterwards | The vehicle store after a resume is not the planned mission |
| A partial upload failure leaves the vehicle store indeterminate — `PlanManager::_finishTransaction` discards QGC's write list without rolling the vehicle back | The vehicle store can be in a state no plan file describes |

The first row is the fundamental one, and it is a property of MAVLink's mission representation
rather than anything QGroundControl chose. This is why R-012 was downgraded from `MEDIUM` to `LOW`:
it is not an Aerora-specific defect and declaring plan-file authority resolves it.

### Rejected: an Aerora mission scanner

Implementing an Aerora equivalent of QGroundControl's complex-item scanner — reconstructing Aerora
structure from a downloaded flat mission — was considered and rejected. It would be inference
dressed as fact: a flat item sequence that happens to match an Aerora pattern is not evidence that
the pattern was what the operator planned. The plan file already holds that answer with certainty.

### What this means in practice

| Operation | Behaviour |
| --- | --- |
| Plan, save, upload | Plan file written first, then uploaded |
| Download from vehicle | Permitted, presented as a flat mission, clearly not an Aerora plan |
| Verify | Compare the uploaded flat form against the plan file's flattened form |
| Post-flight reconstruction | From the plan file plus the log, never from the vehicle store |
| Mission audit record | Keyed on the plan file identity, which is in turn keyed on the Aerora vehicle identifier |

## Latent defects recorded

These are upstream defects on the resume path, found during the R-002 analysis. None blocks the
Aerora architecture; all are worth knowing before anyone touches this code.

| Defect | Location |
| --- | --- |
| `_missionItems[resumeIndex]` indexed with no empty-list or range guard | `MissionManager.cc:94` |
| The current-index write is unguarded | `MissionManager.cc:202` |
| The locally built resume items are never deleted — one leak per resume | `MissionManager.cc:71-211` |
| A resume requested while a transaction is in progress is dropped with only a debug log | `MissionManager.cc:77-80` |
| The `DO_JUMP` refusal reports only through an application message, emitting no signal | `MissionManager.cc:82-88` |
| `resumeMissionReady` has no consumer anywhere in the source tree | Searched; none |

The fourth is the one most likely to be seen in the field: an operator who triggers resume during
an in-flight transaction gets no feedback at all.

## Aerora mission content

### Complex items

Aerora product-specific mission structures — the painting pattern being the obvious one — are
complex items in the L2 application layer, following the pattern `custom-example` demonstrates with
its own complex item and plan creator. They flatten to primitive mission items on upload, like
every other complex item.

| Element | Layer | Attachment |
| --- | --- | --- |
| Aerora complex item class | L2 | Registered in the overlay's source set |
| Aerora plan creator | L2 | Registered in the overlay's source set |
| Map visual and editor QML | L1 | Overlay QML module |
| Mission command set | L3 | `FirmwarePlugin::supportedMissionCommands()` |

The architecture does not specify which complex items exist. The painting pattern's geometry and
parameters depend on Q-023 and on product definition, and inventing them here would repeat the
mistake [payload-architecture.md](payload-architecture.md) declines to make.

### Mission command set

`FirmwarePlugin::supportedMissionCommands()` (`FirmwarePlugin.h:309`) returns an empty list to mean
"all commands supported". Aerora returns an explicit list, because an explicit list is what makes
the Plan view's command picker match what the vehicle can actually do, and because an empty list
offers the operator commands the Aerora firmware may not implement.

This is `OPEN` pending Q-023.

## Monitoring mission execution

| Observed | Source |
| --- | --- |
| Current mission index | Vehicle-reported |
| Mission progress | Derived from the current index against the plan file |
| Mission state | Aerora mission status, where the dialect provides it |
| Resume availability | QGroundControl's existing logic, unchanged |

The GCS does not infer that a mission item completed. It reports the index the vehicle reports. An
item the GCS believes should have completed but which the vehicle has not advanced past is a
condition to surface, not to smooth over.

## Core edits required

**None.** Complex items, plan creators and the mission command set all attach through existing
registration and existing virtuals. The resume filter is not touched, by decision.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-023 | Payload actuator model, which determines the Aerora mission command set | `ARCH-SHAPING` |
| Q-021 | Whether the Aerora pattern needs survey-style insert behaviour | `DEFERRABLE` |
| Q-041 | Mission verification depth | `DEFERRABLE` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-MISSION-REQ-001` | The saved Aerora plan file shall be the authoritative record of mission intent |
| `AERORA-MISSION-REQ-002` | The GCS shall not reconstruct Aerora mission structure from a mission downloaded from the vehicle |
| `AERORA-MISSION-REQ-003` | Mission resume shall not depend on the GCS replaying payload-state commands |
| `AERORA-MISSION-REQ-004` | The operator shall be informed that resume reproduces the mission from the resume point and not the full mission history |
| `AERORA-MISSION-REQ-005` | The Aerora mission command set shall be declared explicitly rather than by the empty-list default |
| `AERORA-MISSION-REQ-006` | The GCS shall not infer mission item completion; it shall present the vehicle-reported mission index |
| `AERORA-MISSION-REQ-007` | A mission that is uploaded but not verified against the plan file shall be presented as unverified |
| `AERORA-MISSION-REQ-008` | Post-flight mission reconstruction shall use the plan file and the log, not the vehicle mission store |

Full records in [requirements.md](requirements.md).
