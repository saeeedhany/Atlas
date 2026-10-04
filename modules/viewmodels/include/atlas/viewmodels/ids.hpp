#pragma once

#include <QString>

#include <optional>
#include <string>

#include "atlas/core/strong_id.hpp"

namespace atlas::viewmodels {

QString toQString(const std::string& text);
std::string toStdString(const QString& text);

template <typename Tag>
QString idString(const atlas::core::StrongId<Tag>& id) {
    return toQString(id.toString());
}

template <typename Id>
std::optional<Id> parseId(const QString& text) {
    auto uuid = atlas::core::Uuid::parse(toStdString(text));
    if (!uuid) return std::nullopt;
    return Id(*uuid);
}

}  // namespace atlas::viewmodels
