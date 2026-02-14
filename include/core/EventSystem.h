// ============================================================================
// GameVoid Engine — Event System
// ============================================================================
// A lightweight publish-subscribe event bus for decoupled communication
// between engine systems. Supports collision events, input events, custom
// signals, and per-frame script dispatch.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace gv {

class GameObject;

// ============================================================================
// Event Types
// ============================================================================
enum class EventType {
    None = 0,
    // Collision events
    CollisionEnter,
    CollisionStay,
    CollisionExit,
    TriggerEnter,
    TriggerExit,
    // Input events
    KeyPressed,
    KeyReleased,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    // Lifecycle events
    ObjectCreated,
    ObjectDestroyed,
    SceneLoaded,
    SceneSaved,
    // Custom signals
    Custom
};

// ============================================================================
// Event — base data for all events
// ============================================================================
struct Event {
    EventType type = EventType::None;
    bool handled = false;

    // Collision data (used by CollisionEnter/Stay/Exit, TriggerEnter/Exit)
    GameObject* objectA = nullptr;
    GameObject* objectB = nullptr;
    Vec3 contactPoint{ 0, 0, 0 };
    Vec3 contactNormal{ 0, 0, 0 };
    f32  penetration = 0.0f;

    // Input data (used by Key/Mouse events)
    i32 keyCode = 0;
    i32 mouseButton = 0;
    Vec2 mousePosition{ 0, 0 };
    Vec2 mouseDelta{ 0, 0 };

    // Custom signal data
    std::string signalName;
    std::string signalData;

    // Source object (the object that emitted the event)
    GameObject* source = nullptr;
};

// ============================================================================
// Event Listener — callback type
// ============================================================================
using EventCallback = std::function<void(const Event&)>;

struct EventListener {
    u32 id = 0;                        // unique listener ID for unsubscription
    EventType type = EventType::None;  // event type to listen for
    EventCallback callback;
    GameObject* filter = nullptr;      // optional: only receive events involving this object
};

// ============================================================================
// EventBus — central event dispatcher
// ============================================================================
class EventBus {
public:
    EventBus() = default;

    /// Subscribe to an event type. Returns a listener ID for unsubscribing.
    u32 Subscribe(EventType type, EventCallback callback, GameObject* filter = nullptr) {
        u32 id = m_NextID++;
        m_Listeners.push_back({ id, type, std::move(callback), filter });
        return id;
    }

    /// Unsubscribe a listener by ID.
    void Unsubscribe(u32 listenerID) {
        m_Listeners.erase(
            std::remove_if(m_Listeners.begin(), m_Listeners.end(),
                [listenerID](const EventListener& l) { return l.id == listenerID; }),
            m_Listeners.end());
    }

    /// Dispatch an event to all matching listeners.
    void Dispatch(const Event& event) {
        for (auto& listener : m_Listeners) {
            if (listener.type != event.type) continue;
            // Apply object filter if set
            if (listener.filter) {
                if (event.objectA != listener.filter &&
                    event.objectB != listener.filter &&
                    event.source != listener.filter) continue;
            }
            listener.callback(event);
        }
    }

    /// Emit a custom signal by name.
    void EmitSignal(const std::string& signalName, const std::string& data = "",
                    GameObject* source = nullptr) {
        Event e;
        e.type = EventType::Custom;
        e.signalName = signalName;
        e.signalData = data;
        e.source = source;
        Dispatch(e);
    }

    /// Queue an event for deferred dispatch (useful during physics step).
    void QueueEvent(const Event& event) {
        m_EventQueue.push_back(event);
    }

    /// Dispatch all queued events and clear the queue.
    void FlushQueue() {
        for (auto& event : m_EventQueue) {
            Dispatch(event);
        }
        m_EventQueue.clear();
    }

    /// Clear all listeners.
    void Clear() {
        m_Listeners.clear();
        m_EventQueue.clear();
    }

    /// Get the singleton instance.
    static EventBus& Instance() {
        static EventBus instance;
        return instance;
    }

private:
    std::vector<EventListener> m_Listeners;
    std::vector<Event> m_EventQueue;
    u32 m_NextID = 1;
};

} // namespace gv
