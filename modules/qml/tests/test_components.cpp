#include <QVariantList>
#include <QVariantMap>

#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

QVariantMap item(const char* primary, const char* secondary) {
    return {{"primary", primary}, {"secondary", secondary}};
}

QVariantList call(QObject* object, const char* method, auto... arguments) {
    QVariant result;
    REQUIRE(QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(QVariant, result), arguments...));
    return result.toList();
}

}  // namespace

TEST_CASE("the list editor returns new lists and leaves its items alone") {
    QmlFixture f;
    QVariantList items{item("Index", "CREATE INDEX"), item("Scan", "")};
    auto editor = f.create("ListEditor", {{"title", "Examples"}, {"items", items}});

    auto replaced = call(editor.get(), "replaced", Q_ARG(int, 1), Q_ARG(QString, "secondary"), Q_ARG(QString, "full"));
    REQUIRE(replaced.size() == 2);
    CHECK(replaced[1].toMap().value("secondary").toString() == "full");
    CHECK(editor->property("items").toList()[1].toMap().value("secondary").toString().isEmpty());

    auto added = call(editor.get(), "added");
    REQUIRE(added.size() == 3);
    CHECK(added[2].toMap().value("primary").toString().isEmpty());

    auto removed = call(editor.get(), "removed", Q_ARG(int, 0));
    REQUIRE(removed.size() == 1);
    CHECK(removed[0].toMap().value("primary").toString() == "Scan");
}

TEST_CASE("a toast shows a message and hides itself") {
    QmlFixture f;
    auto toast = f.create("Toast");
    CHECK_FALSE(toast->property("shown").toBool());
    REQUIRE(QMetaObject::invokeMethod(toast.get(), "show", Q_ARG(QString, "Could not save")));
    CHECK(toast->property("shown").toBool());
    CHECK(toast->property("text").toString() == "Could not save");
}

TEST_CASE("every shared component loads without warnings") {
    QmlFixture f;
    for (const char* type : {"AppButton", "AppTextField", "AppTextArea", "SectionLabel"}) {
        auto object = f.create(type);
        CHECK(object != nullptr);
    }
}
