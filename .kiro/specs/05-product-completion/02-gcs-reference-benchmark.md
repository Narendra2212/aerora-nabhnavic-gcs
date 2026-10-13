# GCS Reference Benchmark

Owner: **Tab 7 (`M-Ui`)**, compiled by Tab 1 for Phase A. Workflow-principle extraction from
eighteen reference products.

## Scope and rules

| Rule | Application |
| --- | --- |
| **Identity verified first** | No feature is attributed to a product until the product's identity and official documentation are confirmed. Three of the eighteen names did not resolve to what the list implied â€” see [Â§2](#2-identity-corrections) |
| **No proprietary source, branding or visual assets** | Nothing is copied. Only workflow principles are extracted |
| **Original design system** | Aerora's visual language is its own. See [03 Â§ Design system](03-gui-information-architecture-and-screens.md#5-design-system) |
| **Paraphrase only** | All descriptions are summarised from the cited sources. Content was rephrased for compliance with licensing restrictions |
| **Unverified is labelled** | Where official documentation could not be located, the entry says so instead of guessing |

Searched this pass, October 2026. Where a vendor page carried no publication date, the entry notes
that the information is undated.

## Contents

- [1. Benchmark summary](#1-benchmark-summary)
- [2. Identity corrections](#2-identity-corrections)
- [3. Product entries](#3-product-entries)
- [4. Cross-product pattern analysis](#4-cross-product-pattern-analysis)
- [5. Patterns Aerora adopts](#5-patterns-aerora-adopts)
- [6. Patterns Aerora deliberately rejects](#6-patterns-aerora-deliberately-rejects)
- [7. Gaps none of the references fill](#7-gaps-none-of-the-references-fill)
- [8. Sources](#8-sources)

## 1. Benchmark summary

| # | Reference as listed | Resolves to | Identity | Domain |
| --- | --- | --- | --- | --- |
| 1 | iX Suite | Phase One iX Suite (iX Plan, iX Flight Pro, iX Process) | **Confirmed** | Aerial photogrammetry / mapping, sensor-led |
| 2 | Drone Harmony | Drone Harmony AG platform (Web, Mobile, Cloud) | **Confirmed** | 3D asset inspection, DJI-ecosystem |
| 3 | QBase 3D | Quantum Systems QBase 3D | **Confirmed** | Survey/mapping for Quantum-Systems UAVs |
| 4 | eMotion 3 | senseFly / AgEagle eMotion | **Confirmed** | Fixed-wing + multirotor mapping (eBee) |
| 5 | HowayGIS | Shanghai HowayGIS Infotech â€” GNSS/geospatial vendor | **Corrected** â€” not a GCS | GNSS field data collection |
| 6 | AiProFlight | Aibotix AiProFlight (Leica Geosystems) | **Confirmed, legacy** | Industrial inspection / survey |
| 7 | MissionGuidance | No product of this name located | **Unverified** | â€” |
| 8 | U-Pilot | Airelectronics U-Pilot â€” an **autopilot**; the GCS is U-See | **Corrected** | Autopilot + ground segment |
| 9 | Route Planning | Generic feature name, not a product | **Corrected** | â€” |
| 10 | Veronte | Embention Veronte ecosystem (PCS, Link, Pipe) | **Confirmed** | Certifiable autopilot + GCS, UAV/eVTOL |
| 11 | QGroundControl | QGroundControl | **Confirmed** | Open-source MAVLink GCS â€” **Aerora's base** |
| 12 | Mission Planner | ArduPilot Mission Planner | **Confirmed** | Open-source ArduPilot GCS, Windows |
| 13 | TitanPlanner | Titan Dynamics â€” a Mission Planner derivative | **Confirmed, derivative** | Vendor-branded ArduPilot GCS |
| 14 | Helios GCS | heliosgcs.com / `jamesagarside/helios` | **Confirmed** | Open-source modern GCS, telemetry-database-led |
| 15 | Skybrush | CollMot Skybrush Suite (Live, Server, Studio) | **Confirmed** | Drone shows / swarms |
| 16 | DJI FlightHub 2 | DJI FlightHub 2 (cloud + on-premises) | **Confirmed** | Enterprise fleet/cloud operations, DJI-only |
| 17 | Auterion Mission Control | Auterion Mission Control (AMC) | **Confirmed** | Enterprise PX4 operations â€” **closest peer** |
| 18 | Visionair | UAV Navigation (Grupo OesÃ­a) Visionair | **Confirmed** | Defence/professional UAV GCS |

**15 of 18 confirmed. 3 corrected or unverified.** Those three are reported rather than quietly
dropped, because a benchmark that silently substitutes a plausible product for an unverifiable name
is worse than one with gaps.

## 2. Identity corrections

### HowayGIS â€” not a ground control station

[Shanghai HowayGIS Infotech](https://geo-matching.com/companies/howaygis) is a Shanghai-registered
geospatial-information company working in the GNSS field. Its documented products are GNSS field
devices (the T18 and T21 handheld series appear in published quick-start manuals). No GCS product
and no flight-control software was located under this name.

**Disposition**: excluded from feature attribution. If a specific HowayGIS flight application is
intended, the exact product name is needed. Recorded as `OPEN` in
[08](08-risk-register-and-open-decisions.md).

### U-Pilot â€” an autopilot, not a GCS

[Airelectronics](https://airelectronics.es/) describes an ecosystem in which the autopilot is
U-Pilot, the ground hardware is U-Ground, and **the ground software is U-See**; a separate U-Mosaic
handles image projection. Published kit descriptions (Skywalker, Octocopter) consistently pair
U-Pilot with U-See in that split.

**Disposition**: the GCS-relevant reference is **U-See**. Benchmarked at
[Â§3.8](#38-airelectronics-u-see-the-gcs-for-u-pilot) with the autopilot/GCS split made explicit,
because that split is itself the relevant architectural observation.

### MissionGuidance and Route Planning â€” not resolvable as products

Neither name resolved to a product with official documentation. Both read as feature or module names
rather than products â€” "mission guidance" and "route planning" are generic capabilities present in
most of the other seventeen.

**Disposition**: excluded from attribution. Treated as capability areas and covered by the
cross-product analysis in [Â§4](#4-cross-product-pattern-analysis). If specific products are intended,
vendor names are needed.

## 3. Product entries

Each entry: documentation URL, users and domain, live monitoring, planning and visualisation, fleet,
payload/sensor, AI/automation, data and reporting, the pattern useful to Aerora, and limitations.

### 3.1 Phase One iX Suite

| Field | Content |
| --- | --- |
| **Documentation** | [geospatial.phaseone.com](https://geospatial.phaseone.com/) Â· [iX Plan Operation Guide (PDF)](https://8b6065f7bd796377729f-3328e8a66c82a5c5f6aaaf6531b2a2ea.ssl.cf5.rackcdn.com/Documentation%2FGEO%2FiX%20Plan%2015.0.0%20Operation%20Guide.pdf) |
| **Users / domain** | Professional aerial survey and photogrammetry crews, including manned aircraft |
| **Structure** | Three applications: iX Plan (planning), iX Flight Pro (in-flight sensor and flight management), iX Process (post-flight quality review and conversion) |
| **Live monitoring** | In-flight imagery is displayed overlaid on a digital terrain model, so the onboard operator can judge image quality and confirm coverage of the area of interest while still airborne |
| **Planning / visualisation** | Image-capture positions are computed in 3D from the customer's requirements, terrain data and sensor characteristics |
| **Fleet** | Not a fleet product. Project- and mission-centric |
| **Payload / sensor** | The strongest sensor-first workflow in the set. The sensor's characteristics drive the plan, and sensor management is a first-class in-flight surface |
| **AI / automation** | Automated plan generation from requirements; no autonomy supervision |
| **Data / reporting** | Explicit quality-control stage spanning planning, flight and post-flight review, aimed at coverage, image quality and on-time delivery |
| **Useful to Aerora** | **The three-stage plan â†’ acquire â†’ verify loop with quality control at each stage.** Also: payload characteristics as a planning *input*, not a planning afterthought. Aerora's painting profile has the same shape â€” the applicator's characteristics determine the coverage pattern |
| **Limitations** | Proprietary; bound to Phase One sensors; mapping-only domain; no supervisory-autonomy concept |

### 3.2 Drone Harmony

| Field | Content |
| --- | --- |
| **Documentation** | [droneharmony.com/user-guide](https://www.droneharmony.com/user-guide) Â· [3D flight planner](https://www.droneharmony.com/3d-flight-planner) |
| **Users / domain** | Asset-inspection and complex-environment operators: towers, mines, quarries, forests, industrial sites |
| **Live monitoring** | Mobile app executes plans authored on the platform; pre-launch review dialogue gathers launch parameters before flight |
| **Planning / visualisation** | **Scene-based workflow** â€” a two-step model in which the operator first builds a *scene* from polygons and polygonal lines representing assets and obstructions, then generates flights against that scene |
| **Fleet** | Site-centric organisation with cloud sync across web and mobile |
| **Payload / sensor** | Per-shot camera framing, exact or approximate |
| **AI / automation** | Automated trajectory generation between required shots, plus plan-adjustment tools |
| **Data / reporting** | Cloud storage of planning data, synchronised across devices |
| **Useful to Aerora** | **The scene-then-mission separation is the single most transferable idea in this benchmark.** A painted structure is a scene â€” a geometry plus obstructions â€” and the paint pass is a mission generated against it. Separating the two means re-planning a pass does not mean re-tracing the building. Also: the explicit pre-launch review dialogue as a distinct step |
| **Limitations** | Proprietary; primarily DJI-ecosystem; cloud-oriented (though mobile execution is supported); not a MAVLink GCS |

### 3.3 Quantum Systems QBase 3D

| Field | Content |
| --- | --- |
| **Documentation** | [quantum-systems.com](https://quantum-systems.com/) Â· [QBase Tactical](https://quantum-systems.com/qbase-tactical/) |
| **Users / domain** | Survey and mapping with Quantum-Systems UAVs. A sibling product, QBase Tactical, targets government and military users |
| **Live monitoring** | Aircraft and sensor data received at the ground controller is stored locally for post-mission analysis â€” a **local-first** posture |
| **Planning / visualisation** | Flight paths generated automatically from mission parameters and no-fly zones; large sites can be planned as a whole and segmented automatically into flyable areas, with continuity prompts across the segments |
| **Fleet** | Vendor-bound rather than fleet-generic |
| **Payload / sensor** | Integrated with the vendor's sensor set |
| **AI / automation** | **Pre-flight mission simulation** including wind forecast, battery consumption and flight time, plus automated verification of mission risks and errors, and terrain following |
| **Data / reporting** | Local storage for post-mission analysis; QuickStart missions without prior planning for time-critical tactical use |
| **Useful to Aerora** | **Three patterns.** (a) Pre-flight simulation that produces a reviewable prediction before committing. (b) Automated mission *validation* as a distinct step that reports risks and errors. (c) Plan-whole-site-then-segment, which maps directly onto a multi-pass painting job that exceeds one battery |
| **Limitations** | Proprietary and vendor-locked. Note for Aerora: the battery-consumption and flight-time predictions are exactly the class of estimate `P7-ARCH-005` prohibits unless the method is specified and approved |

### 3.4 senseFly / AgEagle eMotion

| Field | Content |
| --- | --- |
| **Documentation** | [ageagle.com](https://ageagle.com/) Â· [sensefly.com eMotion 3](https://www.sensefly.com/software/emotion-3.html) |
| **Users / domain** | Professional mapping with eBee fixed-wing and multirotor aircraft |
| **Live monitoring** | In-flight interaction with the aircraft; commands can be issued during a planned mission to alter the trajectory, change the landing point, hold position, resume, or land |
| **Planning / visualisation** | Draw the area to map and the software derives the flight plan |
| **Fleet** | Mission- and battery-cycle-centric rather than fleet-centric; battery-change management is called out as a managed workflow |
| **Payload / sensor** | Supports the vendor's camera range from RGB through multispectral and thermal; images are georeferenced during flight to reduce post-processing |
| **AI / automation** | Safety-driven automatic return-to-launch under conditions such as strong wind, low light, misconfigured sensors or degraded GNSS accuracy |
| **Data / reporting** | Data prepared for post-processing as part of the flight, not after it |
| **Useful to Aerora** | **(a)** Mid-mission operator intervention as a designed, bounded set of actions rather than an escape hatch. **(b)** Battery-change as an explicit workflow step â€” directly relevant to multi-pass painting. **(c)** Georeferencing during flight: do the post-processing preparation while the data is being captured |
| **Limitations** | Proprietary and aircraft-locked. The automatic RTL is an **aircraft** behaviour; Aerora must display such behaviour, never implement it (`P7-ARCH-001`, `P7-SAFE-002`) |

### 3.5 HowayGIS

**Excluded.** Not a ground control station â€” see [Â§2](#2-identity-corrections). No features
attributed.

### 3.6 Aibotix AiProFlight

| Field | Content |
| --- | --- |
| **Documentation** | [AiProFlight 2.9 User Manual (Leica Geosystems, PDF)](https://cm.leica-geosystems.com/-/media/files/aibotix/products/aiproflight/manuals/20161110aibotixaiproflightmanual29en.ashx?hash=0F35479BE4B8F26622756400793FFF39&sc_lang=sv-se) |
| **Users / domain** | Industrial inspection and survey with Aibotix multirotors, under Leica Geosystems / Hexagon |
| **Status** | **Legacy.** The located documentation is version 2.9, dated 2016. No current product page was found |
| **Live monitoring** | Per the manual. Not re-attributed here beyond its documented existence |
| **Useful to Aerora** | Primarily as a **lifecycle warning**: a vendor-locked GCS tied to one airframe generation becomes unmaintained when the airframe does. This is a direct argument for Aerora's profile architecture (`P7-PROF-002`), where a new aircraft is a configuration change rather than a new product |
| **Limitations** | Legacy, proprietary, airframe-locked. Treated as a historical reference, not a current benchmark |

### 3.7 MissionGuidance and Route Planning

**Excluded.** Not resolvable as products â€” see [Â§2](#2-identity-corrections). The capability areas
they name are covered in [Â§4](#4-cross-product-pattern-analysis).

### 3.8 Airelectronics U-See (the GCS for U-Pilot)

| Field | Content |
| --- | --- |
| **Documentation** | [airelectronics.es](https://airelectronics.es/) Â· [product catalogue](https://www.aeroexpo.online/soc/airelectronics-185532.html) |
| **Users / domain** | Integrators and operators building on the U-Pilot autopilot; applications cited include reconnaissance, border and traffic monitoring, policing and firefighting |
| **Structure** | Explicit three-part split: **U-Pilot** (airborne autopilot), **U-Ground** (ground hardware providing the link), **U-See** (ground software the operator interacts with). U-Mosaic handles image projection using data from U-Pilot and U-See |
| **Live monitoring** | Real-time mission planning and execution through the ground software |
| **Planning / visualisation** | A flight-plan pattern designer; plans can be modified in real time |
| **Payload / sensor** | Separate payload products (e.g. U-Camera) composed into kits |
| **Data / reporting** | U-Mosaic projects captured images onto the ground for a preliminary view that aids analysis |
| **Useful to Aerora** | **The vendor's own vocabulary separates airborne authority, link hardware and ground software into three named products.** That is the boundary `P7-ARCH-001`..`004` assert, expressed as a product structure. Aerora should be as explicit: the station is the ground software, and it says so |
| **Limitations** | Proprietary; ecosystem-locked; firefighting is listed as an application domain, which is useful corroboration that Aerora's planned profile set is a real market, but is not evidence about any Aerora capability |

### 3.9 Embention Veronte

| Field | Content |
| --- | --- |
| **Documentation** | [embention.com](https://www.embention.com/en/products/) Â· [manuals.embention.com](https://manuals.embention.com/) Â· [Veronte PCS](https://www.embention.com/en/product/pcs/) |
| **Users / domain** | UAV and eVTOL programmes with certification obligations. The vendor cites development against DO-178C, DO-254 and DO-160 |
| **Live monitoring** | Veronte Link supports multiple control stations and autopilots interconnected and operating simultaneously |
| **Planning / visualisation** | Control *phases* as a first-class concept, with actuator mixing and transitions adapted per vehicle layout |
| **Fleet / interoperability** | Partial support for STANAG 4586, the NATO UAV control-system interface standard, aimed at mixed-fleet integration with compliant stations |
| **Payload / sensor** | Part of the broader avionics ecosystem |
| **AI / automation** | Autopilot-resident decision-making; the GCS is the operator interface to it |
| **Data / reporting** | A post-flight viewer replays recorded data and generates plots and reports. The vendor explicitly discusses data integrity and accessibility as certification-relevant properties |
| **Useful to Aerora** | **Four patterns, and the most architecturally relevant entry in the benchmark.** (a) The autopilot holds authority; the station is the interface to it â€” Aerora's `P7-ARCH-002`. (b) "Phase" as the unit of operational state, which is a better model than a flat flight-mode enum for a multi-stage painting job. (c) Data integrity treated as a product property because certification demands it â€” Aerora's `P7-AUDIT-001`. (d) **Multiple stations against one vehicle**, which forces the question of which station holds authority. Aerora has not answered this and it is not in the current requirement set |
| **Limitations** | Proprietary; tied to Veronte autopilots; STANAG 4586 support is described as partial. Aerora must not claim standard conformance it has not implemented |

### 3.10 QGroundControl

| Field | Content |
| --- | --- |
| **Documentation** | [docs.qgroundcontrol.com](https://docs.qgroundcontrol.com/) Â· [dev guide](https://docs.qgroundcontrol.com/Stable_V5.0/en/qgc-dev-guide/index.html) Â· [github.com/mavlink/qgroundcontrol](https://github.com/mavlink/qgroundcontrol/) |
| **Users / domain** | Any MAVLink vehicle, PX4 and ArduPilot included. Cross-platform: Windows, Linux, macOS, Android, iOS |
| **Relationship to Aerora** | **This is Aerora's base.** Not a competitor â€” the substrate. Everything below is a capability Aerora already has |
| **Live monitoring** | Fly view with map or 3D viewport, instrument panel, telemetry value grid, guided-action bar, video with picture-in-picture, multi-vehicle list, proximity/obstacle overlays, per-vehicle toolbar indicators |
| **Planning / visualisation** | Plan view with waypoints and five built-in complex items (Survey, Corridor Scan, Structure Scan, fixed-wing landing, VTOL landing), geofence, rally points, terrain follow, plan-file round-tripping. An existing Qt Quick 3D viewport (`src/Viewer3D/`) |
| **Fleet** | Multi-vehicle via `MultiVehicleManager`. No fleet *records* â€” vehicles are sessions, not inventory |
| **Payload / sensor** | Camera manager with camera-definition files, gimbal control, on-screen gimbal and tracking controllers |
| **AI / automation** | None. QGC is deliberately not an autonomy layer |
| **Data / reporting** | Log download, ULog and DataFlash plotting, log replay, GeoTag, MAVLink inspector and console, vibration analysis |
| **Extension model** | `QGCCorePlugin` and `QGCOptions` virtuals, four purpose-built QML seams, settings JSON overlay, firmware and autopilot plugin factories, URL interception for resource replacement. Fully inventoried in [03](03-gui-information-architecture-and-screens.md) |
| **Useful to Aerora** | Everything above is reused. Aerora's job is the industrial/supervisory layer QGC does not have: state-class honesty, authorization, audit, product profiles, payload supervision and autonomy approval |
| **Limitations** | No fleet inventory, no roles or permissions, no audit trail, no autonomy-proposal concept, no state-quality vocabulary beyond per-fact staleness. **These absences are precisely Aerora's product thesis** |

### 3.11 ArduPilot Mission Planner

| Field | Content |
| --- | --- |
| **Documentation** | [ardupilot.org/planner](https://ardupilot.org/planner/docs/mission-planner-overview.html) Â· [missionplanner.com](https://www.missionplanner.com/) |
| **Users / domain** | ArduPilot operators and integrators. **Windows only** |
| **Live monitoring** | The Flight Data screen is a virtual-cockpit HUD with an adjacent control-and-status area. Telemetry fields are **user-selectable into the HUD** via a right-click "User Items" menu |
| **Planning / visualisation** | Point-and-click waypoint, fence and rally entry over several map providers including custom WMS; mission commands chosen from drop-down menus |
| **Fleet** | Single-vehicle oriented |
| **AI / automation** | None; configuration and mission tooling |
| **Data / reporting** | Log download and analysis; full SITL integration for software-in-the-loop simulation |
| **Useful to Aerora** | **(a)** The operator-composable HUD â€” the operator decides which values are prominent. QGC has an equivalent in its value grid, and Aerora should extend rather than replace it. **(b)** Custom WMS map support matters for industrial sites with their own basemaps. **(c)** The documentation is blunt that commands only take effect when the vehicle is under autopilot control, and that an RC switch in manual overrides the station. **That honesty about where authority actually sits is the tone Aerora's UI should adopt** |
| **Limitations** | Windows-only; ArduPilot-centric; dated UI conventions; no roles, audit or state-quality model |

### 3.12 TitanPlanner

| Field | Content |
| --- | --- |
| **Documentation** | [github.com/Titan-Dynamics/TitanPlanner](https://github.com/Titan-Dynamics/TitanPlanner/releases) |
| **Identity** | A Titan Dynamics vendor-branded derivative of ArduPilot Mission Planner (C# / .NET) |
| **Useful to Aerora** | **A direct precedent for the overlay strategy, and a cautionary one.** A vendor fork of an open GCS is a recognised way to ship a branded industrial station. The caution: a fork of a populated application inherits merge cost forever. Aerora's class-R/C/O/X scheme and its tier-3 count of **zero** exist to avoid exactly this. Where Aerora must fork a populated file, [03](03-gui-information-architecture-and-screens.md) records the fork point and an owner |
| **Limitations** | Inherits Mission Planner's Windows-only constraint and its ArduPilot focus. No independent documentation was located |

### 3.13 Helios GCS

| Field | Content |
| --- | --- |
| **Documentation** | [heliosgcs.com](https://heliosgcs.com/) Â· [github.com/jamesagarside/helios](https://github.com/jamesagarside/helios) |
| **Identity** | Free, open-source GCS for drones and unmanned vehicles |
| **Live monitoring** | Planning through live control in one operator workflow |
| **Data / reporting** | **The distinguishing feature: every telemetry point is recorded automatically into a queryable database, so flights are analysed with SQL rather than only with fixed graphs** |
| **Useful to Aerora** | **The strongest single idea for Aerora's post-flight story.** Aerora needs an event timeline joining state transitions, operator actions and authorization outcomes on one axis (`P7-TEL-004`, `P7-LOG-003`), and a queryable local store is the natural shape for that. It also aligns with `P7-DAT-002` (documented local schema) and with audit durability (`Q-037`) â€” one mechanism could serve both. **Aerora's version must additionally record the state class of every value**, which no reference product does |
| **Limitations** | Young project; smaller ecosystem than QGC or Mission Planner; no roles, authorization or autonomy supervision. Not a candidate base â€” Aerora's base decision is QGC and is settled |

### 3.14 Skybrush

| Field | Content |
| --- | --- |
| **Documentation** | [docs.skybrush.io](https://docs.skybrush.io/public/skybrush-live-doc/latest/index.html) Â· [skybrush.io/modules/live](https://skybrush.io/modules/live/) Â· [github.com/skybrush-io/live](https://github.com/skybrush-io/live) |
| **Users / domain** | Drone shows and drone-swarm missions |
| **Architecture** | A three-part split the documentation is explicit about: **Skybrush Live** (GCS frontend GUI), **Skybrush Server** (GCS backend, talking to drones, RTK base stations and peripherals), **Skybrush Studio** (design, with a Blender add-on). Server can run on the operator desktop or on a dedicated network device |
| **Live monitoring** | Several simultaneous viewports â€” 2D, 3D, list and a compressed form â€” with both individual and group control, oriented to monitoring and executing a show safely including handling critical situations |
| **Planning / visualisation** | Show design in Studio; a show origin and orientation are mapped onto the geodetic coordinate system, with a fit-to-observed-drone-positions helper |
| **Fleet** | **The best fleet-scale monitoring model in the set** â€” designed from the start for many simultaneous vehicles |
| **AI / automation** | Collective control actions run a preparation phase first, informing all drones of the upcoming procedure before it takes effect; the show continues without visible change during preparation |
| **Useful to Aerora** | **(a)** The frontend/backend split, with the backend optionally on separate hardware â€” relevant if Aerora's GPU/autonomy element becomes a separate network peer. **(b)** Multiple simultaneous viewports over the same fleet state, including a *list* view, which scales where a map does not. **(c)** **The two-phase collective action â€” prepare, then commit â€” is directly applicable to Aerora's autonomy approval**: a proposal is prepared and acknowledged before it is executed, and the preparation is observable. **(d)** Explicit documentation of mode preconditions: drones must be in the show mode or they will not launch, stated plainly rather than implied |
| **Limitations** | Show/swarm domain; requires Skybrush-compatible firmware and parameter setup; not an industrial single-aircraft payload station. Virtual drones are available for testing, which is a good precedent for `P7-SIM-002` |

### 3.15 DJI FlightHub 2

| Field | Content |
| --- | --- |
| **Documentation** | [enterprise.dji.com/flighthub-2](https://enterprise.dji.com/flighthub-2/faq) Â· [support](https://www.dji.com/uk/support/product/flighthub-2) |
| **Users / domain** | Enterprise drone-operations management. Available as a public cloud service and as an on-premises deployment, in Standard, Professional and Enterprise tiers |
| **Live monitoring** | Live situational awareness and remote control; dock automation |
| **Planning / visualisation** | Flight-route management and intelligent flight scheduling; cloud mapping. A 2.5D base map overlays elevation on satellite imagery for a more three-dimensional sense of terrain in emergency scenarios |
| **Fleet** | **The reference fleet-and-cloud product.** Task planning, fleet oversight and data management in one secure cloud environment |
| **AI / automation** | Automatic anomaly detection with generated operational reports, aimed at inspection workflows in energy, water, forestry and environmental work. Recent versions integrate intelligent algorithms and a multimodal large language model into planning, execution and analysis |
| **Data / reporting** | Centralised data management; third-party integration |
| **Useful to Aerora** | **(a)** The on-premises option alongside the cloud one is the right shape for industrial customers who cannot use a vendor cloud â€” and it validates Aerora's local-first requirement rather than contradicting it. **(b)** Automatic anomaly detection *that produces a report a human reads* rather than an action the system takes â€” exactly the proposal-not-command boundary of `P7-ARCH-002`. **(c)** 2.5D elevation overlay as a cheap middle ground between a 2D map and a full 3D scene |
| **Limitations** | **Works only with DJI enterprise hardware**, and is scoped to flight operations. Cloud-first. Aerora's hardware independence (`P7-HW-001`) is a deliberate contrast. Note also that an LLM in the planning path raises provenance questions Aerora's `P7-ARCH-004` labelling requirement would have to answer |

### 3.16 Auterion Mission Control

| Field | Content |
| --- | --- |
| **Documentation** | [docs.auterion.com/vehicle-operation/auterion-mission-control](https://docs.auterion.com/vehicle-operation/auterion-mission-control) |
| **Users / domain** | Operators of Enterprise PX4 vehicles. Installers for Android, Ubuntu Linux, Windows and macOS |
| **Identity note** | **The closest peer to Aerora** â€” a commercial, productised station over the PX4/MAVLink ecosystem, and the clearest evidence that Aerora's chosen strategy is viable |
| **Live monitoring** | The documented design principle is to put what pilots need most front-and-centre: safety checklists, flight controls, mission planning and payload control. A telemetry panel carries speed, altitude, heading, elapsed time, distances and time-to-return. A quick-actions sidebar exposes takeoff, fly-to, orbit, return and land, with camera and gimbal control once on station |
| **Planning / visualisation** | Plan tools provide waypoint addition, survey and structure-scan insertion, and plan save/load/upload/download. **Options are shown only when relevant to the plan type** |
| **Fleet** | Vehicles register into Auterion Suite; registration is completed by scanning a QR code or following a link |
| **Payload / sensor** | Payload control is named as a front-and-centre pilot need, not a submenu |
| **AI / automation** | Mission state is tracked well enough that the station prompts the operator to start, continue or resume a loaded mission, and the action set offered is filtered by what the vehicle's current state permits |
| **Data / reporting** | Software-update notification on the GCS with a short in-place vehicle update flow |
| **Useful to Aerora** | **The most directly transferable set in the benchmark.** (a) **Context-filtered action sets** â€” offer only what the current vehicle state permits. This is the correct pattern for Aerora, and Aerora must go further: filter on *reported capability* and on *authorization*, not only on state. (b) **Payload control as a first-class surface**, which Aerora's painting profile requires. (c) **Resume/continue prompting driven by observed mission state** â€” Aerora's `AeroraMissionResumeBoundary` is the same concern with a stricter rule: no station-side payload replay. (d) Plan tools that hide irrelevant options rather than disabling them |
| **Limitations** | Proprietary; requires Enterprise PX4 and Auterion hardware; Suite registration is part of the flow. No published roles/permissions or audit model, and no state-quality vocabulary â€” the same gaps as QGC, which is Aerora's opening |

### 3.17 UAV Navigation Visionair

| Field | Content |
| --- | --- |
| **Documentation** | [uavnavigation.com Visionair](https://uavnavigation.com/products/ground-control-station/visionair-ground-control-station-gcs-software) Â· [brochure (PDF)](https://www.uavnavigation.com/sites/default/files/docs/2024-04/UAV_Navigation_Visionair_Brochure.pdf) |
| **Users / domain** | Professional and defence UAV operations. UAV Navigation is part of Grupo OesÃ­a |
| **Live monitoring** | Surrounding air traffic and obstacles are displayed with position, a danger categorisation, and supporting information including predicted trajectories. Real-time telemetry charting is a named feature |
| **Planning / visualisation** | Mission plans can be created and modified before or during flight, with calculation of an efficient waypoint distribution |
| **Fleet** | Autopilots on different platforms exchange information so a designated operator sees other vehicles in Visionair. The vendor describes simultaneous control of up to 32 UAVs from a single ground hub |
| **Payload / sensor** | Payload calibration and configuration are part of the station's remit, alongside autopilot gain adjustment |
| **AI / automation** | Autopilot-resident; the station is the planning, execution and analysis interface |
| **Data / reporting** | Every parameter sent or received over telemetry can be graphed, covering all variables in the vendor's ICD. Post-flight analysis is a stated purpose |
| **Useful to Aerora** | **(a) The strongest pattern here: every ICD variable is graphable.** The station's analysis surface is derived from the interface definition rather than from a hand-picked list, so a new telemetry item is observable the day it is defined. Aerora's `AeroraTelemetrySet` + fact-group model can do this, and should. **(b)** A **danger categorisation** attached to each traffic/obstacle item rather than a raw distance â€” a precedent for attaching a classification, not just a number. **(c)** The explicit "designated operator" concept, which is the multi-station authority question Veronte also raises and Aerora has not addressed |
| **Limitations** | Proprietary; tied to UAV Navigation autopilots; defence-oriented. Note: Aerora must not represent traffic or obstacle awareness it does not receive â€” any such display is `UNKNOWN` until a source exists |

## 4. Cross-product pattern analysis

Seventeen products, nine recurring patterns. The count is how many of the sixteen non-excluded
entries exhibit the pattern in their documentation.

| Pattern | Count | Aerora position |
| --- | --- | --- |
| Plan â†’ execute â†’ analyse as three named stages | 14 | **Adopt.** Becomes Mission Studio â†’ Live Operations â†’ Session Review |
| Automated plan generation from parameters plus constraints | 11 | **Adopt** for coverage patterns. The generator is station-side and labelled as derived |
| Mid-mission operator intervention from a bounded action set | 9 | **Adopt**, with the action set additionally filtered by reported capability and authorization |
| Pre-flight validation / simulation as a distinct gate | 7 | **Adopt.** Must report what it could *not* check (`P7-CHK-002`) |
| 3D scene or elevation-aware visualisation | 9 | **Adopt** via existing `src/Viewer3D/`. No new engine |
| Fleet inventory with persistent per-vehicle records | 6 | **Adopt**, blocked on fleet identity format |
| Cloud as the primary coordination layer | 4 | **Reject as primary.** Optional and never authorising (`P7-ARCH-010`..`013`) |
| Explicit autopilot / ground-software authority split in the product's own vocabulary | 4 | **Adopt and strengthen.** This is Aerora's architecture, stated on screen |
| Queryable telemetry store for post-flight analysis | 2 | **Adopt**, extended to record state class per value |

### Where the references are consistent and Aerora differs

Every one of the sixteen displays telemetry **values**. None of them, in the documentation located,
carries a *vocabulary* distinguishing `REPORTED` from `EFFECTIVE` from `REQUESTED` from `PROPOSED`
from `STALE` from `UNKNOWN` from `UNAVAILABLE`. Staleness appears, usually as a connection indicator
or a greyed field. The seven-state distinction does not.

That is Aerora's differentiator and its main obligation. It is also the reason Aerora cannot borrow
a reference product's instrument layout wholesale: those layouts assume a value is a value.

## 5. Patterns Aerora adopts

| # | Pattern | Source | Requirement |
| --- | --- | --- | --- |
| 1 | Scene then mission â€” model the asset once, generate passes against it | Drone Harmony | `P7-MIS-002` |
| 2 | Payload characteristics as a planning input | Phase One iX Suite | `P7-PAY-004`, `P7-MIS-002` |
| 3 | Plan whole site, segment into flyable passes, prompt for continuity | QBase 3D | `P7-MIS-002` |
| 4 | Validation as a distinct gate that reports what it could not check | QBase 3D, eMotion | `P7-CHK-002` |
| 5 | Context-filtered action sets; hide irrelevant, don't just disable | Auterion AMC | `P7-FLY-004` |
| 6 | Payload control as a first-class surface | Auterion AMC, U-See | `P7-PAY-001` |
| 7 | Two-phase collective action â€” prepare, observe, then commit | Skybrush | `P7-MOD-012`, `P7-AI-002` |
| 8 | Multiple simultaneous viewports including a list view | Skybrush | `P7-FLEET-003`, `P7-SHELL-004` |
| 9 | Frontend/backend split with the backend optionally remote | Skybrush | `P7-AI-004` boundary |
| 10 | Queryable telemetry/event store, SQL-grade | Helios GCS | `P7-TEL-004`, `P7-DAT-002` |
| 11 | Analysis surface derived from the ICD, so every defined item is graphable | Visionair | `P7-TEL-001`, `P7-LOG-003` |
| 12 | Attach a classification to an item, not only a number | Visionair | `P7-STATE-001` |
| 13 | Battery/consumable change as an explicit managed workflow step | eMotion | `P7-MIS-002`, `P7-PWR-006` |
| 14 | Operator-composable value display | Mission Planner, QGC value grid | `P7-SET-004` |
| 15 | Custom WMS / customer basemap support | Mission Planner | `P7-DAT-001` |
| 16 | On-premises deployment alongside any cloud option | DJI FlightHub 2 | `P7-ARCH-010` |
| 17 | Anomaly detection that produces a report, not an action | DJI FlightHub 2 | `P7-ARCH-002` |
| 18 | State the authority boundary plainly in the operator documentation | Mission Planner, Skybrush | `P7-HELP-003` |
| 19 | Simulated vehicles for test, labelled as simulated | Skybrush | `P7-SIM-002` |
| 20 | Data integrity as a stated product property | Veronte | `P7-AUDIT-001` |

## 6. Patterns Aerora deliberately rejects

| Rejected | Seen in | Reason |
| --- | --- | --- |
| Cloud as the coordination requirement | DJI FlightHub 2, Drone Harmony | `P7-ARCH-010`. Industrial sites are frequently offline and some customers cannot use a vendor cloud |
| Hardware lock to one vendor's aircraft | Most proprietary entries | `P7-HW-001`. Also the AiProFlight lifecycle lesson |
| Station-side predicted battery consumption and flight time | QBase 3D | `P7-ARCH-005`. No estimation method is approved. `kRemainingEnergyEstimationImplemented = false` |
| Station-implemented automatic return-to-launch | eMotion (an aircraft behaviour) | `P7-ARCH-001`, `P7-SAFE-002`. The station displays failsafes; it does not own them |
| Fork of a populated GCS application as the delivery mechanism | TitanPlanner | `P7-ARCH-023`. Aerora uses the supported overlay seams; tier-3 count stays at 0 |
| Presenting an action button before an authorized command path exists | â€” | `P7-FLY-004`. An inert button that looks live is worse than its absence |
| A single aggregated "link quality" bar | Several | `P7-RAD-002`. Per-link state, per-link freshness |
| Claiming interoperability-standard conformance | Veronte describes STANAG 4586 support as partial | `P7-HW-002`. Aerora claims nothing it has not implemented and tested |
| An LLM in the command-generation path without provenance labelling | DJI FlightHub 2 recent versions | `P7-ARCH-004`. Any station-derived suggestion is labelled as derived and is `PROPOSED` at most |

## 7. Gaps none of the references fill

Four. The first two are Aerora's product thesis; the last two are open questions the benchmark
surfaced and Aerora has not answered.

### 7.1 A state-quality vocabulary

No reference product distinguishes the seven states. **Aerora's differentiator.** Already
`IMPLEMENTED-UNCOMPILED` as `AeroraTelemetryState` + `AeroraStateValue` + `AeroraReportedValue`.

### 7.2 Operator roles, permissions and a command audit trail

None of the sixteen publishes a roles/permissions model or a command-level audit trail. Enterprise
cloud products have account-level access control, which is not the same as authorising an individual
MAVLink command at the send path. **Aerora's second differentiator**, and currently its largest gap:
the enforcement point exists and evaluates nothing (`Q-029`, ADR-014).

### 7.3 Which station holds authority when several are connected

Raised by **Veronte** (multiple interconnected stations) and **Visionair** (a designated operator
concept). Aerora supports multi-vehicle through upstream `MultiVehicleManager`, but has **no
requirement covering multi-*station* authority**.

**Action**: new `OPEN` decision, recorded in
[08](08-risk-register-and-open-decisions.md#3-new-open-decisions-raised-by-the-benchmark). This is a
benchmark finding, not a pre-existing gap someone forgot.

### 7.4 Consumable-constrained multi-pass work

QBase 3D segments large sites by flyable area and eMotion manages battery changes. Neither models a
job constrained by a **consumable carried in the payload** â€” paint, water, chemical â€” where the
remaining consumable, not the battery, determines pass boundaries.

This is specific to Aerora's painting, firefighting and agriculture profiles, and no reference
addresses it. It is a product-requirement gap: `P7-PAY-004` and `P7-MIS-002` both depend on a
reported-consumable item set that does not yet exist, and `P7-ARCH-005` prohibits inventing the
consumption model. Recorded as `OPEN`.

## 8. Sources

All entries paraphrased from the pages below. Content was rephrased for compliance with licensing
restrictions. No source is quoted beyond a short phrase, and no proprietary code, branding or visual
asset is reproduced.

| Product | Source |
| --- | --- |
| Phase One iX Suite | [geospatial.phaseone.com](https://geospatial.phaseone.com/) Â· [iX Plan Operation Guide](https://8b6065f7bd796377729f-3328e8a66c82a5c5f6aaaf6531b2a2ea.ssl.cf5.rackcdn.com/Documentation%2FGEO%2FiX%20Plan%2015.0.0%20Operation%20Guide.pdf) Â· [iX Suite launch coverage](https://www.gim-international.com/content/news/phase-one-launches-ix-suite-for-seamless-aerial-mapping-workflow) Â· [geo-matching iX Suite](https://geo-matching.com/products/ix-suite) |
| Drone Harmony | [user guide](https://www.droneharmony.com/user-guide) Â· [3D flight planner](https://www.droneharmony.com/3d-flight-planner) Â· [flight plan creation](https://www.droneharmony.com/post/3-flight-plan-creation) Â· [site management](https://www.droneharmony.com/post/2-site-management) Â· [flight execution](https://www.droneharmony.com/post/4-flight-execution) |
| QBase 3D | [quantum-systems.com](https://quantum-systems.com/) Â· [QBase Tactical](https://quantum-systems.com/qbase-tactical/) Â· [Trinity Pro / QBase 3D update](https://quantum-systems.com/news/trinity-pro-and-qbase-3d-updates-enhanced-integration-and-advanced-features/) Â· [geo-matching QBase 3D](https://geo-matching.com/products/qbase-3d) |
| eMotion | [sensefly.com eMotion 3](https://www.sensefly.com/software/emotion-3.html) Â· [ageagle.com software](https://ageagle.com/solutions/software/) Â· [geo-matching eMotion 3](https://geo-matching.com/products/emotion-3) |
| HowayGIS | [geo-matching company page](https://geo-matching.com/companies/howaygis) â€” identity only, excluded from attribution |
| AiProFlight | [AiProFlight 2.9 User Manual](https://cm.leica-geosystems.com/-/media/files/aibotix/products/aiproflight/manuals/20161110aibotixaiproflightmanual29en.ashx?hash=0F35479BE4B8F26622756400793FFF39&sc_lang=sv-se) |
| Airelectronics U-See / U-Pilot | [airelectronics.es](https://airelectronics.es/) Â· [product catalogue](https://www.aeroexpo.online/soc/airelectronics-185532.html) Â· [U-Mosaic](https://www.aeroexpo.online/prod/airelectronics/product-185532-28775.html) |
| Embention Veronte | [products](https://www.embention.com/en/products/) Â· [Veronte PCS](https://www.embention.com/en/product/pcs/) Â· [manuals](https://manuals.embention.com/bcs-software-manual/en/latest/) Â· [Command Forge / STANAG 4586](https://www.embention.com/news/veronte-ecosystem-7-6-command-forge-a-new-standard-in-interoperability-and-advanced-control/) Â· [data integrity](https://www.embention.com/en/news/software-update-6-8-more-robust-and-accessible/) |
| QGroundControl | [docs.qgroundcontrol.com](https://docs.qgroundcontrol.com/) Â· [dev guide](https://docs.qgroundcontrol.com/Stable_V5.0/en/qgc-dev-guide/index.html) Â· [repository](https://github.com/mavlink/qgroundcontrol/) Â· plus direct reading of this checkout |
| Mission Planner | [overview](https://ardupilot.org/planner/docs/mission-planner-overview.html) Â· [flight data screen](https://ardupilot.org/planner/docs/mission-planner-ground-control-station.html) Â· [flight data detail](https://en.ardupilot.org/planner/docs/mission-planner-flight-data.html) Â· [missionplanner.com](https://www.missionplanner.com/) |
| TitanPlanner | [releases](https://github.com/Titan-Dynamics/TitanPlanner/releases) |
| Helios GCS | [heliosgcs.com](https://heliosgcs.com/) Â· [repository](https://github.com/jamesagarside/helios) |
| Skybrush | [docs index](https://docs.skybrush.io/public/skybrush-live-doc/latest/index.html) Â· [features](https://docs.skybrush.io/public/skybrush-live-doc/2.9.0/features.html) Â· [hardware / architecture](https://docs.skybrush.io/public/skybrush-live-doc/2.8.1/hardware.html) Â· [show control](https://docs.skybrush.io/public/skybrush-live-doc/latest/panels/show_control.html) Â· [Live module](https://skybrush.io/modules/live/) Â· [repository](https://github.com/skybrush-io/live) |
| DJI FlightHub 2 | [FAQ](https://enterprise.dji.com/flighthub-2/faq) Â· [support / editions](https://www.dji.com/uk/support/product/flighthub-2) Â· [downloads](https://www.dji.com/de/downloads/softwares/flighthub-enterprise) Â· [cloud intelligence update](https://enterprise-insights.dji.com/blog/dji-flighthub-2-a-new-era-of-cloud-based-drone-intelligence) Â· [top features](https://enterprise-insights.dji.com/blog/flighthub-2-top-features) |
| Auterion Mission Control | [AMC overview](https://docs.auterion.com/vehicle-operation/auterion-mission-control) Â· [installation](https://docs.auterion.com/vehicle-operation/auterion-mission-control/installation) Â· [quick actions sidebar](https://docs.auterion.com/vehicle-operation/auterion-mission-control/fly/fly-view-ui-overview/quick-actions-sidebar) Â· [monitoring the flight](https://docs.auterion.com/vehicle-operation/auterion-mission-control/fly/monitoring-the-flight) Â· [plan UI overview](https://docs.auterion.com/auterion-mission-control/plan/plan-ui-overview) Â· [flying a mission](https://docs.auterion.com/vehicle-operation/auterion-mission-control/ui-breakdown/fly/flying-a-mission) |
| Visionair | [product page](https://uavnavigation.com/products/ground-control-station/visionair-ground-control-station-gcs-software) Â· [brochure](https://www.uavnavigation.com/sites/default/files/docs/2024-04/UAV_Navigation_Visionair_Brochure.pdf) Â· [v4.36 notes](https://uavnavigation.com/company/news-and-events/visionair-v436-available-now) Â· [swarming](https://uavnavigation.com/uav-swarming) Â· [version 9](https://uavnavigation.com/company/news-events/version-9-flight-control-solution) |
