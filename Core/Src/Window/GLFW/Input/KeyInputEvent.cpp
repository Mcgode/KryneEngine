/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#include "KeyInputEvent.hpp"

#include "GLFW/glfw3.h"

namespace KryneEngine::GLFW
{
    InputKeys ToInputPhysicalKeys(s32 _glfwKey)
    {
#       define MAP(glfwName, keKey) case glfwName: return InputKeys::keKey

        switch (_glfwKey)
        {
            MAP(GLFW_KEY_SPACE, Space);
            MAP(GLFW_KEY_APOSTROPHE, Apostrophe);
            MAP(GLFW_KEY_COMMA, Comma);
            MAP(GLFW_KEY_MINUS, Minus);
            MAP(GLFW_KEY_PERIOD, Period);
            MAP(GLFW_KEY_SLASH, Slash);
            MAP(GLFW_KEY_SEMICOLON, SemiColon);
            MAP(GLFW_KEY_EQUAL, Equal);
            MAP(GLFW_KEY_0, Num0);
            MAP(GLFW_KEY_1, Num1);
            MAP(GLFW_KEY_2, Num2);
            MAP(GLFW_KEY_3, Num3);
            MAP(GLFW_KEY_4, Num4);
            MAP(GLFW_KEY_5, Num5);
            MAP(GLFW_KEY_6, Num6);
            MAP(GLFW_KEY_7, Num7);
            MAP(GLFW_KEY_8, Num8);
            MAP(GLFW_KEY_9, Num9);
            MAP(GLFW_KEY_A, A);
            MAP(GLFW_KEY_B, B);
            MAP(GLFW_KEY_C, C);
            MAP(GLFW_KEY_D, D);
            MAP(GLFW_KEY_E, E);
            MAP(GLFW_KEY_F, F);
            MAP(GLFW_KEY_G, G);
            MAP(GLFW_KEY_H, H);
            MAP(GLFW_KEY_I, I);
            MAP(GLFW_KEY_J, J);
            MAP(GLFW_KEY_K, K);
            MAP(GLFW_KEY_L, L);
            MAP(GLFW_KEY_M, M);
            MAP(GLFW_KEY_N, N);
            MAP(GLFW_KEY_O, O);
            MAP(GLFW_KEY_P, P);
            MAP(GLFW_KEY_Q, Q);
            MAP(GLFW_KEY_R, R);
            MAP(GLFW_KEY_S, S);
            MAP(GLFW_KEY_T, T);
            MAP(GLFW_KEY_U, U);
            MAP(GLFW_KEY_V, V);
            MAP(GLFW_KEY_W, W);
            MAP(GLFW_KEY_X, X);
            MAP(GLFW_KEY_Y, Y);
            MAP(GLFW_KEY_Z, Z);
            MAP(GLFW_KEY_LEFT_BRACKET, LeftBracket);
            MAP(GLFW_KEY_RIGHT_BRACKET, RightBracket);
            MAP(GLFW_KEY_BACKSLASH, BackSlash);
            MAP(GLFW_KEY_GRAVE_ACCENT, GraveAccent);
            MAP(GLFW_KEY_ESCAPE, Escape);
            MAP(GLFW_KEY_ENTER, Enter);
            MAP(GLFW_KEY_TAB, Tab);
            MAP(GLFW_KEY_BACKSPACE, Backspace);
            MAP(GLFW_KEY_INSERT, Insert);
            MAP(GLFW_KEY_DELETE, Delete);
            MAP(GLFW_KEY_UP, Up);
            MAP(GLFW_KEY_DOWN, Down);
            MAP(GLFW_KEY_LEFT, Left);
            MAP(GLFW_KEY_RIGHT, Right);
            MAP(GLFW_KEY_PAGE_UP, PageUp);
            MAP(GLFW_KEY_PAGE_DOWN, PageDown);
            MAP(GLFW_KEY_HOME, Home);
            MAP(GLFW_KEY_END, End);
            MAP(GLFW_KEY_CAPS_LOCK, CapsLock);
            MAP(GLFW_KEY_SCROLL_LOCK, ScrollLock);
            MAP(GLFW_KEY_NUM_LOCK, NumLock);
            MAP(GLFW_KEY_PRINT_SCREEN, PrintScreen);
            MAP(GLFW_KEY_PAUSE, Pause);
            MAP(GLFW_KEY_F1, F1);
            MAP(GLFW_KEY_F2, F2);
            MAP(GLFW_KEY_F3, F3);
            MAP(GLFW_KEY_F4, F4);
            MAP(GLFW_KEY_F5, F5);
            MAP(GLFW_KEY_F6, F6);
            MAP(GLFW_KEY_F7, F7);
            MAP(GLFW_KEY_F8, F8);
            MAP(GLFW_KEY_F9, F9);
            MAP(GLFW_KEY_F10, F10);
            MAP(GLFW_KEY_F11, F11);
            MAP(GLFW_KEY_F12, F12);
            MAP(GLFW_KEY_KP_0, Keypad0);
            MAP(GLFW_KEY_KP_1, Keypad1);
            MAP(GLFW_KEY_KP_2, Keypad2);
            MAP(GLFW_KEY_KP_3, Keypad3);
            MAP(GLFW_KEY_KP_4, Keypad4);
            MAP(GLFW_KEY_KP_5, Keypad5);
            MAP(GLFW_KEY_KP_6, Keypad6);
            MAP(GLFW_KEY_KP_7, Keypad7);
            MAP(GLFW_KEY_KP_8, Keypad8);
            MAP(GLFW_KEY_KP_DECIMAL, KeypadDecimal);
            MAP(GLFW_KEY_KP_DIVIDE, KeypadDivide);
            MAP(GLFW_KEY_KP_MULTIPLY, KeypadMultiply);
            MAP(GLFW_KEY_KP_SUBTRACT, KeypadSubtract);
            MAP(GLFW_KEY_KP_ADD, KeypadAdd);
            MAP(GLFW_KEY_KP_ENTER, KeypadEnter);
            MAP(GLFW_KEY_LEFT_SHIFT, LeftShift);
            MAP(GLFW_KEY_LEFT_CONTROL, LeftCtrl);
            MAP(GLFW_KEY_LEFT_ALT, LeftAlt);
            MAP(GLFW_KEY_LEFT_SUPER, LeftSuper);
            MAP(GLFW_KEY_RIGHT_SHIFT, RightShift);
            MAP(GLFW_KEY_RIGHT_CONTROL, RightCtrl);
            MAP(GLFW_KEY_RIGHT_ALT, RightAlt);
            MAP(GLFW_KEY_RIGHT_SUPER, RightSuper);
            MAP(GLFW_KEY_MENU, Menu);
        default:
            return InputKeys::Unknown;
        }

#undef  MAP
    }

    s32 FromInputPhysicalKeys(InputKeys _key)
    {
#       define MAP(glfwName, keKey) case InputKeys::keKey: return glfwName

        switch (_key)
        {
            MAP(GLFW_KEY_SPACE, Space);
            MAP(GLFW_KEY_APOSTROPHE, Apostrophe);
            MAP(GLFW_KEY_COMMA, Comma);
            MAP(GLFW_KEY_MINUS, Minus);
            MAP(GLFW_KEY_PERIOD, Period);
            MAP(GLFW_KEY_SLASH, Slash);
            MAP(GLFW_KEY_SEMICOLON, SemiColon);
            MAP(GLFW_KEY_EQUAL, Equal);
            MAP(GLFW_KEY_0, Num0);
            MAP(GLFW_KEY_1, Num1);
            MAP(GLFW_KEY_2, Num2);
            MAP(GLFW_KEY_3, Num3);
            MAP(GLFW_KEY_4, Num4);
            MAP(GLFW_KEY_5, Num5);
            MAP(GLFW_KEY_6, Num6);
            MAP(GLFW_KEY_7, Num7);
            MAP(GLFW_KEY_8, Num8);
            MAP(GLFW_KEY_9, Num9);
            MAP(GLFW_KEY_A, A);
            MAP(GLFW_KEY_B, B);
            MAP(GLFW_KEY_C, C);
            MAP(GLFW_KEY_D, D);
            MAP(GLFW_KEY_E, E);
            MAP(GLFW_KEY_F, F);
            MAP(GLFW_KEY_G, G);
            MAP(GLFW_KEY_H, H);
            MAP(GLFW_KEY_I, I);
            MAP(GLFW_KEY_J, J);
            MAP(GLFW_KEY_K, K);
            MAP(GLFW_KEY_L, L);
            MAP(GLFW_KEY_M, M);
            MAP(GLFW_KEY_N, N);
            MAP(GLFW_KEY_O, O);
            MAP(GLFW_KEY_P, P);
            MAP(GLFW_KEY_Q, Q);
            MAP(GLFW_KEY_R, R);
            MAP(GLFW_KEY_S, S);
            MAP(GLFW_KEY_T, T);
            MAP(GLFW_KEY_U, U);
            MAP(GLFW_KEY_V, V);
            MAP(GLFW_KEY_W, W);
            MAP(GLFW_KEY_X, X);
            MAP(GLFW_KEY_Y, Y);
            MAP(GLFW_KEY_Z, Z);
            MAP(GLFW_KEY_LEFT_BRACKET, LeftBracket);
            MAP(GLFW_KEY_RIGHT_BRACKET, RightBracket);
            MAP(GLFW_KEY_BACKSLASH, BackSlash);
            MAP(GLFW_KEY_GRAVE_ACCENT, GraveAccent);
            MAP(GLFW_KEY_ESCAPE, Escape);
            MAP(GLFW_KEY_ENTER, Enter);
            MAP(GLFW_KEY_TAB, Tab);
            MAP(GLFW_KEY_BACKSPACE, Backspace);
            MAP(GLFW_KEY_INSERT, Insert);
            MAP(GLFW_KEY_DELETE, Delete);
            MAP(GLFW_KEY_UP, Up);
            MAP(GLFW_KEY_DOWN, Down);
            MAP(GLFW_KEY_LEFT, Left);
            MAP(GLFW_KEY_RIGHT, Right);
            MAP(GLFW_KEY_PAGE_UP, PageUp);
            MAP(GLFW_KEY_PAGE_DOWN, PageDown);
            MAP(GLFW_KEY_HOME, Home);
            MAP(GLFW_KEY_END, End);
            MAP(GLFW_KEY_CAPS_LOCK, CapsLock);
            MAP(GLFW_KEY_SCROLL_LOCK, ScrollLock);
            MAP(GLFW_KEY_NUM_LOCK, NumLock);
            MAP(GLFW_KEY_PRINT_SCREEN, PrintScreen);
            MAP(GLFW_KEY_PAUSE, Pause);
            MAP(GLFW_KEY_F1, F1);
            MAP(GLFW_KEY_F2, F2);
            MAP(GLFW_KEY_F3, F3);
            MAP(GLFW_KEY_F4, F4);
            MAP(GLFW_KEY_F5, F5);
            MAP(GLFW_KEY_F6, F6);
            MAP(GLFW_KEY_F7, F7);
            MAP(GLFW_KEY_F8, F8);
            MAP(GLFW_KEY_F9, F9);
            MAP(GLFW_KEY_F10, F10);
            MAP(GLFW_KEY_F11, F11);
            MAP(GLFW_KEY_F12, F12);
            MAP(GLFW_KEY_KP_0, Keypad0);
            MAP(GLFW_KEY_KP_1, Keypad1);
            MAP(GLFW_KEY_KP_2, Keypad2);
            MAP(GLFW_KEY_KP_3, Keypad3);
            MAP(GLFW_KEY_KP_4, Keypad4);
            MAP(GLFW_KEY_KP_5, Keypad5);
            MAP(GLFW_KEY_KP_6, Keypad6);
            MAP(GLFW_KEY_KP_7, Keypad7);
            MAP(GLFW_KEY_KP_8, Keypad8);
            MAP(GLFW_KEY_KP_DECIMAL, KeypadDecimal);
            MAP(GLFW_KEY_KP_DIVIDE, KeypadDivide);
            MAP(GLFW_KEY_KP_MULTIPLY, KeypadMultiply);
            MAP(GLFW_KEY_KP_SUBTRACT, KeypadSubtract);
            MAP(GLFW_KEY_KP_ADD, KeypadAdd);
            MAP(GLFW_KEY_KP_ENTER, KeypadEnter);
            MAP(GLFW_KEY_LEFT_SHIFT, LeftShift);
            MAP(GLFW_KEY_LEFT_CONTROL, LeftCtrl);
            MAP(GLFW_KEY_LEFT_ALT, LeftAlt);
            MAP(GLFW_KEY_LEFT_SUPER, LeftSuper);
            MAP(GLFW_KEY_RIGHT_SHIFT, RightShift);
            MAP(GLFW_KEY_RIGHT_CONTROL, RightCtrl);
            MAP(GLFW_KEY_RIGHT_ALT, RightAlt);
            MAP(GLFW_KEY_RIGHT_SUPER, RightSuper);
            MAP(GLFW_KEY_MENU, Menu);
        default:
            return GLFW_KEY_UNKNOWN;
        }

#undef  MAP
    }

    InputActionType ToInputEventAction(s32 _glfwAction)
    {
        switch (_glfwAction)
        {
        case GLFW_PRESS:
            return InputActionType::StartPress;
        case GLFW_REPEAT:
            return InputActionType::KeepPressing;
        case GLFW_RELEASE:
            return InputActionType::StopPress;
        default:
            KE_ERROR("Unknown action %d", _glfwAction);
            return InputActionType::Unknown;
        }
    }

    KeyInputModifiers ToInputEventModifiers(s32 _glfwMods)
    {
        KeyInputModifiers modifiers = KeyInputModifiers::None;
        if (_glfwMods & GLFW_MOD_SHIFT)
        {
            modifiers = KeyInputModifiers::Shift;
        }
        if (_glfwMods & GLFW_MOD_CONTROL)
        {
            modifiers = KeyInputModifiers::Ctrl;
        }
        if (_glfwMods & GLFW_MOD_ALT)
        {
            modifiers = KeyInputModifiers::Alt;
        }
        if (_glfwMods & GLFW_MOD_SUPER)
        {
            modifiers = KeyInputModifiers::Super;
        }
        if (_glfwMods & GLFW_MOD_CAPS_LOCK)
        {
            modifiers = KeyInputModifiers::CapsLock;
        }
        if (_glfwMods & GLFW_MOD_NUM_LOCK)
        {
            modifiers = KeyInputModifiers::NumLock;
        }
        return modifiers;
    }

    MouseInputButton ToMouseInputButton(s32 _glfwMouse)
    {
        switch (_glfwMouse)
        {
        case GLFW_MOUSE_BUTTON_1:
            return MouseInputButton::Button1;
        case GLFW_MOUSE_BUTTON_2:
            return MouseInputButton::Button2;
        case GLFW_MOUSE_BUTTON_3:
            return MouseInputButton::Button3;
        case GLFW_MOUSE_BUTTON_4:
            return MouseInputButton::Button4;
        case GLFW_MOUSE_BUTTON_5:
            return MouseInputButton::Button5;
        case GLFW_MOUSE_BUTTON_6:
            return MouseInputButton::Button6;
        case GLFW_MOUSE_BUTTON_7:
            return MouseInputButton::Button7;
        case GLFW_MOUSE_BUTTON_8:
            return MouseInputButton::Button8;
        default:
            KE_ERROR("Unknown mouse button %d", _glfwMouse);
            return MouseInputButton::Unknown;
        }
    }
}
