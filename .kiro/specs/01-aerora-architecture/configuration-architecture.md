# Configuration Architecture

Four kinds of configuration, each with a different lifetime and a different owner: version control,
build configuration, runtime settings, and vehicle configuration distributed from the cloud.

| Kind | Changes | Owner |
| --- | --- | --- |
| Version control | Per commit | Engineering |
| Build configuration | Per build | Engineering, in the overlay |
| Runtime settings | Per operator action | Operator, persisted locally |
| Vehicle configuration | Per fleet decision | Fleet management, distributed and cached |

## Version control

### The current state, which must change

The checkout this architecture was derived from is **upstream QGroundControl, not an Aerora fork**:

| Property | Value |
| --- | --- |
| Remote | The upstream mavlink/qgroundcontrol repository, for both fetch and push |
| Branch | `master` |
| Commit | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| Version description | `v5.2.0-dev-102-g9459a4b57` |
| Submodules | None — dependencies are fetched by the package manager at configure time |

Pushing Aerora work to that remote is not possible and would not be appropriate. Establishing the
Aerora fork is Q-003 and is ranked **first** in
[phase-2-open-question-resolution.md](phase-2-open-question-resolution.md), because every
subsequent engineering action depends on it.

### The target structure

```text
Aerora repository (origin)
    │
    ├── master                      Aerora integration branch
    ├── overlay/                    all Aerora code and resources
    │
    └── remote: upstream  ──────→   mavlink/qgroundcontrol (fetch only)
            │
            └── fork point: 9459a4b57986f40df8bdc193d5453c0596fbb416
```

| Element | Decision |
| --- | --- |
| `origin` | The Aerora repository |
| `upstream` | QGroundControl, configured fetch-only |
| Fork point | The Phase 1 analysis commit, recorded above |
| Aerora code location | A single overlay directory, never scattered through `src/` |
| Upstream integration | Deliberate merges, never automatic |

Recording the fork point explicitly is what makes every later question answerable: what has upstream
changed, which overridden files moved, which seam signatures altered. Phase 1 analysed that exact
commit, so the baseline documents and the fork point describe the same code.

### Why fetch-only on upstream

A push to upstream from an Aerora working tree would be an accident with reputational consequences.
Configuring the remote as fetch-only removes the possibility rather than relying on care.

### Upstream integration obligations

Each upstream merge carries work that no tool will prompt for:

| Obligation | Reason |
| --- | --- |
| Drift review of every overridden populated file | A tier-3 override silently discards upstream fixes — see [ui-architecture.md](ui-architecture.md) |
| Seam signature review | A changed virtual signature is a compile error; a changed virtual *semantic* is not |
| Startup self-check run | Catches an override that stopped resolving |
| Settings-version review | See the key-destruction coupling below |
| Dialect compatibility review | The pinned dialect and the merged upstream must still agree |

The second row is the subtle one. If upstream changes what a seam *means* while keeping its
signature — for example changing when `initializeVehicle` is called relative to parameter load —
nothing fails to build and the Aerora behaviour changes. The mitigation is that Aerora's seam use is
documented in [vehicle-integration.md](vehicle-integration.md) with the call-site line numbers, so
the review has something concrete to check against.

## Build configuration

### The overlay override file

Aerora build configuration lives in the overlay's CMake override file, which the top-level build
includes when a custom directory is configured. The variables Aerora sets:

| Variable | Purpose |
| --- | --- |
| Application and package name | Branding and platform identity |
| Platform icon and installer-art paths | Branding assets |
| `QGC_MAVLINK_GIT_REPO`, `_TAG`, `_DIALECT` | Aerora dialect selection, pinned to a commit |
| `QGC_DISABLE_PX4_PLUGIN_FACTORY`, `QGC_DISABLE_APM_PLUGIN_FACTORY` | Single-stack or multi-stack, per Q-017 |
| `QGC_ENABLE_GST_VIDEOSTREAMING` | Video retention, per Q-031 |
| `QGC_SETTINGS_VERSION` | Incremented when Aerora changes persisted settings shape |

All of these are existing upstream variables. The overlay sets values; it does not introduce
mechanisms. That is what keeps the build configuration a configuration rather than a fork.

### The settings-version coupling

This deserves its own treatment because it destroys data silently.

The application clears **all** persisted settings when the stored settings-schema version differs
from the build's value, before any subsystem starts (`QGCApplication.cc:129-137`). The build's
current value is 9.

MAVLink signing keys are persisted in that same settings store. Therefore:

```text
bump QGC_SETTINGS_VERSION  →  settings.clear() on next launch  →  every signing key destroyed
```

| Consequence | Handling |
| --- | --- |
| A build-configuration change becomes a fleet reprovisioning event | A settings-version bump is a release-process decision, not a routine edit |
| The operator sees only a generic settings-upgraded notice | Aerora adds an explicit key-loss warning path |
| The real fix is moving key custody out of the settings store | Tracked in [security-architecture.md](security-architecture.md) as R-005 |

Until key custody moves, **incrementing the settings-schema version requires a reprovisioning
plan**. Recording it here is the point: the coupling is invisible from either side on its own.

### Reproducibility

| Element | Pinning |
| --- | --- |
| Upstream QGroundControl | Fork point commit, then explicit merge commits |
| MAVLink dialect | An immutable commit, never a branch |
| Third-party dependencies | Pinned; R-011 records two that currently track moving references |
| Qt version | Pinned by the build and reported in the build summary |
| Build identity | Derived from the version-control description, as upstream already does |

The dialect pin is an architectural requirement rather than a preference. Without it, two builds of
the same Aerora commit can embed different message definitions, and the compatibility matrix the
baseline requires becomes meaningless. Stated in full in
[mavlink-architecture.md](mavlink-architecture.md).

## Runtime settings

### Mechanism

Settings are generated at configure time from two JSON input sets: page definitions and settings
group metadata. A custom build may overlay the page list, add or shadow page definitions, and add
its own settings group metadata, through two directories the build passes to the generator
(`src/AppSettings/CMakeLists.txt:28-40`).

| Aerora element | Supplied as |
| --- | --- |
| Aerora settings group | An Aerora settings group metadata file in the overlay |
| Aerora settings pages | Page definition files in the overlay's pages directory |
| Changed upstream defaults | Shadowed page definitions and settings metadata |

### All settings are Facts

Settings reach QML and C++ as Facts, with units, bounds, enumerations and translation from their
metadata. This is the same system as vehicle parameters and the same principle applies: there is no
Aerora settings store beside the Fact system.

The distinction from vehicle parameters is ownership, not mechanism:

| Kind | Lives on | Authority |
| --- | --- | --- |
| Vehicle parameter | The vehicle | The vehicle |
| GCS setting | The station | The station |

An Aerora setting never shadows or caches a vehicle parameter. Where the two would overlap, the
vehicle parameter is authoritative and the setting does not exist.

### Aerora default changes

| Setting | Aerora default | Reason |
| --- | --- | --- |
| SiK radio auto-connect | Disabled | Prevents the serial misidentification trap in [communication-architecture.md](communication-architecture.md) |
| Flight log auto-upload | Disabled | The upstream default targets a third-party endpoint — see [logging-architecture.md](logging-architecture.md) |
| Delete log after upload | Disabled, not operator-configurable | The local record is evidence |
| Save telemetry when not armed | Enabled | Aerora operations include significant pre-arm activity |
| Signing policy default | `OPEN` as Q-010 | Not yet decided |

Four of five are changes from upstream, and three of those four exist because an upstream default
is actively wrong for Aerora rather than merely different. That ratio is worth noting: default
review is not a cosmetic exercise.

## Vehicle configuration from the cloud

### Properties

| Property | Statement |
| --- | --- |
| Versioned | Every distributed configuration carries a version |
| Cached locally | The last received configuration is persisted |
| Usable without the network | The last-applied configuration is retained and used |
| Never on the command path | A missing or stale configuration never blocks a command |
| Applied deliberately | A newly received configuration is not silently applied mid-operation |
| Recorded | Every configuration change is an audit event |

The fifth row prevents the failure where a configuration arrives during a flight and changes
behaviour an operator has already planned around. A received configuration becomes pending; it is
applied at a defined point.

### What it does not contain

| Excluded | Reason |
| --- | --- |
| Vehicle parameters | Those belong to the vehicle and flow through the Fact system |
| Signing keys | Key provisioning is a security process, not a configuration push |
| Authorisation policy that gates a flight action | That would put the cloud on the authorisation path |
| Anything whose absence would block an operation | Third hidden-dependency trap in [cloud-architecture.md](cloud-architecture.md) |

## Configuration identity

A deployed Aerora GCS must be able to state exactly what it is, because a compatibility matrix is
useless if a station cannot identify itself:

| Element | Source |
| --- | --- |
| Aerora GCS version | Build, from the version-control description |
| Upstream fork point and merge state | Build |
| Dialect commit | Build |
| Settings schema version | Build |
| Applied vehicle configuration version | Runtime, from the cache |
| Qt and dependency versions | Build |

All six are recorded in the log header and reported to the fleet registry. The Diagnostics module
surfaces them and flags a mismatch against the compatibility matrix.

## Core edits required

**None.** Every item above is an existing upstream variable, an existing generator input, or an
Aerora module in the overlay.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-003 | Establishing the Aerora fork | `ARCH-SHAPING`, ranked first |
| Q-017 | Single-stack or multi-stack build | `ARCH-SHAPING` |
| Q-010 | Default signing policy | `DEFERRABLE` |
| Q-031 | Whether video is retained | `DEFERRABLE` |
| Q-006 | Removal of QGroundControl branding and artwork | `IMPL-BLOCKING` |
| Q-038 | Upstream merge cadence and ownership | `IMPL-BLOCKING` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-BUILD-REQ-004` | Aerora shall maintain its own repository with upstream QGroundControl configured as a fetch-only remote |
| `AERORA-BUILD-REQ-005` | The upstream fork point commit shall be recorded and shall match the commit the baseline analysis was performed against |
| `AERORA-BUILD-REQ-006` | All Aerora code and resources shall reside in a single overlay directory |
| `AERORA-BUILD-REQ-007` | Each upstream integration shall include a drift review of every overridden file and a seam signature review |
| `AERORA-BUILD-REQ-008` | The MAVLink dialect shall be pinned to an immutable commit and never to a branch |
| `AERORA-BUILD-REQ-009` | A settings-schema version change shall require a recorded reprovisioning plan while signing keys are persisted in the settings store |
| `AERORA-BUILD-REQ-010` | A deployed GCS shall be able to report its build version, fork point, dialect commit, settings schema version, applied configuration version and dependency versions |
| `AERORA-BUILD-REQ-011` | An Aerora setting shall not shadow or cache a vehicle parameter |
| `AERORA-BUILD-REQ-012` | Distributed vehicle configuration shall be versioned, cached, usable without network access, and applied deliberately rather than mid-operation |

Full records in [requirements.md](requirements.md).
