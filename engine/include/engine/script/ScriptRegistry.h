#pragma once

#include "engine/script/ScriptComponent.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace engine::script {

// Factory keyed by class name (docs/01 section 6.2): "the Editor knows which scripts
// exist in the compiled binary and can list/assign them in the Inspector (no complex C++
// reflection required)". Populated at static-init time by REGISTER_SCRIPT below, read by
// samples/m3_hello_script/main.cpp today and, later, by the Editor's Inspector.
class ScriptRegistry {
public:
    using Factory = std::function<std::unique_ptr<ScriptComponent>()>;

    static void Register(const std::string& scriptName, Factory factory);
    static std::unique_ptr<ScriptComponent> Create(const std::string& scriptName);

private:
    static std::unordered_map<std::string, Factory>& Table();
};

} // namespace engine::script

// Registers `ClassName` under its own name (e.g. `REGISTER_SCRIPT(MoveScript);` at
// namespace scope, right after the class), so it can be instantiated by name later without
// the caller needing to know the concrete C++ type. A static object at namespace scope
// whose constructor runs the registration once, at static-init time, before main() --
// same pattern EXPOSE() uses for per-field registration (script/Expose.h).
#define REGISTER_SCRIPT(ClassName)                                                                \
    namespace {                                                                                   \
    struct ClassName##Registrar {                                                                 \
        ClassName##Registrar() {                                                                  \
            engine::script::ScriptRegistry::Register(#ClassName,                                   \
                                                       [] { return std::make_unique<ClassName>(); }); \
        }                                                                                          \
    };                                                                                             \
    const ClassName##Registrar g_##ClassName##Registrar;                                          \
    } // namespace
