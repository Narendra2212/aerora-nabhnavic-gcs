#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

/// Defined in AeroraDualPowerConfiguration.cc.
Q_DECLARE_LOGGING_CATEGORY(AeroraDualPowerLog)

class Vehicle;

/// \brief What the aircraft is **configured** for on a dual-power airframe, read from its own
/// parameters, and whether those parameters agree with each other.
///
/// Design record: 05-product-completion/16-dual-power-tether-and-battery-contract.md.
///
/// \section why Why this is not part of AeroraPowerModel
///
/// `AeroraPowerModel` carries what the vehicle **reports** - tether attachment (`IF-PWR-001`),
/// tether supply (`IF-PWR-013`), effective state (`IF-PWR-003`). This class carries what the
/// vehicle is **configured to do**, which is a different question answered by a different
/// mechanism: parameters, not telemetry. Folding the two together would put a never-observed
/// reported item next to a populated configured one and invite reading either as the other.
///
/// The separation is load-bearing on this airframe rather than tidy. Design §3 establishes that the
/// flight controller's outbound set carries **no tether message and no tether field**, so
/// attachment and supply have no producer at all, while configuration is fully readable. A single
/// "tether" surface would have to present both, and the populated half would lend credibility to
/// the empty half.
///
/// \section topology The airframe this reads
///
/// \code
/// ground 800 V DC --tether--> onboard DC-DC converter --48 V--> J10  "pack input B (tether)"
///                                                                     | F2 - D4 -+
/// battery (safe-landing reserve) ------------------------------> J9   | F1 - D3 -+--> VIN_RAW
/// \endcode
///
/// The 800 V domain terminates at the converter and never reaches the flight controller, which
/// therefore has no sensing on it - no voltage, current, converter state or tether continuity
/// (design §4.1). Both sources arrive as ~48 V pack inputs and are diode-OR'd, so failover to the
/// battery is passive, automatic and **unreported** (design §4.2).
///
/// \section interlock The interlock this exists to surface
///
/// There is no single "tethered" setting. There are two independent flash parameters, each set by
/// hand over MAVLink while disarmed, with unrelated effects and nothing enforcing their
/// consistency:
///
/// \li \c BAT_TETHERED - power. "1 = tether power: no battery sensor needed, no battery failsafe,
///     SoC unknown."
/// \li \c TETHER_MODE - navigation. Clamps the fence radius to \c FENCE_TETHER_RADIUS_M.
///
/// Both inconsistent combinations are reachable and neither is rejected. The dangerous one is
/// tether power with an untethered fence: the energy failsafe is disarmed while the fence still
/// permits flight beyond tether length. isInterlockConsistent() reports that disagreement and
/// nothing more - see \c kStationInitiatedPowerActionImplemented.
///
/// \section reserve Why a configured tether makes the reserve unknown
///
/// The battery is carried for safe landing. But \c BAT_TETHERED = 1 disables the battery sensor and
/// the failsafe, so during tethered flight - exactly while the battery is serving as a reserve -
/// its charge is not tracked. Combined with the silent diode failover, the aircraft can end up
/// flying on a reserve whose state nobody has observed (design §4.4).
///
/// isReserveCapacityUnknown() exists so a consumer can say so, instead of showing a stale, zero or
/// last-known state of charge that would read as a live reserve figure. Whether tethered flight may
/// proceed on that basis is a product and safety judgement recorded in design §7; this reports the
/// unknown under either answer.
///
/// \section absence Absence is not false
///
/// Every reading is a \c ...Known() / value pair rather than a single boolean. A vehicle that is
/// not this firmware, or whose parameters have not loaded, must not read as "battery power, fence
/// untethered, interlock consistent" - which is what a bare `false` would say, and it is the most
/// reassuring of the possible answers. Unknown is reported as unknown.
class AeroraDualPowerConfiguration : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Constructed by the Aerora vehicle integration layer")

    Q_PROPERTY(bool powerSourceKnown READ isPowerSourceKnown NOTIFY configurationChanged)
    Q_PROPERTY(bool tetherPowerConfigured READ isTetherPowerConfigured NOTIFY configurationChanged)

    Q_PROPERTY(bool fenceModeKnown READ isFenceModeKnown NOTIFY configurationChanged)
    Q_PROPERTY(bool tetherFenceConfigured READ isTetherFenceConfigured NOTIFY configurationChanged)

    Q_PROPERTY(bool fenceRadiusKnown READ isFenceRadiusKnown NOTIFY configurationChanged)
    Q_PROPERTY(float tetherFenceRadiusMetres READ tetherFenceRadiusMetres NOTIFY configurationChanged)

    Q_PROPERTY(bool interlockKnown READ isInterlockKnown NOTIFY configurationChanged)
    Q_PROPERTY(bool interlockConsistent READ isInterlockConsistent NOTIFY configurationChanged)

    Q_PROPERTY(bool reserveCapacityUnknown READ isReserveCapacityUnknown NOTIFY configurationChanged)

public:
    /// The flight controller parameter that selects tether power.
    ///
    /// Matched against a MAVLink `PARAM_VALUE` / `PARAM_SET` `param_id`, so unlike the service keys
    /// elsewhere in the overlay this is a value on the wire. It is **not authored here**: it is the
    /// firmware's own name, from a `PARAM_TABLE.md` generated by
    /// `harness_scripts/gen_param_table_md.m`.
    static constexpr const char* kTetheredPowerParameter = "BAT_TETHERED";

    /// The flight controller parameter that selects the tethered fence. Firmware-owned name; see
    /// \c kTetheredPowerParameter.
    static constexpr const char* kTetherFenceModeParameter = "TETHER_MODE";

    /// The flight controller parameter carrying the tethered fence radius in metres.
    ///
    /// **This is the wire name, which differs from the dictionary name.** `PARAM_TABLE.md` lists
    /// parameter 204 as `FENCE_TETHER_RADIUS_M`, which is 21 characters and cannot be a MAVLink
    /// `param_id` - that field is 16. `fw_lib/param_defs.m:245` holds the name actually sent, as a
    /// 16-byte zero-padded array decoding to `FENCE_TETH_R_M`. The firmware's own note records the
    /// rule: names are at most 16 characters, with long words abbreviated.
    ///
    /// So `param_defs.m` is the source of truth for anything matched against a `param_id`, and
    /// `PARAM_TABLE.md` is the source of truth for ids, units, ranges and ownership. Using the
    /// dictionary name here would have failed silently - `parameterExists()` would simply never
    /// match, leaving the reading permanently unknown with no error anywhere.
    ///
    /// The firmware marks its default of 30 m **`(PLACEHOLDER)`**, so a consumer must present the
    /// value as unconfirmed rather than as a settled limit.
    static constexpr const char* kTetherFenceRadiusParameter = "FENCE_TETH_R_M";

    /// The longest a MAVLink `param_id` can be, so a name this module sends can be checked against
    /// it rather than trusted. Protocol-fixed by `PARAM_VALUE` / `PARAM_SET`, not an Aerora choice.
    static constexpr int kMaxParameterNameLength = 16;

    /// The number of voltage, current, power, energy or timing thresholds this class defines.
    ///
    /// Zero, and pinned so it stays zero, for the same reason `AeroraPowerModel` pins its own: the
    /// station has no authority to enforce a power limit, and every figure this class can reach is
    /// either the firmware's or marked a placeholder by it.
    static constexpr int kDefinedThresholdCount = 0;

    /// The ground station initiates no power action and writes no parameter from here.
    ///
    /// `AERORA-POWER-REQ-008`. This class reads parameters and reports; it has no sink, no
    /// `Q_INVOKABLE` and no write path. Reporting the \c isInterlockConsistent() disagreement is
    /// deliberately not the same as correcting it - the aircraft accepts a parameter write only
    /// while disarmed, and which of the two parameters is the mistaken one is not knowable here.
    static constexpr bool kStationInitiatedPowerActionImplemented = false;

    /// Whether the station infers the live power source from any measurement.
    ///
    /// False, and not merely unimplemented. Design §4.2: the INA228 sits after the diodes and reads
    /// `VIN_RAW`, and a 48 V converter output falls inside a 14S pack's own 46.2-58.8 V range, so
    /// rail voltage cannot separate the two sources. This class reports the **configured** source
    /// and never guesses the live one.
    static constexpr bool kLivePowerSourceInferenceImplemented = false;

    /// Whether the station estimates remaining reserve capacity. False - see \c isReserveCapacityUnknown().
    static constexpr bool kReserveCapacityEstimationImplemented = false;

    /// Whether the two interlock parameters agree, as a pure function of their values.
    ///
    /// A static seam so the verdict is testable without a `Vehicle` and a loaded parameter set,
    /// following the same pattern as `boardUidFromAutopilotVersion()` and
    /// `reportFleetIdentityFrom()`. The instance accessor adds only the known-ness gate.
    [[nodiscard]] static bool interlockConsistentFrom(bool tetherPowerConfigured, bool tetherFenceConfigured)
    {
        return tetherPowerConfigured == tetherFenceConfigured;
    }

    /// Whether the reserve's capacity is unknown, as a pure function of the power-source reading.
    ///
    /// Static for the same reason as interlockConsistentFrom(). Unknown when the configuration
    /// could not be read, which is the fail-closed direction, and unknown when tether power is
    /// configured, because that setting disables the battery sensor.
    [[nodiscard]] static bool reserveCapacityUnknownFrom(bool powerSourceKnown, bool tetherPowerConfigured)
    {
        return !powerSourceKnown || tetherPowerConfigured;
    }

    /// \param vehicle The vehicle whose parameters are read. May be null, which is the mock and
    ///                unit-test path; every reading then stays unknown.
    /// \param parent  Owner.
    explicit AeroraDualPowerConfiguration(Vehicle* vehicle, QObject* parent = nullptr);
    ~AeroraDualPowerConfiguration() override;

    AeroraDualPowerConfiguration(const AeroraDualPowerConfiguration&) = delete;
    AeroraDualPowerConfiguration& operator=(const AeroraDualPowerConfiguration&) = delete;
    AeroraDualPowerConfiguration(AeroraDualPowerConfiguration&&) = delete;
    AeroraDualPowerConfiguration& operator=(AeroraDualPowerConfiguration&&) = delete;

    /// Whether \c BAT_TETHERED could be read. False for a vehicle that is not this firmware, or
    /// before its parameters load.
    [[nodiscard]] bool isPowerSourceKnown() const { return _powerSourceKnown; }

    /// Whether the aircraft is configured for tether power. Meaningful only while
    /// isPowerSourceKnown(); false otherwise, and that false carries no claim.
    [[nodiscard]] bool isTetherPowerConfigured() const { return _tetherPowerConfigured; }

    /// Whether \c TETHER_MODE could be read.
    [[nodiscard]] bool isFenceModeKnown() const { return _fenceModeKnown; }

    /// Whether the aircraft is configured for the tethered fence. Meaningful only while
    /// isFenceModeKnown().
    [[nodiscard]] bool isTetherFenceConfigured() const { return _tetherFenceConfigured; }

    /// Whether \c FENCE_TETHER_RADIUS_M could be read.
    [[nodiscard]] bool isFenceRadiusKnown() const { return _fenceRadiusKnown; }

    /// The configured tethered fence radius in metres, or 0 while unknown.
    ///
    /// The firmware marks its default a placeholder; a consumer must say so rather than present it
    /// as a settled tether length.
    [[nodiscard]] float tetherFenceRadiusMetres() const { return _tetherFenceRadiusMetres; }

    /// Whether both interlock parameters could be read, so a verdict exists at all.
    [[nodiscard]] bool isInterlockKnown() const { return _powerSourceKnown && _fenceModeKnown; }

    /// Whether \c BAT_TETHERED and \c TETHER_MODE agree.
    ///
    /// Meaningful only while isInterlockKnown(). True when both select tethered operation or
    /// neither does. False is a real misconfiguration on the aircraft, not a station fault - see
    /// the class note on the interlock.
    [[nodiscard]] bool isInterlockConsistent() const;

    /// Whether the safe-landing reserve's capacity is unknown.
    ///
    /// True whenever the aircraft is configured for tether power, because \c BAT_TETHERED = 1
    /// disables the battery sensor and the failsafe. Also true while isPowerSourceKnown() is false,
    /// which is the fail-closed direction: an unread configuration must not imply a monitored
    /// reserve.
    [[nodiscard]] bool isReserveCapacityUnknown() const;

    /// Re-reads every parameter and emits configurationChanged() if any reading moved.
    ///
    /// Called on construction, when the vehicle's parameters become ready, and when one of the
    /// three parameters changes. Public so a test can drive it without a parameter signal.
    void refresh();

signals:
    /// Any reading changed. One signal rather than one per property: the readings are read together
    /// from one parameter set, and the interlock verdict depends on two of them, so a consumer that
    /// reacted to them individually would observe intermediate states that never held on the
    /// aircraft.
    void configurationChanged();

private:
    /// Reads a parameter as a boolean. \a known is set false when the vehicle, its parameter
    /// manager, or the parameter itself is absent.
    [[nodiscard]] bool _readBool(const char* name, bool& known) const;

    /// Reads a parameter as a float. \a known is set false when it cannot be reached.
    [[nodiscard]] float _readFloat(const char* name, bool& known) const;

    void _connectParameterSignals();

    QPointer<Vehicle> _vehicle;

    bool _powerSourceKnown = false;
    bool _tetherPowerConfigured = false;

    bool _fenceModeKnown = false;
    bool _tetherFenceConfigured = false;

    bool _fenceRadiusKnown = false;
    float _tetherFenceRadiusMetres = 0.0F;
};
