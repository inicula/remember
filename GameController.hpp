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

struct GameController {
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

    /* Structs for the state union */
    struct MainMenuParams {
        i8 pos;
    };
    struct GameParams {
        Position player;
        u8 tileIdx;
        u8 subState;
        u8 level;
        u8 captured;
        u8 score;
    };
    struct SettingsParams {
        i8 pos;
    };
    struct SettingSliderParams {
        const char* description;
        i32* value;
        i32 min, max;
        i32 step;
        void (*callback)(i32);
    };
    struct GameOverParams {
        u8 score;
        i8 rank;
        bool highScore;
    };
    struct NameSelectionParams {
        u8 score;
        i8 pos;
        i8 rank;
    };
    struct LeaderboardUpdateParams {
        i8 pos;
    };
    struct AboutUpdateParams {
        u8 subState;
        i8 pos;
        i16 shift;
        const Tiny::String* header;
        const Tiny::String* content;
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
            NameSelectionParams nameSelection;
            LeaderboardUpdateParams leaderboard;
            AboutUpdateParams about;
        } params;
    };

    /* Member functions */
    GameController();
    void init();
    void update(const Input&);

    /* Static constexpr variables */
    static constexpr u8 DIN_PIN = 12;
    static constexpr u8 CLOCK_PIN = 4;
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
    static constexpr i32 DEFAULT_CONTRAST = 90;
    static constexpr i32 DEFAULT_BRIGHTNESS = 255;
    static constexpr i32 DEFAULT_MATRIX_INTENSITY = 8;
    static constexpr u8 LEADERBOARD_SIZE = 5;
    static constexpr LeaderboardEntry LEADERBOARD_ENTRY_NONE = { "**********", 0 };
    static constexpr LeaderboardEntry DEFAULT_LEADERBOARD[] = {
        LEADERBOARD_ENTRY_NONE,
        LEADERBOARD_ENTRY_NONE,
        LEADERBOARD_ENTRY_NONE,
        LEADERBOARD_ENTRY_NONE,
        LEADERBOARD_ENTRY_NONE,
    };

public:
    /* Data members */
    struct {
        LiquidCrystal controller;
        i32 contrast;
        i32 brightness;
    } lcd;
    struct {
        LedControl controller;
        i32 intensity;
    } matrix;
    State state;
    LeaderboardEntry leaderboard[LEADERBOARD_SIZE];
};

extern GameController gameController;
