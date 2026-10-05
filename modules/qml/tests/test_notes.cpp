#include <QColor>
#include <QPointF>
#include <QSignalSpy>

#include "atlas/viewmodels/notes_model.hpp"
#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

struct NoteFixture {
    QmlFixture f;
    QString concept_ = f.context().map().createConcept("Paging");
    std::unique_ptr<QObject> window;
    QObject* canvas = nullptr;
    QObject* layer = nullptr;

    NoteFixture() {
        f.context().settings().setReducedMotion(true);
        window = f.create("Main");
        QmlFixture::settle();
        canvas = QmlFixture::child(window.get(), "graphCanvas");
        layer = QmlFixture::child(window.get(), "notesLayer");
    }

    QPointF toScreen(double x, double y) {
        QPointF point;
        REQUIRE(QMetaObject::invokeMethod(canvas, "mapToScreen", Q_RETURN_ARG(QPointF, point), Q_ARG(double, x),
                                          Q_ARG(double, y)));
        return point;
    }
};

}  // namespace

TEST_CASE("double clicking empty board creates a note there") {
    NoteFixture n;
    QPointF empty = n.toScreen(2000, 2000);
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "handleDoubleClick", Q_ARG(double, empty.x()), Q_ARG(double, empty.y())));
    QmlFixture::settle();
    REQUIRE(n.f.context().notes().count() == 1);
    CHECK(n.window->findChildren<QObject*>("stickyNote").size() == 1);
    CHECK(n.layer->property("noteItems").toList().size() == 1);
}

TEST_CASE("typing note: in the bar creates a note with that text") {
    NoteFixture n;
    auto* bar = QmlFixture::child(n.window.get(), "topicBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "addConcept", Q_ARG(QString, "Note: Why half full?")));
    QmlFixture::settle();
    REQUIRE(n.f.context().notes().count() == 1);
    auto id = n.f.context().notes().data(n.f.context().notes().index(0), atlas::viewmodels::NotesModel::IdRole).toString();
    CHECK(n.f.context().notes().note(id).value("body").toString() == "Why half full?");
    CHECK(n.f.context().map().conceptCount() == 1);
}

TEST_CASE("notes keep their world place while the board zooms") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(100, 100, "Stay");
    QmlFixture::settle();
    auto* note = QmlFixture::child(n.window.get(), "stickyNote");
    double width = note->property("width").toDouble();
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "zoomAt", Q_ARG(double, 2.0), Q_ARG(double, 400.0), Q_ARG(double, 300.0)));
    QmlFixture::settle();
    CHECK(n.f.context().notes().note(id).value("x").toDouble() == doctest::Approx(100.0));
    CHECK(note->property("width").toDouble() == doctest::Approx(width * 2.0).epsilon(0.02));
}

TEST_CASE("dragging and linking a note use board positions") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Link me");
    QmlFixture::settle();
    QPointF target = n.toScreen(500, 500);
    REQUIRE(QMetaObject::invokeMethod(n.layer, "moveNoteTo", Q_ARG(QString, id), Q_ARG(double, target.x()),
                                      Q_ARG(double, target.y())));
    CHECK(n.f.context().notes().note(id).value("x").toDouble() == doctest::Approx(500.0));

    QVariant point;
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "screenPositionOf", Q_RETURN_ARG(QVariant, point),
                                      Q_ARG(QString, n.concept_)));
    QPointF node = point.toPointF();
    bool linked = false;
    REQUIRE(QMetaObject::invokeMethod(n.layer, "linkNoteAt", Q_RETURN_ARG(bool, linked), Q_ARG(QString, id),
                                      Q_ARG(double, node.x()), Q_ARG(double, node.y())));
    CHECK(linked);
    auto links = n.f.context().notes().note(id).value("links").toList();
    REQUIRE(links.size() == 1);
    CHECK(links[0].toMap().value("targetId").toString() == n.concept_);
}

TEST_CASE("notes recolor with the theme") {
    NoteFixture n;
    n.f.context().notes().createNote(0, 0, "Color");
    QmlFixture::settle();
    auto* note = QmlFixture::child(n.window.get(), "stickyNote");
    QColor dark = note->property("color").value<QColor>();
    n.f.context().settings().setDarkTheme(false);
    CHECK(note->property("color").value<QColor>() != dark);
}

TEST_CASE("note errors reach the toast") {
    NoteFixture n;
    emit n.f.context().notes().errorOccurred("Could not save the note");
    auto* toast = QmlFixture::child(n.window.get(), "toast");
    CHECK(toast->property("shown").toBool());
    CHECK(toast->property("text").toString() == "Could not save the note");
}

TEST_CASE("releasing a link drag on a concept links the note") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Drag");
    QmlFixture::settle();
    QVariant point;
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "screenPositionOf", Q_RETURN_ARG(QVariant, point),
                                      Q_ARG(QString, n.concept_)));
    QPointF node = point.toPointF();
    REQUIRE(QMetaObject::invokeMethod(n.layer, "beginDraft", Q_ARG(QString, id), Q_ARG(QPointF, QPointF(1, 1))));
    REQUIRE(QMetaObject::invokeMethod(n.layer, "moveDraft", Q_ARG(QPointF, node)));
    CHECK(n.layer->property("draftNoteId").toString() == id);
    REQUIRE(QMetaObject::invokeMethod(n.layer, "endDraft", Q_ARG(QPointF, node)));
    QmlFixture::settle();
    CHECK(n.layer->property("draftNoteId").toString().isEmpty());
    CHECK(n.f.context().notes().note(id).value("links").toList().size() == 1);
}

TEST_CASE("link lines follow a note while its header is dragged") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Follow");
    REQUIRE(n.f.context().notes().link(id, "concept", n.concept_));
    QmlFixture::settle();
    auto* note = QmlFixture::child(n.window.get(), "stickyNote");
    QSignalSpy painted(QmlFixture::child(n.layer, "noteLinks"), SIGNAL(painted()));
    while (painted.wait(300)) painted.clear();
    note->setProperty("dragDX", 40.0);
    CHECK(painted.wait(300));
}
