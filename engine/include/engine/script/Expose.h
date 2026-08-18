#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace engine::script {

// Field types the (future) Editor's Inspector will be able to show/edit (docs/01 section
// 6.2). Asset-reference types (e.g. PrefabRef) arrive with the Asset Pipeline, later.
enum class ExposedFieldType { Float, Int, Bool, Vec3 };

template <typename T>
struct ExposedFieldTraits; // no generic definition: only the four types below may be EXPOSE()d.

template <>
struct ExposedFieldTraits<float> {
    static constexpr ExposedFieldType kType = ExposedFieldType::Float;
};
template <>
struct ExposedFieldTraits<int> {
    static constexpr ExposedFieldType kType = ExposedFieldType::Int;
};
template <>
struct ExposedFieldTraits<bool> {
    static constexpr ExposedFieldType kType = ExposedFieldType::Bool;
};
template <>
struct ExposedFieldTraits<glm::vec3> {
    static constexpr ExposedFieldType kType = ExposedFieldType::Vec3;
};

struct ExposedFieldInfo {
    std::string name;
    ExposedFieldType type;
};

// Per-script-class table (docs/01 section 6.2): "the Editor knows which scripts exist...
// and can list/assign them in the Inspector" needs a field list keyed by script *type*,
// available before any instance of that script exists in a scene -- a Meyer's-singleton
// map, populated once per field at static-init time by ExposedField<T>'s constructor
// below, never mutated afterward.
class ExposedFieldRegistry {
public:
    static void Register(std::type_index scriptType, ExposedFieldInfo info) {
        Table()[scriptType].push_back(std::move(info));
    }
    static const std::vector<ExposedFieldInfo>& FieldsOf(std::type_index scriptType) {
        static const std::vector<ExposedFieldInfo> kEmpty;
        auto it = Table().find(scriptType);
        return it != Table().end() ? it->second : kEmpty;
    }

private:
    static std::unordered_map<std::type_index, std::vector<ExposedFieldInfo>>& Table() {
        static std::unordered_map<std::type_index, std::vector<ExposedFieldInfo>> table;
        return table;
    }
};

// Transparent value proxy behind EXPOSE() below. docs/01 section 6.2 describes EXPOSE as
// leaving the field "a normal float" -- literally true for reads/writes at every call
// site (implicit conversions + assignment make it behave exactly like T), but the actual
// mechanism needs the field's own constructor to run so it can self-register {name, type}
// into ExposedFieldRegistry -- the same "looks like a T, isn't quite one" trade already
// made for ComponentHandle<T> (docs/01 section 6.2: "for the developer, the syntax stays
// identical to a normal pointer"). No offsetof/reflection-of-incomplete-type trickery, no
// .generated.h, no separate build step -- the IDE always sees exactly this declaration.
template <typename T>
class ExposedField {
public:
    ExposedField(const std::type_info& scriptType, const char* name, T defaultValue)
        : m_value(defaultValue) {
        ExposedFieldRegistry::Register(std::type_index(scriptType),
                                        ExposedFieldInfo{name, ExposedFieldTraits<T>::kType});
    }

    operator T&() { return m_value; }
    operator const T&() const { return m_value; }
    ExposedField& operator=(const T& value) {
        m_value = value;
        return *this;
    }

private:
    T m_value;
};

} // namespace engine::script

// Intrusive field exposure (docs/01 section 6.2): declares `fieldName` as a normal-looking
// member with `defaultValue`, self-registered for the (future) Inspector. `typeid(*this)`
// resolves to the *derived* script class (ScriptComponent is polymorphic), so every
// EXPOSE()d field lands in its own script's table, not ScriptComponent's.
//
// One deliberate deviation from docs/01 section 6.2's illustration
// (`EXPOSE(speed) float speed = 3.0f;`, one macro argument, type inferred from the
// declaration that follows): that form needs offsetof() against the *still-being-defined*
// enclosing class, which requires either UB (address-of-not-yet-constructed-member) or a
// second, out-of-line macro naming the class again -- both worse than asking for the
// default value once, up front, which additionally hands ExposedField<T> its type via
// simple template argument deduction. Usage: `EXPOSE(speed, 3.0f);`.
#define EXPOSE(fieldName, defaultValue)                                                          \
    engine::script::ExposedField<decltype(defaultValue)> fieldName {                              \
        typeid(*this), #fieldName, defaultValue                                                   \
    }
