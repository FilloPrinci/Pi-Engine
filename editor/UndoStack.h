#pragma once

#include <functional>
#include <vector>

// Editor-wide Undo/Redo (post-Editor-E8, docs/07-unity-parity-analysis.md). A stack of
// {undo, redo} closure pairs -- deliberately generic, not per-field-type, so every
// editable value in the Inspector (Transform/Collider/Rigidbody/Mesh fields, Parent
// reparenting) and the viewport gizmo drag can all push through this exact same Push()
// call. Each closure re-resolves its own ecs::Entity -> component pointer fresh when it
// actually runs, rather than capturing a raw component pointer across frames (CLAUDE.md
// rule 4: components can move in memory between frames) -- this class itself never needs
// to know anything about ecs::World/Entity/components at all, it just stores and runs
// closures. No namespace, matches ProjectHub.h/BuildPipeline.h's precedent for
// editor-local (not engine_core) code.
class UndoStack {
public:
    using Action = std::function<void()>;

    // Pushes one undo step and clears the redo stack -- a new action after an Undo()
    // invalidates whatever was "ahead" in the timeline, same as every real undo system
    // (Unity included): redoing back into a timeline a fresh edit already diverged from
    // doesn't mean anything coherent.
    void Push(Action undo, Action redo);

    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }

    // No-ops (not asserts) if the respective stack is empty -- callers are expected to
    // check CanUndo()/CanRedo() for UI purposes (e.g. graying out a button), but Ctrl+Z
    // with nothing to undo should just do nothing, not be a caller error.
    void Undo();
    void Redo();

private:
    struct Entry {
        Action undo;
        Action redo;
    };
    std::vector<Entry> m_undoStack;
    std::vector<Entry> m_redoStack;
};
