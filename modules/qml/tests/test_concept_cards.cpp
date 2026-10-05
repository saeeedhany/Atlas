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

TEST_CASE("a detached card stays inside the board") {
    CardFixture p;
    p.select(p.a);
    REQUIRE(QMetaObject::invokeMethod(p.card, "detachTo", Q_ARG(double, -500.0), Q_ARG(double, 99999.0)));
    CHECK(p.card->property("x").toDouble() >= 0.0);
    CHECK(p.card->property("y").toDouble() >= 0.0);
    CHECK(p.card->property("x").toDouble() + p.card->property("width").toDouble() <= p.overlay->property("width").toDouble() + 1e-6);
    CHECK(p.card->property("y").toDouble() + p.card->property("height").toDouble() <= p.overlay->property("height").toDouble() + 1e-6);
}

TEST_CASE("pinned summaries never overlap") {
    CardFixture p;
    for (const QString& id : {p.a, p.b, p.c}) {
        p.select(id);
        bool pinned = false;
        REQUIRE(QMetaObject::invokeMethod(p.overlay, "pinCurrent", Q_RETURN_ARG(bool, pinned)));
    }
    p.select(p.e);
    auto summaries = p.window->findChildren<QObject*>("pinnedCard");
    REQUIRE(summaries.size() == 3);
    auto rectOf = [](QObject* o) {
        return QRectF(o->property("x").toDouble(), o->property("y").toDouble(), o->property("width").toDouble(),
                      o->property("height").toDouble());
    };
    for (int i = 0; i < summaries.size(); ++i)
        for (int j = i + 1; j < summaries.size(); ++j)
            CHECK_FALSE(rectOf(summaries[i]).intersects(rectOf(summaries[j])));
    for (QObject* s : summaries)
        CHECK_FALSE(rectOf(s).intersects(rectOf(p.card)));
}

TEST_CASE("a card at the same node position lands below the first") {
    CardFixture p;
    QVariant first;
    QVariant second;
    QVariant point = QVariant::fromValue(QPointF(300, 300));
    REQUIRE(QMetaObject::invokeMethod(p.overlay, "placeClear", Q_RETURN_ARG(QVariant, first), Q_ARG(QVariant, point),
                                      Q_ARG(QVariant, 240), Q_ARG(QVariant, 200), Q_ARG(QVariant, QVariantList{})));
    REQUIRE(QMetaObject::invokeMethod(p.overlay, "placeClear", Q_RETURN_ARG(QVariant, second), Q_ARG(QVariant, point),
                                      Q_ARG(QVariant, 240), Q_ARG(QVariant, 200),
                                      Q_ARG(QVariant, QVariantList{first})));
    CHECK_FALSE(first.toRectF().intersects(second.toRectF()));
}

TEST_CASE("the pin button shows the pinned state and a fourth pin explains the limit") {
    CardFixture p;
    p.select(p.a);
    CHECK_FALSE(p.card->property("pinned").toBool());
    REQUIRE(QMetaObject::invokeMethod(p.card, "pinRequested"));
    CHECK(p.card->property("pinned").toBool());
    CHECK(QmlFixture::child(p.card, "cardPinButton")->property("checked").toBool());
    for (const QString& id : {p.b, p.c, p.d}) {
        p.select(id);
        REQUIRE(QMetaObject::invokeMethod(p.card, "pinRequested"));
    }
    QmlFixture::settle();
    CHECK_FALSE(p.card->property("pinned").toBool());
    auto* toast = QmlFixture::child(p.window.get(), "toast");
    CHECK(toast->property("shown").toBool());
    CHECK(toast->property("text").toString() == "Up to three concepts can stay pinned");
}
