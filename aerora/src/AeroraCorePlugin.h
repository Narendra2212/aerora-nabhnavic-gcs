#pragma once

#include "QGCCorePlugin.h"

/// \brief Aerora core plugin. Binds the Aerora security foundation to QGC's extension points.
///
/// Layer 1 scope: this overrides only what the security and command boundary needs. Branding,
/// UI, firmware factories and resource overrides are deliberately untouched - they belong to
/// the overlay foundation work, and the vendor firmware factories in particular must not be
/// disabled while Q4-004 is unresolved.
///
class AeroraCorePlugin : public QGCCorePlugin
{
    Q_OBJECT

public:
    static AeroraCorePlugin *instance();

    explicit AeroraCorePlugin(QObject *parent = nullptr);
    ~AeroraCorePlugin() override;

    void init() override;

    /// Delegates to AeroraCommandAuthorizer when an authorization policy is configured.
    ///
    /// While no policy is configured (Q-029 is OPEN) this returns true and warns once. That is a
    /// recorded gap, not a design position: the enforcement point exists in the core send path and
    /// the chain is wired, but it cannot evaluate without an approved role policy. The two
    /// alternatives were rejected - denying every command makes the station unusable, and claiming
    /// a boundary that performs no evaluation would be dishonest.
    bool authorizeMavCommand(Vehicle *vehicle, int targetCompId, int command, bool commandInt,
                             float param1, float param2, float param3, float param4,
                             double param5, double param6, float param7) override;

    /// True when Aerora is actually evaluating command authorization. False means no policy is
    /// installed and no authorization decisions are being made.
    [[nodiscard]] static bool isAuthorizationEnforced();

private:
    void _installSigningKeyStore();

    bool _warnedUnconfigured = false;
};
