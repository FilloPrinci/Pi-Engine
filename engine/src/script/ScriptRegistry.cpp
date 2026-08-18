#include "engine/script/ScriptRegistry.h"

namespace engine::script {

void ScriptRegistry::Register(const std::string& scriptName, Factory factory) {
    Table()[scriptName] = std::move(factory);
}

std::unique_ptr<ScriptComponent> ScriptRegistry::Create(const std::string& scriptName) {
    auto it = Table().find(scriptName);
    if (it == Table().end()) {
        return nullptr;
    }
    return it->second();
}

std::unordered_map<std::string, ScriptRegistry::Factory>& ScriptRegistry::Table() {
    static std::unordered_map<std::string, Factory> table;
    return table;
}

} // namespace engine::script
