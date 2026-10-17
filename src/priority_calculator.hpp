#ifndef MEDIRUSH_PRIORITY_CALCULATOR_HPP
#define MEDIRUSH_PRIORITY_CALCULATOR_HPP

namespace medirush {

/** Only verified system data is allowed into this structure. Patient-written notes are excluded. */
struct PriorityContext {
    int criticality{1};
    bool verified{false};
    double hoursRemaining{24.0};
    double minutesWaiting{0.0};
    bool overrideActive{false};
    bool emergency{false};
};

/** Simple, configurable implementation of the MediRush priority formula. */
class PriorityCalculator {
public:
    struct Settings {
        double urgencyMax{10.0};
        double urgencyWindowHours{48.0};
        double agingRatePerMinute{0.02};
        double agingCap{12.0};
        double overrideBonus{25.0};
        double unverifiedMultiplier{0.3};
        double unverifiedCeiling{3.0};
        double emergencyBoost{100.0};
    };

    PriorityCalculator();
    explicit PriorityCalculator(Settings settings);
    /** Calculate a score; unverified orders are always clamped to the configured ceiling. */
    [[nodiscard]] double calculate(const PriorityContext& context) const;

private:
    Settings settings_;
};

} // namespace medirush

#endif
