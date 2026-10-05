#include <QPointF>
#include <QRectF>
#include <memory>

#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

struct CardFixture {
    QmlFixture f;
    QString a = f.context().map().createConcept("Alpha");
    QString b = f.context().map().createConcept("Beta");
    QString c = f.context().map().createConcept("Gamma");
    QString d = f.context().map().createConcept("Delta");
    QString e = f.context().map().createConcept("Epsilon");
    std::unique_ptr<QObject> window;
    QObject* overlay = nullptr;
    QObject* card = nullptr;

    CardFixture() {
        f.context().settings().setReducedMotion(true);
        window = f.create("Main");
        QmlFixture::settle();
        overlay = QmlFixture::child(window.get(), "mapOverlay");
        card = QmlFixture::child(window.get(), "conceptCard");
    }

    void select(const QString& id) {
        f.context().map().setSelectedId(id);
        QmlFixture::settle();
    }

    QRectF beside(double x, double y) {
        QVariant rect;
        REQUIRE(QMetaObject::invokeMethod(overlay, "besideRect", Q_RETURN_ARG(QVariant, rect),
                                          Q_ARG(QVariant, QVariant::fromValue(QPointF(x, y))), Q_ARG(QVariant, 380),
                                          Q_ARG(QVariant, 500), Q_ARG(QVariant, 1200), Q_ARG(QVariant, 800)));
        return rect.toRectF();
    }
};

}  // namespace

TEST_CASE("cards open beside their node and flip near the right edge") {
    CardFixture p;
    QRectF right = p.beside(300, 300);
    CHECK(right.left() == doctest::Approx(328.0));
    QRectF flipped = p.beside(1000, 300);
    CHECK(flipped.right() <= 1000.0 - 28.0 + 1e-6);
    CHECK(flipped.left() >= 16.0);
    QRectF low = p.beside(300, 790);
    CHECK(low.bottom() <= 800.0 - 16.0 + 1e-6);
}

TEST_CASE("the editable card follows the selection and can detach and expand") {
    CardFixture p;
    p.select(p.a);
    CHECK(p.card->property("visible").toBool());
    REQUIRE(QMetaObject::invokeMethod(p.card, "detachTo", Q_ARG(double, 60.0), Q_ARG(double, 70.0)));
    CHECK(p.card->property("detached").toBool());
    CHECK(p.card->property("x").toDouble() == doctest::Approx(60.0));
    REQUIRE(QMetaObject::invokeMethod(p.card, "expand"));
    CHECK(p.card->property("width").toDouble() > 900.0);
    p.select(p.b);
    CHECK_FALSE(p.card->property("detached").toBool());
    CHECK_FALSE(p.card->property("expanded").toBool());
}

TEST_CASE("up to three concepts stay pinned as summaries") {
    CardFixture p;
    for (const QString& id : {p.a, p.b, p.c, p.d}) {
        p.select(id);
        bool pinned = false;
        REQUIRE(QMetaObject::invokeMethod(p.overlay, "pinCurrent", Q_RETURN_ARG(bool, pinned)));
        CHECK(pinned == (id != p.d));
    }
    p.select(p.e);
    auto summaries = p.window->findChildren<QObject*>("pinnedCard");
    CHECK(summaries.size() == 3);
    REQUIRE(QMetaObject::invokeMethod(summaries[0], "edit"));
    QmlFixture::settle();
    CHECK(p.f.context().map().selectedId() == summaries[0]->property("conceptId").toString());
}
