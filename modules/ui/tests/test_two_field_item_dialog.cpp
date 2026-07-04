#include "atlas/ui/two_field_item_dialog.hpp"
#include "doctest.h"

using namespace atlas::ui;

TEST_CASE("TwoFieldItemDialog returns trimmed field values") {
    TwoFieldItemDialog dialog("Title", "Field1", "  hello  ", "Field2", "  world  ", true);
    CHECK(dialog.field1() == "hello");
    CHECK(dialog.field2() == "world");
}

TEST_CASE("TwoFieldItemDialog pre-fills from the initial values given") {
    TwoFieldItemDialog dialog("Edit Thing", "Title", "Existing Title", "URL",
                               "https://example.com", false);
    CHECK(dialog.field1() == "Existing Title");
    CHECK(dialog.field2() == "https://example.com");
}

TEST_CASE("TwoFieldItemDialog with an optional field2 accepts an empty field2") {
    TwoFieldItemDialog dialog("Add Reference", "Title", "Some Title", "URL", "", false);
    CHECK(dialog.field1() == "Some Title");
    CHECK(dialog.field2().isEmpty());
}
