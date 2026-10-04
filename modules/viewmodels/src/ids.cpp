#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

QString toQString(const std::string& text) { return QString::fromStdString(text); }

std::string toStdString(const QString& text) { return text.toStdString(); }

}  // namespace atlas::viewmodels
