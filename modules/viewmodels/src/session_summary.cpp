#include "atlas/viewmodels/session_summary.hpp"

#include <algorithm>
#include <unordered_set>

#include "atlas/learning/calibration.hpp"

namespace atlas::viewmodels {

using atlas::core::Certainty;
using atlas::core::Exercise;
using atlas::core::Grade;
using atlas::core::ItemRef;
using atlas::core::RelationshipType;
using atlas::core::ReviewEvent;

QString linkPrompt(RelationshipType type, const QString& source, const QString& target) {
    switch (type) {
        case RelationshipType::DependsOn: return QString("Why does %1 depend on %2?").arg(source, target);
        case RelationshipType::Uses: return QString("Why does %1 use %2?").arg(source, target);
        case RelationshipType::Implements: return QString("How does %1 implement %2?").arg(source, target);
        case RelationshipType::Solves: return QString("How does %1 solve %2?").arg(source, target);
        case RelationshipType::Contains: return QString("Why does %1 contain %2?").arg(source, target);
        case RelationshipType::PartOf: return QString("Why is %1 part of %2?").arg(source, target);
        case RelationshipType::RelatedTo: return QString("How is %1 related to %2?").arg(source, target);
        case RelationshipType::AlternativeTo: return QString("When would you pick %1 over %2?").arg(source, target);
        case RelationshipType::OppositeOf: return QString("How is %1 the opposite of %2?").arg(source, target);
        case RelationshipType::Causes: return QString("How does %1 cause %2?").arg(source, target);
    }
    return QString("How are %1 and %2 connected?").arg(source, target);
}

int itemsReviewed(const std::vector<ReviewEvent>& events) {
    std::unordered_set<ItemRef> items;
    for (const auto& event : events) items.insert(event.item);
    return static_cast<int>(items.size());
}

int itemsRecalled(const std::vector<ReviewEvent>& events) {
    std::unordered_set<ItemRef> seen;
    int recalled = 0;
    for (const auto& event : events) {
        if (seen.insert(event.item).second && event.grade != Grade::Again) ++recalled;
    }
    return recalled;
}

QStringList certaintyNames() { return {"Guess", "Unsure", "Fairly sure", "Certain"}; }

QStringList calibrationLines(const std::vector<ReviewEvent>& events) {
    auto calibration = atlas::learning::learnerCalibration(events);
    QStringList names = certaintyNames();
    QStringList lines;
    for (int level = 3; level >= 0; --level) {
        const auto& stats = calibration.levels[static_cast<size_t>(level)];
        if (stats.attempts == 0) continue;
        lines.append(QString("%1 %2 %3, right %4")
                         .arg(names[level])
                         .arg(stats.attempts)
                         .arg(stats.attempts == 1 ? "time" : "times")
                         .arg(stats.successes));
    }
    return lines;
}

SessionTip chooseTip(const std::vector<ReviewEvent>& events) {
    auto any = [&](auto predicate) { return std::any_of(events.begin(), events.end(), predicate); };
    if (any([](const ReviewEvent& e) { return e.predicted == Certainty::Certain && e.grade == Grade::Again; })) {
        return {"Confident mistakes are the easiest to fix. Look at those again tomorrow.", "Butterfield & Metcalfe 2001"};
    }
    if (any([](const ReviewEvent& e) { return e.wrongTarget.has_value(); })) {
        return {"Ideas you mix up are worth studying side by side, so their differences stand out.", "Rohrer & Taylor 2007"};
    }
    auto rebuilds = std::count_if(events.begin(), events.end(), [](const ReviewEvent& e) { return e.exercise == Exercise::Rebuild; });
    auto hinted = std::count_if(events.begin(), events.end(), [](const ReviewEvent& e) {
        return e.exercise == Exercise::Rebuild && e.hintsUsed > 0;
    });
    if (hinted > 0 && hinted * 2 >= rebuilds) {
        return {"Struggling to recall is part of learning. Try a little longer before taking a hint.", "Bjork 1994"};
    }
    return {"Recalling beats rereading. A short session on most days works best.", "Roediger & Karpicke 2006"};
}

}  // namespace atlas::viewmodels
