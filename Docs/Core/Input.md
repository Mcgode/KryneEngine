# Input System

## Overview

`InputManager` is the engine's per-application input hub. It is owned and fed by `WindowManager`,
which translates raw GLFW callbacks into engine events and forwards them into a per-frame event
queue. Once a frame, `InputManager::Update()` drains that queue: each event is offered to a
priority-ordered stack of `InputConsumer`s, then folded into pollable per-frame state and the action
map (unless a consumer captured it).

This replaces the earlier register/unregister-callback-by-id model, which invoked listeners
synchronously from inside GLFW callbacks. The current model is closer to Godot's `Input` singleton or
Unreal's Enhanced Input: raw events, a capture-aware consumer stack, and a polling API for gameplay
code.

Exactly one `WindowManager` and one `InputManager` may exist at a time — GLFW's process-global state
forces this. Unlike `WindowManager`, `InputManager` exposes a non-lazy `InputManager::Get()` accessor
backed by the app-owned instance, because polling gameplay code benefits from ambient access the way
window creation/resize handling does not.

## Main loop

```cpp
windowManager.PollEvents();          // pumps the OS, fills the InputManager's raw event queue
windowManager.GetInput().Update();   // drains the queue -> per-frame state, dispatches to consumers

// ... simulate, render ...
```

Call `Update()` once per frame, immediately after `PollEvents()` and before any code that polls input
state or reads `ImGuiIO`.

## Raw events

`InputEvent` is a small tagged union (`InputEventType` + `Window*` + payload) covering:

- `Key`, `Text`, `MouseButton`, `MouseMove`, `Scroll` — genuine input; these fold into per-frame state
  only if no consumer captures them.
- `WindowFocus`, `WindowResize`, `WindowDpiChange`, `WindowCloseRequest` — window-lifecycle events;
  these always fold into state regardless of consumption (a window losing focus isn't something a UI
  widget "consumes").

`WindowManager` pushes these via `InputManager::On*Event` from its GLFW callbacks. The window-lifecycle
events are also still delivered through `WindowManager::SetWindowEventCallbacks` (used by the ImGui
multi-viewport backend for `WindowMove` / `CursorEnter` / `MonitorsChanged`, which have no
`InputEventType` equivalent) — the two paths are independent and both fire.

## Consumer stack

```cpp
class InputConsumer
{
public:
    virtual bool HandleEvent(const InputEvent&) = 0;   // true = consumed, stop propagation
};

inputManager.PushConsumer(myConsumer, priority);   // higher priority sees events first
inputManager.RemoveConsumer(myConsumer);
```

During `Update()`, each event is offered to consumers from highest to lowest priority; the first one
whose `HandleEvent` returns `true` stops propagation to lower-priority consumers *and* to the
polling-state fold (window-lifecycle events excepted, see above).

`Modules::ImGui::Input` is the reference consumer: it unconditionally forwards every event to
`ImGuiIO` (ImGui needs the raw stream to compute next frame's `WantCapture*`), then reports the event
as consumed whenever `io.WantCaptureMouse` / `io.WantCaptureKeyboard` is set. Register it at a high
priority (it uses `1000`) so gameplay consumers naturally stop seeing input while a widget has focus.

## Polling API

```cpp
bool   IsKeyPressed(InputKeys) const;
bool   WasKeyJustPressed(InputKeys) const;
bool   WasKeyJustReleased(InputKeys) const;

float2 GetCursorPosition(Window* = nullptr) const;   // nullptr = focused window
float2 GetCursorDelta(Window* = nullptr) const;      // accumulated this frame, reset each Update()
float2 GetScrollDelta(Window* = nullptr) const;
bool   IsMouseButtonPressed(MouseInputButton, Window* = nullptr) const;
```

Cursor delta and scroll delta accumulate every `MouseMove` / `Scroll` event seen during the frame (so
multiple raw events per frame still produce one correct total displacement), then reset at the top of
the next `Update()`. `Window* = nullptr` resolves to `GetFocusedWindow()`.

`Samples/RenderGraphDemo/Scene/OrbitCamera` is the reference polling consumer: it no longer registers
any callback, and instead polls `IsMouseButtonPressed(MouseInputButton::Right)` +  `GetCursorDelta()`
once per frame in `Process()`.

## Action map

Data-shaped, but registration is hardcoded for now (no config-file loading or rebinding UI yet):

```cpp
struct InputBinding
{
    enum class Source : u8 { Key, MouseButton, MouseAxis };
    Source m_source;
    s32    m_code;         // an InputKeys / MouseInputButton / MouseAxis value
    float  m_scale = 1.f;  // negate via -1
};

struct InputAction
{
    eastl::fixed_vector<InputBinding, 4> m_bindings;
    float m_deadzone = 0.15f;
};

ActionId id = inputManager.RegisterAction("move_forward", { .m_bindings = { ... } });
bool     pressed = inputManager.IsActionPressed(id);
float    value   = inputManager.GetActionValue(id);
```

Every binding is summed (scaled) each `Update()`; the result is zeroed if its magnitude is below
`m_deadzone`. Input contexts (stackable/prioritised action scopes) are an intentional gap — add one
only once a sample actually needs more than one input scope.

## Deferred to a later pass

- Gamepad bindings (`glfwGetGamepadState`) — `InputBinding::Source` and `InputEventType` intentionally
  leave room for `GamepadButton` / `GamepadAxis`.
- Config-file-driven action maps and a runtime rebinding UI.
- A deterministic `InputSnapshot` record/replay seam for a future fixed-tick physics simulation.
