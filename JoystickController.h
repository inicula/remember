#pragma once
#include "utils.h"
#include <Arduino.h>

class JoystickController {
public:
    enum class Direction : u8 {
        None = 0,
        Up,
        Down,
        Left,
        Right,
        NumDirections,
    };
    enum class Press : u8 {
        None = 0,
        Short,
        Long,
    };
    enum class MoveState : u8 {
        Ok = 0,
        NeedsReset,
    };

    void init();
    Press getButtonValue(u32 currentTs);
    Direction getDirection();

    static constexpr u8 BUTTON_PIN = 2;
    static constexpr u8 X_AXIS_PIN = A0;
    static constexpr u8 Y_AXIS_PIN = A1;
    static constexpr auto NUM_DIRECTIONS = u8(Direction::NumDirections);

private:
    bool updateButton(u32 currentTs);

private:
    struct {
        bool previousValue;
        u32 previousTs;
        u32 pressDur;
    } button;
    MoveState moveState;
};
