# Upstream Drift Detection

A mechanism design. **Not implemented in Phase 4** — it is not required for the build bootstrap, and
the bootstrap is blocked anyway.

## The problem it solves

> **"No QGroundControl source modification" does not mean "no behavioural divergence from
> upstream."**

| Claim | True today | Mechanism |
| --- | --- | --- |
| Zero core edits required | **Yes** — `git diff --stat HEAD` is empty | Verifiable by one command |
| Zero silent forks | **No** — one tier-3 override exists | **No mechanism exists** |

A resource override replaces upstream behaviour at runtime while leaving the upstream file untouched
in the working tree. `git diff` stays empty. That is precisely what makes the drift silent: no
compiler warning, no lint rule, no merge conflict. When upstream fixes a bug in an overridden file,
the Aerora build keeps the bug and nothing reports it.

R-014, R-019, R-026. `AERORA-UI-REQ-006` requires a drift review at each merge;
`AERORA-TEST-REQ-008` requires it to be **mechanical rather than procedural**, because three
merge-time procedures that people forget become one check that fails.

## What must be tracked

| Item | Why |
| --- | --- |
| The pinned upstream commit | The baseline every comparison is against |
| Every overridden upstream file | The tier-3 list, and any tier-2 seam whose upstream content changes |
| The content hash of each overridden file **at its fork point** | What "unchanged since" means |
| The current upstream content hash of the same file | What changed |
| A named owner per override | Who reviews the change |
| The recorded reason for the override | So it can be removed if upstream adds a seam |

### Current inventory

| Tier | File | Fork point | Owner |
| --- | --- | --- | --- |
| 3 | `src/FlyView/FlyViewToolStripActionList.qml` | Recorded in the Phase 2 inventory | **OPEN** — Q-038 |
| 2 | `src/FlyView/FlyViewCustomLayer.qml` | Recorded | Upstream-documented seam |
| 2 | `src/FlyView/CustomGuidedActionsController.qml` | Recorded | Upstream-documented seam |
| 2 | `src/FlyView/FlyViewAdditionalCustomActionsList.qml` | Recorded | Upstream-documented seam |
| 2 | `src/FlightMap/MapItems/CustomMapItems.qml` | Recorded | Upstream-documented seam |

Tier-2 seams are tracked too. They are documented as override points so upstream has little reason
to change them, and "little reason" is not "no reason".

## The mechanism

A manifest plus a check. Specified, not written.

### Manifest

A single file in the overlay, version-controlled alongside the overrides it describes.

| Field per entry | Content |
| --- | --- |
| Upstream path | Repository-relative |
| Tier | 2 or 3 |
| Fork-point commit | The upstream commit the override was taken from |
| Fork-point content hash | A digest of the upstream file at that commit |
| Overlay path | The Aerora file that replaces it |
| Owner | A named person or team |
| Reason | Why the override exists |
| Review record | The commit at which drift was last reviewed |

The content hash matters more than the commit. A commit tells you the tree moved; a hash tells you
**this file** moved.

### The check

| Step | Action | Outcome |
| --- | --- | --- |
| 1 | For each manifest entry, read the upstream file at the current pinned commit | — |
| 2 | Compute its content hash | — |
| 3 | Compare against the recorded fork-point hash | Equal → pass |
| 4 | Unequal → **fail**, naming the file, both commits and the owner | A changed upstream file demands review |
| 5 | A manifest entry whose upstream path no longer exists → **fail** | Upstream deleted or moved the file |
| 6 | An overlay override with no manifest entry → **fail** | An unrecorded silent fork |
| 7 | A seam whose upstream content is no longer empty → **fail** | A tier-2 seam gained content |

Step 6 is the one that catches the real failure: an override added without being recorded. Steps 1 to
5 police known overrides; step 6 polices the list itself.

### Failure condition in continuous integration

| Property | Decision |
| --- | --- |
| Where it runs | The default gate, not an optional job — same reasoning as `AERORA-TEST-REQ-006` |
| When it runs | Every build, and necessarily at every upstream merge |
| On failure | **The build fails.** Not a warning |
| How it is cleared | A human reviews the upstream change, updates the overlay if needed, and records the new fork-point hash in the manifest |
| What it must not allow | Auto-updating the recorded hash. That would make the check pass while losing the upstream fix |

The last row is the one that would quietly destroy the mechanism. A check that heals itself is not a
check.

### Review workflow

```text
upstream merge proposed
   │
   ▼
drift check runs ──── pass ────► merge proceeds
   │ fail
   ▼
named owner reviews the upstream diff for each flagged file
   │
   ├── upstream change is irrelevant to the override
   │        └─► record the new fork-point hash, with a reason
   │
   ├── upstream change should be adopted
   │        └─► update the overlay file, then record the new hash
   │
   └── upstream added a seam that removes the need for the override
            └─► delete the override and the manifest entry
```

The third branch is the goal. Every tier-3 override is a candidate for deletion the moment upstream
provides a seam, and a tool-strip insertion seam is a small, obviously useful upstream contribution.

## Scope limits, stated honestly

| The mechanism detects | The mechanism does not detect |
| --- | --- |
| An overridden file changing upstream | A **semantic** change in a C++ virtual whose signature is unchanged |
| An override added without a manifest entry | A behavioural divergence caused by a settings default |
| A tier-2 seam gaining content | A divergence caused by the capability model gating something upstream would show |
| An upstream file disappearing | Anything about the vehicle |

The second column's first row is the residual: `AERORA-BUILD-REQ-007` requires a **seam signature
review** as well as a file drift review, and a changed virtual signature is a compile error while
changed virtual *semantics* are not. No mechanism catches that. It stays a review discipline, and
R-014 stays open because of it.

## Status

| Item | Status | Gate |
| --- | --- | --- |
| Manifest format | `IMPLEMENTATION-PLANNED` | Build host |
| Drift check | `IMPLEMENTATION-PLANNED` | Build host |
| Continuous-integration wiring | `IMPLEMENTATION-PLANNED` | Build host, and a CI runner |
| Tier-3 owner assignment | `CONTRACT-ONLY` | Q-038 — a process decision |
| Seam semantic review | **No mechanism** | R-014, carried |

Nothing was implemented. The directive permits implementation only if required for the build
bootstrap; it is not, and the bootstrap is blocked.

## Why this is a Phase 4 deliverable rather than a Phase 5 one

The manifest must be created **with the first override**, not after. An override added before the
manifest exists has no recorded fork point, and reconstructing one later means guessing which
upstream commit the file was copied from. The cost of getting this wrong is small now and
unrecoverable later.
