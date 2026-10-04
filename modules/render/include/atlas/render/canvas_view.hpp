#pragma once

#include <QPointF>
#include <QRectF>

namespace atlas::render {

QRectF viewportInWorld(double offsetX, double offsetY, double scale, double width, double height);
QRectF cullRectFor(const QRectF& viewport);
bool segmentMayCross(const QRectF& rect, QPointF from, QPointF to);
double distanceToSegment(QPointF point, QPointF from, QPointF to);

}  // namespace atlas::render
