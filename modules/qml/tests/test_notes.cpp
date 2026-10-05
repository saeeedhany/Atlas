#include <QColor>
#include <QFont>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPointF>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <functional>

#include "atlas/viewmodels/notes_model.hpp"
#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

bool waitFor(const std::function<bool()>& done, int timeout = 2000) {
    QElapsedTimer clock;
    clock.start();
    while (!done() && clock.elapsed() < timeout) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return done();
}

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

    QObject* onlyNote() { return QmlFixture::child(window.get(), "stickyNote"); }
    QObject* bodyOf(QObject* note) { return QmlFixture::child(note, "noteBody"); }
    QString bodyText(const QString& id) { return f.context().notes().note(id).value("body").toString(); }
    int segmentCount() {
        QVariant segments;
        REQUIRE(QMetaObject::invokeMethod(layer, "segments", Q_RETURN_ARG(QVariant, segments)));
        return static_cast<int>(segments.toList().size());
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

TEST_CASE("note text saves shortly after typing stops") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "");
    QmlFixture::settle();
    auto* body = n.bodyOf(n.onlyNote());
    body->setProperty("text", "Half");
    body->setProperty("text", "Half full");
    CHECK(n.bodyText(id).isEmpty());
    CHECK(waitFor([&] { return n.bodyText(id) == "Half full"; }));
}

TEST_CASE("pending note text flushes on demand") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "");
    QmlFixture::settle();
    n.bodyOf(n.onlyNote())->setProperty("text", "Right away");
    REQUIRE(QMetaObject::invokeMethod(n.layer, "flushNotes"));
    CHECK(n.bodyText(id) == "Right away");
}

TEST_CASE("during a session notes fade and draw no link lines") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Linked");
    REQUIRE(n.f.context().notes().link(id, "concept", n.concept_));
    QmlFixture::settle();
    CHECK(n.segmentCount() == 1);
    REQUIRE(QMetaObject::invokeMethod(n.window.get(), "startSession"));
    QmlFixture::settle();
    REQUIRE(n.window->property("mode").toString() == "session");
    CHECK(waitFor([&] { return n.layer->property("opacity").toDouble() <= 0.25 + 1e-6; }));
    CHECK(n.segmentCount() == 0);
}

TEST_CASE("the note link canvas stays idle while panning a board without note links") {
    NoteFixture n;
    n.f.context().notes().createNote(0, 0, "Alone");
    QmlFixture::settle();
    QSignalSpy painted(QmlFixture::child(n.layer, "noteLinks"), SIGNAL(painted()));
    while (painted.wait(300)) painted.clear();
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "zoomAt", Q_ARG(double, 1.5), Q_ARG(double, 300.0), Q_ARG(double, 300.0)));
    CHECK_FALSE(painted.wait(300));
}

TEST_CASE("resizing a note writes board units") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Grow");
    QmlFixture::settle();
    double zoom = n.canvas->property("zoom").toDouble();
    bool resized = false;
    REQUIRE(QMetaObject::invokeMethod(n.layer, "resizeNoteTo", Q_RETURN_ARG(bool, resized), Q_ARG(QString, id),
                                      Q_ARG(double, 300.0 * zoom), Q_ARG(double, 200.0 * zoom)));
    CHECK(resized);
    CHECK(n.f.context().notes().note(id).value("width").toDouble() == doctest::Approx(300.0));
    CHECK(n.f.context().notes().note(id).value("height").toDouble() == doctest::Approx(200.0));
}

TEST_CASE("a note link can be removed from the note") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Unlink");
    REQUIRE(n.f.context().notes().link(id, "concept", n.concept_));
    QmlFixture::settle();
    auto* note = n.onlyNote();
    CHECK(note->property("linkTitles").toStringList() == QStringList{"Paging"});
    REQUIRE(QMetaObject::invokeMethod(note, "removeLink", Q_ARG(int, 0)));
    QmlFixture::settle();
    CHECK(n.f.context().notes().note(id).value("links").toList().isEmpty());
}

TEST_CASE("a new note is board sized and its text follows the zoom within limits") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Size");
    QmlFixture::settle();
    CHECK(n.f.context().notes().note(id).value("width").toDouble() == doctest::Approx(150.0));
    CHECK(n.f.context().notes().note(id).value("height").toDouble() == doctest::Approx(100.0));
    auto* body = n.bodyOf(n.onlyNote());
    n.canvas->setProperty("zoom", 1.0);
    QmlFixture::settle();
    CHECK(body->property("font").value<QFont>().pixelSize() == 13);
    n.canvas->setProperty("zoom", 4.0);
    QmlFixture::settle();
    CHECK(body->property("font").value<QFont>().pixelSize() == 20);
    n.canvas->setProperty("zoom", 0.5);
    QmlFixture::settle();
    CHECK(body->property("font").value<QFont>().pixelSize() == 9);
}

TEST_CASE("a new note takes keyboard focus") {
    NoteFixture n;
    auto* quick = qobject_cast<QQuickWindow*>(n.window.get());
    REQUIRE(quick != nullptr);
    quick->requestActivate();
    REQUIRE(QTest::qWaitForWindowActive(quick));
    QPointF empty = n.toScreen(2000, 2000);
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "handleDoubleClick", Q_ARG(double, empty.x()), Q_ARG(double, empty.y())));
    QmlFixture::settle();
    CHECK(n.bodyOf(n.onlyNote())->property("activeFocus").toBool());

    auto* bar = QmlFixture::child(n.window.get(), "topicBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "addConcept", Q_ARG(QString, "note: Second")));
    QmlFixture::settle();
    auto notes = n.window->findChildren<QObject*>("stickyNote");
    REQUIRE(notes.size() == 2);
    int focused = 0;
    for (QObject* note : notes) {
        if (n.bodyOf(note)->property("activeFocus").toBool()) {
            ++focused;
            CHECK(note->property("body").toString() == "Second");
        }
    }
    CHECK(focused == 1);
}

TEST_CASE("deleting a note asks once") {
    NoteFixture n;
    QString id = n.f.context().notes().createNote(0, 0, "Careful");
    QmlFixture::settle();
    auto* note = n.onlyNote();
    REQUIRE(QMetaObject::invokeMethod(note, "requestDelete"));
    CHECK(note->property("deleteArmed").toBool());
    CHECK(n.f.context().notes().count() == 1);
    REQUIRE(QMetaObject::invokeMethod(note, "requestDelete"));
    QmlFixture::settle();
    CHECK(n.f.context().notes().count() == 0);
}

TEST_CASE("a note blocks the board under it while its text is hidden") {
    NoteFixture n;
    auto* quick = qobject_cast<QQuickWindow*>(n.window.get());
    REQUIRE(quick != nullptr);
    n.canvas->setProperty("zoom", 0.3);
    QmlFixture::settle();
    QPointF world;
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "mapToWorld", Q_RETURN_ARG(QPointF, world), Q_ARG(double, 300.0),
                                      Q_ARG(double, 500.0)));
    n.f.context().notes().createNote(world.x(), world.y(), "Small");
    QmlFixture::settle();
    auto* note = qobject_cast<QQuickItem*>(n.onlyNote());
    REQUIRE(note != nullptr);
    CHECK_FALSE(n.bodyOf(note)->property("visible").toBool());
    QPointF center = note->mapToScene(QPointF(note->width() / 2, note->height() * 0.75));
    QTest::mouseDClick(quick, Qt::LeftButton, {}, center.toPoint());
    QmlFixture::settle();
    CHECK(n.f.context().notes().count() == 1);
}
