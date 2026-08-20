#include "engine/platform/InputSystem.h"

namespace engine::platform {

void InputSystem::Update(const InputState& current) {
    m_previous = m_current;
    m_current = current;
}

bool InputSystem::IsKeyHeld(Key key) const {
    return m_current.keysHeld[static_cast<std::size_t>(key)];
}

bool InputSystem::WasPressedThisFrame(Key key) const {
    const std::size_t index = static_cast<std::size_t>(key);
    return m_current.keysHeld[index] && !m_previous.keysHeld[index];
}

bool InputSystem::WasReleasedThisFrame(Key key) const {
    const std::size_t index = static_cast<std::size_t>(key);
    return !m_current.keysHeld[index] && m_previous.keysHeld[index];
}

float InputSystem::GetMouseX() const {
    return m_current.mouseX;
}

float InputSystem::GetMouseY() const {
    return m_current.mouseY;
}

bool InputSystem::IsMouseLeftHeld() const {
    return m_current.mouseLeftHeld;
}

bool InputSystem::WasMouseLeftPressedThisFrame() const {
    return m_current.mouseLeftHeld && !m_previous.mouseLeftHeld;
}

bool InputSystem::WasMouseLeftReleasedThisFrame() const {
    return !m_current.mouseLeftHeld && m_previous.mouseLeftHeld;
}

} // namespace engine::platform
