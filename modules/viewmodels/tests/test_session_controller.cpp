#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/session_controller.hpp"
#include "doctest.h"
#include "raw_sql.hpp"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase(const std::string& path) {
    auto result = Database::open(path);
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

struct Fixture {
    QTemporaryDir dir;
    std::string path = dir.filePath("atlas.db").toStdString();
    Database db = openTestDatabase(path);
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    QSettings store{dir.filePath("settings.ini"), QSettings::IniFormat};
    AppSettings settings{store};
    Palette palette{settings};
    TimePoint now = std::chrono::system_clock::now();
    MemoryController memory{db, workspace, [this] { return now; }};
    PlacementController placements{db, workspace};
    MapViewModel map{workspace, memory, placements, palette};
    SessionController session{workspace, memory, map, settings};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(placements.load().hasValue());
        REQUIRE(memory.load().hasValue());
        QObject::connect(&session, &SessionController::errorOccurred, [this] { ++errors; });
        settle();
    }

    static void settle() { QCoreApplication::processEvents(); }

    KnowledgeObjectId addConcept(const char* title) {
        auto id = workspace.createKnowledgeObject(title).value();
        settle();
        return id;
    }

    void introduce(const KnowledgeObjectId& id) {
        ReviewEvent event;
        event.id = Uuid::generate();
        event.item = ItemRef::forConcept(id);
        event.sessionId = Uuid::generate();
        event.deviceId = "test";
        event.reviewedAt = now - std::chrono::hours(1);
        event.grade = Grade::Good;
        REQUIRE(memory.record({event}).hasValue());
    }

    RelationshipId dueLink(const KnowledgeObjectId& source, const KnowledgeObjectId& target) {
        introduce(source);
        introduce(target);
        auto link = workspace.createRelationship(source, target, RelationshipType::DependsOn, std::nullopt).value();
        settle();
        return link;
    }

    void reviewLink(const RelationshipId& link) {
        ReviewEvent event;
        event.id = Uuid::generate();
        event.item = ItemRef::forLink(link);
        event.sessionId = Uuid::generate();
        event.deviceId = "test";
        event.reviewedAt = now - std::chrono::minutes(30);
        event.grade = Grade::Good;
        REQUIRE(memory.record({event}).hasValue());
    }

    int edgesMarked(atlas::render::EdgeMark mark) const {
        int count = 0;
        for (const auto& edge : map.edges()) count += edge.mark == mark ? 1 : 0;
        return count;
    }

    QVariantMap feedbackAt(int index) const { return session.feedback()[index].toMap(); }
};

}  // namespace

TEST_CASE("nothing to recall means no session") {
    Fixture f;
    CHECK_FALSE(f.session.start());
    CHECK(f.errors == 1);
    CHECK(f.session.stage() == "idle");
}

TEST_CASE("a new concept is introduced with the explain step and its written answer is saved") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    REQUIRE(f.session.start());
    CHECK(f.session.stage() == "explain");
    CHECK(f.session.focusIsNew());
    CHECK(f.session.prompt() == "What is Tree?");
    CHECK(f.session.answerKey().isEmpty());

    REQUIRE(f.session.submitExplain(3, 3, "  A branching structure "));
    CHECK(f.workspace.findKnowledgeObject(tree)->definition() == "A branching structure");
    CHECK(f.session.stage() == "summary");
    CHECK(f.session.reviewedCount() == 1);
    CHECK(f.session.recalledCount() == 1);
    CHECK(f.session.calibration() == QStringList{"Fairly sure 1 time, right 1"});
    CHECK(f.memory.recallChance(ItemRef::forConcept(tree)).has_value());
}

TEST_CASE("a rebuild hides the focus's links, grades what was named, then explains the link") {
    Fixture f;
    auto alpha = f.addConcept("Alpha");
    auto beta = f.addConcept("Beta");
    auto link = f.dueLink(alpha, beta);

    REQUIRE(f.session.start());
    CHECK(f.session.stage() == "rebuild");
    CHECK(f.session.focusId() == idString(alpha));
    CHECK(f.session.hiddenCount() == 1);
    CHECK(f.map.inSession());
    CHECK(f.map.edges()[0].mark == atlas::render::EdgeMark::Hidden);

    CHECK(f.session.candidates("Be").isEmpty());
    REQUIRE(f.session.candidates("bet").size() == 1);
    CHECK(f.session.candidates("alp").isEmpty());

    REQUIRE(f.session.addRecalled(idString(beta), 0, true));
    REQUIRE(f.session.submitRebuild(3));
    CHECK(f.session.stage() == "feedback");
    CHECK(f.feedbackAt(0).value("outcome").toString() == "recalled");
    CHECK(f.map.edges()[0].mark == atlas::render::EdgeMark::Recalled);

    f.session.continueToExplain();
    CHECK(f.session.stage() == "explain");
    CHECK(f.session.prompt() == "Why does Alpha depend on Beta?");
    auto before = f.memory.events().size();
    REQUIRE(f.session.submitExplain(3, 3, "Beta comes first"));
    CHECK(f.memory.events().size() == before + 2);
    CHECK(f.workspace.graph().findEdge(link)->note() == std::optional<std::string>("Beta comes first"));
    CHECK(f.session.stage() == "summary");
    CHECK(f.memory.events().back().elapsedDays == 0.0);
}

TEST_CASE("a confident miss returns once at the end") {
    Fixture f;
    auto alpha = f.addConcept("Alpha");
    auto beta = f.addConcept("Beta");
    f.dueLink(alpha, beta);

    REQUIRE(f.session.start());
    REQUIRE(f.session.submitRebuild(4));
    CHECK(f.feedbackAt(0).value("outcome").toString() == "missed");
    f.session.continueToExplain();
    REQUIRE(f.session.submitExplain(4, 1, QString()));
    CHECK(f.session.stage() == "rebuild");
    CHECK(f.session.focusNumber() == 2);
    CHECK(f.session.focusCount() == 2);

    REQUIRE(f.session.submitRebuild(4));
    f.session.continueToExplain();
    REQUIRE(f.session.submitExplain(4, 1, QString()));
    CHECK(f.session.stage() == "summary");
    CHECK(f.session.reviewedCount() == 4);
    CHECK(f.session.tipSource() == "Butterfield & Metcalfe 2001");
}

TEST_CASE("hints point at missing neighbors and wrong names show as confused") {
    Fixture f;
    auto alpha = f.addConcept("Alpha");
    auto beta = f.addConcept("Beta");
    auto gamma = f.addConcept("Gamma");
    f.introduce(gamma);
    f.dueLink(alpha, beta);

    REQUIRE(f.session.start());
    CHECK(f.session.hint() == idString(beta));
    CHECK(f.session.hintsUsed() == 1);
    CHECK(f.session.hint().isEmpty());

    REQUIRE(f.session.addRecalled(idString(gamma), 0, true));
    REQUIRE(f.session.submitRebuild(2));
    bool confusedShown = false;
    for (const auto& entry : f.session.feedback()) {
        auto map = entry.toMap();
        if (map.value("outcome").toString() == "confused") confusedShown = confusedShown || map.value("title").toString() == "Gamma";
    }
    CHECK(confusedShown);
    bool confusedEdge = false;
    for (const auto& edge : f.map.edges()) confusedEdge = confusedEdge || edge.mark == atlas::render::EdgeMark::Confused;
    CHECK(confusedEdge);
}

TEST_CASE("a failed write keeps the focus so it can be retried") {
    Fixture f;
    f.addConcept("Tree");
    REQUIRE(f.session.start());
    auto before = f.memory.events().size();
    REQUIRE(executeRawSql(f.path, "DROP TABLE review_events"));
    CHECK_FALSE(f.session.submitExplain(3, 3, QString()));
    CHECK(f.errors == 1);
    CHECK(f.session.stage() == "explain");
    CHECK(f.memory.events().size() == before);
}

TEST_CASE("quitting keeps completed focuses and finishing restores the map") {
    Fixture f;
    auto os = f.workspace.createTopic("OS").value();
    f.addConcept("Alpha");
    f.addConcept("Beta");
    f.map.setTopicId(idString(os));

    REQUIRE(f.session.start());
    CHECK(f.map.topicId().isEmpty());
    CHECK(f.session.focusCount() == 2);
    REQUIRE(f.session.submitExplain(3, 3, QString()));
    CHECK(f.session.stage() == "explain");
    f.session.quit();
    CHECK(f.session.stage() == "summary");
    CHECK(f.session.reviewedCount() == 1);
    CHECK(f.memory.events().size() == 1);

    f.session.finish();
    CHECK(f.session.stage() == "idle");
    CHECK_FALSE(f.map.inSession());
    CHECK(f.map.topicId() == idString(os));
}

TEST_CASE("every introduced link is hidden but only due ones are graded") {
    Fixture f;
    auto alpha = f.addConcept("Alpha");
    auto beta = f.addConcept("Beta");
    auto gamma = f.addConcept("Gamma");
    f.introduce(gamma);
    auto older = f.dueLink(alpha, gamma);
    f.reviewLink(older);
    f.dueLink(alpha, beta);

    REQUIRE(f.session.start());
    REQUIRE(f.session.focusId() == idString(alpha));
    CHECK(f.session.hiddenCount() == 2);
    CHECK(f.edgesMarked(atlas::render::EdgeMark::Hidden) == 2);

    REQUIRE(f.session.addRecalled(idString(beta), 0, true));
    REQUIRE(f.session.addRecalled(idString(gamma), 0, true));
    auto before = f.memory.events().size();
    REQUIRE(f.session.submitRebuild(3));
    CHECK(f.edgesMarked(atlas::render::EdgeMark::Hidden) == 0);
    CHECK(f.edgesMarked(atlas::render::EdgeMark::Confused) == 0);
    CHECK(f.session.feedback().size() == 1);
    f.session.continueToExplain();
    REQUIRE(f.session.submitExplain(3, 3, QString()));
    int linkEvents = 0;
    for (size_t i = before; i < f.memory.events().size(); ++i) {
        linkEvents += f.memory.events()[i].item.kind == atlas::core::ItemKind::Link ? 1 : 0;
    }
    CHECK(linkEvents == 2);
}

TEST_CASE("a concept deleted before its turn is skipped") {
    Fixture f;
    f.addConcept("Alpha");
    f.addConcept("Beta");
    REQUIRE(f.session.start());
    auto first = f.session.focusId();
    auto firstId = parseId<KnowledgeObjectId>(first).value();
    KnowledgeObjectId other = firstId;
    for (const auto& object : f.workspace.allKnowledgeObjects()) {
        if (object.id() != firstId) other = object.id();
    }
    REQUIRE(f.workspace.removeKnowledgeObject(other).hasValue());
    REQUIRE(f.session.submitExplain(3, 3, QString()));
    CHECK(f.session.stage() == "summary");
    CHECK(f.memory.events().size() == 1);
}

TEST_CASE("a link deleted mid rebuild records no event for it") {
    Fixture f;
    auto alpha = f.addConcept("Alpha");
    auto beta = f.addConcept("Beta");
    auto link = f.dueLink(alpha, beta);
    REQUIRE(f.session.start());
    REQUIRE(f.session.submitRebuild(3));
    f.session.continueToExplain();
    REQUIRE(f.workspace.removeRelationship(link).hasValue());
    auto before = f.memory.events().size();
    REQUIRE(f.session.submitExplain(3, 3, QString()));
    for (size_t i = before; i < f.memory.events().size(); ++i) {
        CHECK(f.memory.events()[i].item.kind != atlas::core::ItemKind::Link);
    }
}

TEST_CASE("a failed write in a rebuild focus keeps the focus") {
    Fixture f;
    auto alpha = f.addConcept("Alpha");
    auto beta = f.addConcept("Beta");
    f.dueLink(alpha, beta);
    REQUIRE(f.session.start());
    REQUIRE(f.session.submitRebuild(3));
    f.session.continueToExplain();
    auto before = f.memory.events().size();
    REQUIRE(executeRawSql(f.path, "DROP TABLE review_events"));
    CHECK_FALSE(f.session.submitExplain(3, 3, QString()));
    CHECK(f.session.stage() == "explain");
    CHECK(f.session.focusNumber() == 1);
    CHECK(f.memory.events().size() == before);
}
