# 09 — AI Architecture

## Initial technologies

- OpenCV
- TensorFlow
- Jetson/companion compute where applicable

## Pipeline

```text
Sensor/video
 -> acquisition
 -> timestamp/frame validation
 -> preprocessing
 -> inference
 -> confidence/quality validation
 -> application decision layer
 -> operator/mission integration
```

## AI result contract

Every result should include:
- model ID/version
- timestamp
- confidence
- coordinate/frame
- validity
- optional uncertainty
- source frame/reference

## Safety

AI output is not trusted simply because a model returned a result.

AI failure modes:
- no model
- model crash
- stale result
- low confidence
- corrupted input
- overloaded compute
- thermal/resource limitation
- incompatible model

Each requires defined degraded behavior.

## Product-specific AI role

The product specification explicitly requires an onboard GPU to make intelligent decisions for painting and future industrial missions. The GCS therefore needs an **AI observability and authority model** rather than simply an AI dashboard.

The GCS shall distinguish:
- AI available/unavailable
- model identity/version
- inference freshness
- confidence/quality
- proposed action
- accepted action
- effective vehicle state
- operator override
- degraded mode

The GCS shall not assume that a GPU decision is safe merely because the GPU is connected.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
