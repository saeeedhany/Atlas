#pragma once

#include <QString>
#include <QStringList>

#include <vector>

#include "atlas/core/enums.hpp"
#include "atlas/core/memory.hpp"

namespace atlas::viewmodels {

struct SessionTip {
    QString text;
    QString source;
};

QString linkPrompt(atlas::core::RelationshipType type, const QString& source, const QString& target);
QStringList certaintyNames();
QStringList calibrationLines(const std::vector<atlas::core::ReviewEvent>& events);
SessionTip chooseTip(const std::vector<atlas::core::ReviewEvent>& events);

}  // namespace atlas::viewmodels
