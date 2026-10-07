# Build Readiness Gate

The go/no-go decision: measured host against required host. Phase 3's build-readiness document stated
the **requirements**; [19-build-bootstrap.md](19-build-bootstrap.md) is the **procedure**; this is the
**gate**.

## Verdict

> **NO GO.** Two environment blockers, neither resolvable by Phase 4.

| # | Blocker | Measured | Required | Owner |
| --- | --- | --- | --- | --- |
| 1 | Free disk on the only drive | **1.00 GB** | ~25–40 GB | **User** |
| 2 | Administrator rights | **No** | Yes, for MSVC and GStreamer | **User** |

Nothing else on the checklist has been reached, because every remaining item is downstream of these
two.

## Running the gate

The checklist below is executable:

```text
powershell -NoProfile -ExecutionPolicy Bypass -File .kiro\tools\aerora-preflight.ps1
```

Read-only; installs nothing; exits `0` on go and `1` on no-go. Last run on this host: **6 pass, 8
fail, 1 warn — NO-GO**. Details in [19-build-bootstrap.md](19-build-bootstrap.md).

## Gate checklist

| # | Criterion | State | Evidence | Blocked by |
| --- | --- | --- | --- | --- |
| 1 | Sufficient free disk | **FAIL** | 1.00 GB of 111.1 GB, one drive | — |
| 2 | Elevated session available | **FAIL** | Administrator role: No | — |
| 3 | CMake ≥ 3.25 present | **FAIL** | Absent | 1, 2 |
| 4 | Ninja present | **FAIL** | Absent | 1 |
| 5 | C++20 compiler present | **FAIL** | No `cl`, `gcc`, `clang`, `clang-cl`; no `vswhere`; no Windows Kits | 1, 2 |
| 6 | Qt 6.11.1 with the 14 declared modules | **FAIL** | Nothing at `C:\Qt`, `C:\Qt6`, `%USERPROFILE%\Qt` | 1 |
| 7 | GStreamer 1.28.4 devel | **FAIL** | No `gst-launch-1.0` | 1, 2 |
| 8 | Python 3 present | **PASS** | 3.12 | — |
| 9 | `uv` present | **PASS** | — | — |
| 10 | Git present | **PASS** | — | — |
| 11 | Working tree unmodified at the pinned commit | **PASS** | `HEAD` = `9459a4b5...`; `git diff --stat HEAD` empty | — |
| 12 | Baseline QGC configures | **NOT REACHED** | — | 1–7 |
| 13 | Baseline QGC builds | **NOT REACHED** | — | 12 |
| 14 | Baseline QGC starts | **NOT REACHED** | — | 13 |
| 15 | `ctest -L Unit` passes on the baseline | **NOT REACHED** | — | 13 |
| 16 | Aerora repository exists, upstream fetch-only | **FAIL** | `origin` is upstream for **both fetch and push** — R-008 | **User** |
| 17 | Fork point recorded | **NOT REACHED** | — | 16 |
| 18 | Overlay directory structure in place | **NOT REACHED** | Specified in [02](02-module-architecture.md), not created | 13 |
| 19 | `QGC_CUSTOM_DIR` recognised | **NOT REACHED** | — | 18 |
| 20 | Overlay sources reach the application target | **NOT REACHED** | — | 19 |
| 21 | Aerora QML module loads | **NOT REACHED** | — | 20 |
| 22 | Aerora firmware plugin registers and is selected | **NOT REACHED** | — | 20 |
| 23 | Resource override resolves to Aerora content | **NOT REACHED** | — | 21 |
| 24 | Overlay disables cleanly | **NOT REACHED** | — | 19 |
| 25 | One trivial Aerora test is enumerated by `ctest` | **NOT REACHED** | Q-035, R-021 | 20 |
| 26 | Twelve architectural-property tests pass | **NOT REACHED** | — | 25 |
| 27 | All-network-blocked test passes | **NOT REACHED** | — | 25 |
| 28 | The payload-state plan constraint is implemented | **NOT REACHED** | R-002; carried in a rationale, not a requirement — R3-008 | 20 |

**4 pass, 7 fail, 17 not reached.**

Criterion 28 is the one with no requirement identifier of its own. It is listed here so that R3-008's
gap — an unconditional obligation living in a rationale — cannot be lost between phases.

## What the user must provide

| # | Item | Why it cannot be worked around |
| --- | --- | --- |
| 1 | **~40 GB free on `C:`**, or an additional drive | MSVC 6–8 GB, Qt 4–6 GB, GStreamer 1–2 GB, CPM cache 1–2 GB, Debug build tree 10–20 GB |
| 2 | **An elevated session** | `vs_BuildTools.exe` installs into Program Files; the GStreamer installer writes to `C:\gstreamer` and sets machine-scope environment variables |
| 3 | **An Aerora repository** to push to | `origin` currently points at upstream for push. The first implementation commit would target mavlink/qgroundcontrol |
| 4 | *Independent:* the radio bench observation | Settles Q3-001 and ADR-002. Needs no ground station — any host that enumerates USB devices will do |

Item 4 can proceed in parallel with items 1 to 3 by whoever holds the hardware.

## Why nothing was installed

| Reason | Detail |
| --- | --- |
| Filling the last gigabyte of a system drive risks destabilising Windows | The paging file and Windows Update both need headroom on `C:` |
| A part-completed MSVC installation is worse than none | It leaves a registered-but-broken toolchain that `vswhere` reports as present, which then masks the real problem |
| Elevation is not available | Both admin-requiring installers would fail partway |
| The directive authorises installation, not damage | §20: "Do not damage or replace the user's existing development environment unnecessarily" |

The admin-free half of the bootstrap — Qt, CMake, Ninja — was also **not** attempted, because
installing 4–6 GB of Qt onto 1 GB of free space fails and leaves a partial tree.

## Once unblocked

The sequence is scripted in [19-build-bootstrap.md](19-build-bootstrap.md) and runs in four phases:
B1 admin-free tools, B2 elevated tools, B3 baseline build and test, B4 repository bootstrap, B5
overlay structural proof.

| Expectation | Value |
| --- | --- |
| Build parallelism | `--parallel 4` at most — 4 cores, 7.9 GB memory. Higher will thrash — R4-004 |
| `QGC_ENABLE_WERROR` | Defaults **ON**. Aerora code must be warning-clean from the first commit |
| First build duration | Unknown. Not estimated, because an estimate would be invented |

## The one thing that is proven

| Property | Evidence |
| --- | --- |
| The tree is unmodified upstream at the pinned commit | `git rev-parse HEAD` = `9459a4b57986f40df8bdc193d5453c0596fbb416`; `git diff --stat HEAD` empty; untracked paths are `.kiro/` and the product baseline folder only |

Four phases of specification have produced **zero** modifications to QGroundControl. That is the one
build-adjacent claim this document can make, and it is verified rather than asserted.
