#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>

#include <vector>

namespace atlas::render {

QRectF regionBounds(const std::vector<QPointF>& points, double padding);
int regionHue(const QString& key);
std::vector<QPointF> roundedRectOutline(const QRectF& rect, double radius, int cornerSegments);

}  // namespace atlas::render
