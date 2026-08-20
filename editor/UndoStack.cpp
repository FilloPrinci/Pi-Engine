#include "UndoStack.h"

#include <utility>

void UndoStack::Push(Action undo, Action redo) {
    m_undoStack.push_back(Entry{std::move(undo), std::move(redo)});
    m_redoStack.clear();
}

void UndoStack::Undo() {
    if (m_undoStack.empty()) {
        return;
    }
    Entry entry = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    if (entry.undo) {
        entry.undo();
    }
    m_redoStack.push_back(std::move(entry));
}

void UndoStack::Redo() {
    if (m_redoStack.empty()) {
        return;
    }
    Entry entry = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    if (entry.redo) {
        entry.redo();
    }
    m_undoStack.push_back(std::move(entry));
}
