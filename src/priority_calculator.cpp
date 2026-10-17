#include "priority_calculator.hpp"

#include <algorithm>
#include <stdexcept>

namespace medirush {

PriorityCalculator::PriorityCalculator() : PriorityCalculator(Settings{}) {}

PriorityCalculator::PriorityCalculator(Settings settings) : settings_(settings) {
    if (settings_.urgencyWindowHours <= 0.0 || settings_.agingRatePerMinute < 0.0 ||
        settings_.agingCap < 0.0 || settings_.unverifiedMultiplier < 0.0 ||
        settings_.unverifiedCeiling < 0.0) {
        throw std::invalid_argument("Invalid priority calculator settings");
    }
}

double PriorityCalculator::calculate(const PriorityContext& context) const {
    const int criticality = std::clamp(context.criticality, 1, 10);
    const double multiplier = context.verified ? 1.0 : settings_.unverifiedMultiplier;
    const double hoursRemaining = std::max(0.0, context.hoursRemaining);
    const double urgency = settings_.urgencyMax *
        std::max(0.0, 1.0 - hoursRemaining / settings_.urgencyWindowHours);
    const double waiting = std::max(0.0, context.minutesWaiting);
    const double aging = std::min(settings_.agingCap, settings_.agingRatePerMinute * waiting);
    const double overrideBonus = context.overrideActive ? settings_.overrideBonus : 0.0;

    double score = static_cast<double>(criticality) * multiplier + urgency + aging + overrideBonus;
    if (context.emergency) {
        score += settings_.emergencyBoost;
    }
    // Apply the verification safety ceiling to the final strategy result.
    if (!context.verified) {
        score = std::min(score, settings_.unverifiedCeiling);
    }
    return score;
}

} // namespace medirush
