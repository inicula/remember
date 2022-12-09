#pragma once
#include "EEPROM.h"
#include "JoystickController.hpp"
#include "LedControl.h"
#include "LiquidCrystal.h"

struct Input {
    u32 currentTs;
    JoystickController::Press joyPress;
    JoystickController::Direction joyDir;
};

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using UpdateFunc = void (*)(const Input& input);

class GameController {
public:
    struct Position {
        bool operator==(const Position& rhs) const { return x == rhs.x && y == rhs.y; }
        bool operator!=(const Position& rhs) const { return !(*this == rhs); }
        Position clamp(const i8 low, const i8 high) const
        {
            return { Tiny::clamp(x, low, high), Tiny::clamp(y, low, high) };
        }

        i8 x, y;
    };

    struct LeaderboardEntry {
        static constexpr u8 NAME_SIZE = 10;

        char name[NAME_SIZE + 1];
        i8 score;
    };

    struct MainMenuParams {
        i8 pos;
    };
    struct GameParams {
        Position player;
        Position food;
        u8 score;
    };
    struct SettingsParams {
        i8 pos;
    };
    struct SettingSliderParams {
        const char* description;
        i32* value;
        i32 min, max;
        void (*callback)(const void*);
    };
    struct GameOverParams {
        u8 score;
    };
    struct State {
        UpdateFunc updateFunc;
        u32 beginTs;
        bool entry;
        union {
            MainMenuParams mainMenu;
            GameParams game;
            SettingsParams settings;
            SettingSliderParams slider;
            GameOverParams gameOver;
        } params;
    };

    GameController();

    void init();
    void update(const Input&);

    static constexpr u8 DIN_PIN = 12;
    static constexpr u8 CLOCK_PIN = 11;
    static constexpr u8 LOAD_PIN = 10;
    static constexpr u8 MATRIX_SIZE = 8;
    static constexpr u8 RS_PIN = 9;
    static constexpr u8 ENABLE_PIN = 8;
    static constexpr u8 D4 = A2;
    static constexpr u8 D5 = A3;
    static constexpr u8 D6 = A4;
    static constexpr u8 D7 = A5;
    static constexpr u8 NUM_ROWS = 2;
    static constexpr u8 NUM_COLS = 16;
    static constexpr u8 CONTRAST_PIN = 6;
    static constexpr u8 BRIGHTNESS_PIN = 5;
    static constexpr u8 DEFAULT_CONTRAST = 90;
    static constexpr u8 DEFAULT_BRIGHTNESS = 255;
    static constexpr u8 DEFAULT_MATRIX_BRIGHTNESS = 255;
    static constexpr u8 LEADERBOARD_SIZE = 5;

public:
    struct {
        LiquidCrystal controller;
        i32 contrast;
        i32 brightness;
    } lcd;
    LedControl lc;
    State state;
    LeaderboardEntry leaderboard[LEADERBOARD_SIZE];
};

extern GameController gameController;
