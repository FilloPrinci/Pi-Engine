#include "engine/script/ScriptComponent.h"
#include "engine/script/ScriptRegistry.h"

#include <doctest/doctest.h>

namespace {

class DummyTestScript : public engine::script::ScriptComponent {
public:
    void OnStart() override { started = true; }

    bool started = false;
};

} // namespace

REGISTER_SCRIPT(DummyTestScript);

TEST_CASE("ScriptRegistry::Create instantiates a script registered via REGISTER_SCRIPT") {
    auto script = engine::script::ScriptRegistry::Create("DummyTestScript");
    REQUIRE(script != nullptr);

    script->OnStart();
    // Downcast just to confirm Create() really returned the concrete registered type, not
    // some other ScriptComponent.
    CHECK(dynamic_cast<DummyTestScript*>(script.get()) != nullptr);
}

TEST_CASE("ScriptRegistry::Create returns nullptr for an unregistered name") {
    auto script = engine::script::ScriptRegistry::Create("ThisScriptDoesNotExist");
    CHECK(script == nullptr);
}
