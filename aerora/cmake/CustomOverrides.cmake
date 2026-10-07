# ============================================================================
# Aerora Build Configuration Overrides
#
# Required by the overlay contract in the top-level CMakeLists.txt, which treats a
# missing cmake/CustomOverrides.cmake as a fatal error.
#
# Intentionally minimal. Layer 1 covers the security and command boundary only, so no
# option is overridden yet. Each omission below is a decision, not an oversight.
# ============================================================================

# Branding (app name, icons, installer assets): NOT overridden.
#   Layer 1 changes no user-visible identity. Branding belongs with the UI work.

# QGC_DISABLE_APM_PLUGIN_FACTORY / QGC_DISABLE_PX4_PLUGIN_FACTORY: NOT set.
#   custom-example disables both to install a single vendor factory. Aerora must not,
#   yet: Q4-004 (consequences of disabling both vendor factories) is unresolved, and
#   routing every unknown autopilot into Aerora via the generic plugin is explicitly
#   out of bounds. Revisit with the firmware plugin work.

# QGC_ENABLE_WERROR: left at its default (ON).
#   Aerora does not lower the core warning bar.

# Settings version: NOT bumped.
#   Layer 1 adds no persisted settings schema change. The signing-key settings layout is
#   unchanged by ADR-013 - same group, same manifest, same timestamp paths - so no
#   migration and no version bump is required.
