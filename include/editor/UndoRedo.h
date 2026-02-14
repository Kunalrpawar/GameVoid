// ============================================================================
// GameVoid Engine — Undo / Redo System (Command Pattern)
// ============================================================================
// Provides a generic undo/redo stack for editor operations.
// Each undoable action is encapsulated as a Command object.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace gv {

// ============================================================================
// Command — abstract base for undoable actions
// ============================================================================
class Command {
public:
    virtual ~Command() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual std::string GetDescription() const = 0;
};

// ============================================================================
// UndoStack — manages command history
// ============================================================================
class UndoStack {
public:
    /// Execute a command and push it onto the undo stack.
    void Execute(std::unique_ptr<Command> cmd) {
        cmd->Execute();
        m_UndoStack.push_back(std::move(cmd));
        m_RedoStack.clear(); // new action invalidates redo history
    }

    /// Undo the last command. Returns true if successful.
    bool Undo() {
        if (m_UndoStack.empty()) return false;
        auto cmd = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        cmd->Undo();
        m_RedoStack.push_back(std::move(cmd));
        return true;
    }

    /// Redo the last undone command. Returns true if successful.
    bool Redo() {
        if (m_RedoStack.empty()) return false;
        auto cmd = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        cmd->Execute();
        m_UndoStack.push_back(std::move(cmd));
        return true;
    }

    bool CanUndo() const { return !m_UndoStack.empty(); }
    bool CanRedo() const { return !m_RedoStack.empty(); }

    std::string GetUndoDescription() const {
        return m_UndoStack.empty() ? "" : m_UndoStack.back()->GetDescription();
    }
    std::string GetRedoDescription() const {
        return m_RedoStack.empty() ? "" : m_RedoStack.back()->GetDescription();
    }

    void Clear() {
        m_UndoStack.clear();
        m_RedoStack.clear();
    }

    u32 GetUndoCount() const { return static_cast<u32>(m_UndoStack.size()); }
    u32 GetRedoCount() const { return static_cast<u32>(m_RedoStack.size()); }

private:
    std::vector<std::unique_ptr<Command>> m_UndoStack;
    std::vector<std::unique_ptr<Command>> m_RedoStack;
};

// ============================================================================
// Concrete Commands for common editor operations
// ============================================================================

/// Lambda-based command (convenient for simple one-off actions).
class LambdaCommand : public Command {
public:
    LambdaCommand(std::string desc,
                  std::function<void()> exec,
                  std::function<void()> undo)
        : m_Description(std::move(desc))
        , m_Execute(std::move(exec))
        , m_Undo(std::move(undo)) {}

    void Execute() override { m_Execute(); }
    void Undo() override    { m_Undo(); }
    std::string GetDescription() const override { return m_Description; }

private:
    std::string m_Description;
    std::function<void()> m_Execute;
    std::function<void()> m_Undo;
};

/// Transform change command (stores old and new transform values).
class TransformCommand : public Command {
public:
    TransformCommand(Vec3* posPtr, Vec3 oldPos, Vec3 newPos,
                     Vec3* scalePtr, Vec3 oldScale, Vec3 newScale,
                     const std::string& objectName)
        : m_PosPtr(posPtr), m_OldPos(oldPos), m_NewPos(newPos)
        , m_ScalePtr(scalePtr), m_OldScale(oldScale), m_NewScale(newScale)
        , m_ObjectName(objectName) {}

    void Execute() override {
        if (m_PosPtr)   *m_PosPtr   = m_NewPos;
        if (m_ScalePtr) *m_ScalePtr = m_NewScale;
    }
    void Undo() override {
        if (m_PosPtr)   *m_PosPtr   = m_OldPos;
        if (m_ScalePtr) *m_ScalePtr = m_OldScale;
    }
    std::string GetDescription() const override {
        return "Transform " + m_ObjectName;
    }

private:
    Vec3* m_PosPtr;
    Vec3 m_OldPos, m_NewPos;
    Vec3* m_ScalePtr;
    Vec3 m_OldScale, m_NewScale;
    std::string m_ObjectName;
};

} // namespace gv
