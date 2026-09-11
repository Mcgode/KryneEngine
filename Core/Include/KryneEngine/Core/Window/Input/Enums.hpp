/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#pragma once

#include "KryneEngine/Core/Common/BitUtils.hpp"

namespace KryneEngine
{
    enum class InputActionType: u8
    {
        Unknown,
        StartPress,
        KeepPressing,
        StopPress,
    };

    enum class KeyInputModifiers : u8
    {
        None        = 0,
        Ctrl        = 1 << 0,
        Shift       = 1 << 1,
        Alt         = 1 << 2,
        Super       = 1 << 3,
        CapsLock    = 1 << 4,
        NumLock     = 1 << 5,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(KeyInputModifiers)

    enum class MouseInputButton : u8
    {
        Unknown,

        Left,
        Right,
        Middle,

        Button1 = Left,
        Button2 = Right,
        Button3 = Middle,
        Button4,
        Button5,
        Button6,
        Button7,
        Button8,
        Button9,
        Button10,

        Count,
    };

    /// @brief Analog mouse axes usable as an @ref InputBinding source.
    enum class MouseAxis : u8
    {
        X,
        Y,
        ScrollX,
        ScrollY,
    };

    enum class InputKeys
    {
        Unknown,
        Space,
        Apostrophe,
        Comma,
        Minus,
        Period,
        Slash,
        SemiColon,
        Equal,
        Num0,
        Num1,
        Num2,
        Num3,
        Num4,
        Num5,
        Num6,
        Num7,
        Num8,
        Num9,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        LeftBracket,
        RightBracket,
        BackSlash,
        GraveAccent,
        Escape,
        Enter,
        Tab,
        Backspace,
        Insert,
        Delete,
        Up,
        Down,
        Right,
        Left,
        PageUp,
        PageDown,
        Home,
        End,
        CapsLock,
        ScrollLock,
        NumLock,
        PrintScreen,
        Pause,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        Keypad0,
        Keypad1,
        Keypad2,
        Keypad3,
        Keypad4,
        Keypad5,
        Keypad6,
        Keypad7,
        Keypad8,
        Keypad9,
        KeypadDecimal,
        KeypadDivide,
        KeypadMultiply,
        KeypadSubtract,
        KeypadAdd,
        KeypadEnter,
        KeypadEqual,
        LeftShift,
        LeftCtrl,
        LeftAlt,
        LeftSuper,
        RightShift,
        RightCtrl,
        RightAlt,
        RightSuper,
        Menu,

        Count,
    };
}
