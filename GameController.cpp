#include "GameController.hpp"
#include "MelodyPlayer.hpp"

/* Typedefs */
using State = GameController::State;

/* Structs */
struct StorageEntry {
    void* addr;
    const void* defaultAddr;
    u16 size;
};
struct SpecialChar {
    u8 data[8];
    char id;
};

/* Static constexpr class variables */
constexpr i32 GameController::DEFAULT_CONTRAST;
constexpr i32 GameController::DEFAULT_BRIGHTNESS;
constexpr i32 GameController::DEFAULT_MATRIX_INTENSITY;
constexpr GameController::LeaderboardEntry GameController::DEFAULT_LEADERBOARD[];

/* Template function declarations */
template <typename... Ts> static void printfLCD(u8, const char*, Ts&&...);
template <bool INITIALIZE = false> static void setDefaultState(const Input&);

/* Function declarations */
static void readEEPROM(size_t, void*, size_t);
static void writeEEPROM(size_t, const void*, size_t);
static void refreshContrast(i32);
static void refreshBrightness(i32);
static void refreshIntensity(i32 value);
static void greetUpdate(const Input&);
static void gameOverUpdate(const Input&);
static void mainMenuUpdate(const Input&);
static void gameUpdate(const Input&);
static void settingsUpdate(const Input&);
static void aboutUpdate(const Input&);
static void sliderUpdate(const Input&);
static void nameSelectionUpdate(const Input&);
static void leaderboardUpdate(const Input&);
static void saveToStorage();
static void highlightMovement(JoystickController::Direction);
static void highlightPress(JoystickController::Press);

/* Extern variables */
GameController gameController;

/* Constexpr variables */
static constexpr u16 GREET_MELODY_DURATION = 10000;
static constexpr u8 INPUT_SOUND_DUR = 50;
static constexpr const char* STR_FMT = "%-16s";
static constexpr const char* INT_FMT = "%-16d";
static constexpr u8 PRINTF_BUFSIZE = 17;
static constexpr StorageEntry STORAGE_DATA[] = {
    {
        &gameController.lcd.contrast,
        &GameController::DEFAULT_CONTRAST,
        sizeof(gameController.lcd.contrast),
    },
    {
        &gameController.lcd.brightness,
        &GameController::DEFAULT_BRIGHTNESS,
        sizeof(gameController.lcd.brightness),
    },
    {
        &gameController.matrix.intensity,
        &GameController::DEFAULT_MATRIX_INTENSITY,
        sizeof(gameController.matrix.intensity),
    },
    {
        &gameController.leaderboard,
        &GameController::DEFAULT_LEADERBOARD,
        sizeof(gameController.leaderboard),
    },
};
static constexpr State DEFAULT_MENU_STATE = {
    &mainMenuUpdate,
    0,
    true,
    { .mainMenu = { 0 } },
};

/* Special characters */
#define UP_DOWN_ARROW_STR "\1"
#define DOWN_ARROW_STR "\2"
#define UP_DOWN_ARROW '\1'
#define DOWN_ARROW '\2'
static SpecialChar SPECIAL_CHARS[] = {
    {
        {
            0b00100,
            0b01010,
            0b10001,
            0b00000,
            0b00000,
            0b10001,
            0b01010,
            0b00100,
        },
        UP_DOWN_ARROW,
    },
    {
        {
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b10001,
            0b01010,
            0b00100,
        },
        DOWN_ARROW,
    },
};

/* Static variables */
static char printfBuffer[PRINTF_BUFSIZE] = {};
static GameController::LeaderboardEntry currentPlayer = { "         ", 0 };
static Tiny::Array<u8, GameController::MATRIX_SIZE> matrixRowIndices = {};
static Tiny::Array<u8, GameController::MATRIX_SIZE> matrixColIndices = {};
static MelodyPlayer mp(CONTRAPUNCTUS_1, GREET_MELODY_DURATION);

template <typename... Ts> static void printfLCD(u8 row, const char* fmt, Ts&&... args)
{
    snprintf(&printfBuffer[0], PRINTF_BUFSIZE, fmt, args...);

    gameController.lcd.controller.setCursor(0, row);
    gameController.lcd.controller.print(&printfBuffer[0]);
}

template <bool INITIALIZE> void setDefaultState(const Input&)
{
    size_t eepromAddr = 0;
    for (auto& data : STORAGE_DATA) {
        if constexpr (INITIALIZE) {
            writeEEPROM(eepromAddr, data.defaultAddr, data.size);
            eepromAddr += data.size;
        }

        memcpy(data.addr, data.defaultAddr, data.size);
    }

    refreshContrast(gameController.lcd.contrast);
    refreshBrightness(gameController.lcd.brightness);
    refreshIntensity(gameController.matrix.intensity);

    gameController.state = { &settingsUpdate, 0, true, {} };
}

static void readEEPROM(size_t eepromBaseAddr, void* addr, size_t count)
{
    u8 buffer[count];
    for (size_t i = 0; i < count; ++i)
        buffer[i] = EEPROM.read(i16(eepromBaseAddr + i));

    memcpy(addr, &buffer[0], count);
}

static void writeEEPROM(size_t eepromBaseAddr, const void* addr, size_t count)
{
    u8 buffer[count];
    memcpy(&buffer[0], addr, count);

    for (size_t i = 0; i < count; ++i)
        EEPROM.update(i16(eepromBaseAddr + i), buffer[i]);
}

void refreshContrast(i32 value) { analogWrite(GameController::CONTRAST_PIN, i16(value)); }

void refreshBrightness(i32 value) { analogWrite(GameController::BRIGHTNESS_PIN, i16(value)); }

void refreshIntensity(i32 value)
{
    gameController.matrix.controller.setIntensity(0, i16(value));
}

void greetUpdate(const Input& input)
{
    auto& state = gameController.state;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "REMEMBER");
        printfLCD(1, STR_FMT, "A Memory Game");
    }

    mp.play(input.currentTs);

    if (u8(input.joyPress)) {
        mp.stop();
        highlightPress(input.joyPress);
        state = DEFAULT_MENU_STATE;
    }
}

void gameOverUpdate(const Input& input)
{
    static constexpr u32 DURATION = 5000;

    auto& state = gameController.state;
    auto& params = gameController.state.params.gameOver;

    if (state.entry) {
        state.entry = false;

        currentPlayer.score = i8(params.score);
        params.rank = 0;
        while (params.rank < GameController::LEADERBOARD_SIZE
            && params.score <= gameController.leaderboard[params.rank].score)
            ++params.rank;

        if (params.rank < GameController::LEADERBOARD_SIZE)
            params.highScore = true;

        printfLCD(0, STR_FMT, "GAME OVER!");
        printfLCD(1, "%s %-2d %s %2d", "Score", params.score, "Rank", params.rank + 1);
    }

    if (u8(input.joyPress) || input.currentTs - state.beginTs > DURATION) {
        highlightPress(input.joyPress);

        if (params.highScore) {
            const auto score = params.score;
            const auto rank = params.rank;
            state = {
                &nameSelectionUpdate,
                input.currentTs,
                true,
                {},
            };
            state.params.nameSelection.score = score;
            state.params.nameSelection.rank = rank;
        } else {
            state = DEFAULT_MENU_STATE;
        }
    }
}

void mainMenuUpdate(const Input& input)
{
    enum MenuPosition : u8 {
        StartGame = 0,
        Leaderboard,
        Settings,
        About,
        NumPositions,
    };

    /* clang-format off */
    static constexpr const char* MENU_DESCRIPTORS[NumPositions] = {
        [StartGame] = DOWN_ARROW_STR " Start Game",
        [Leaderboard]  = UP_DOWN_ARROW_STR " Leaderboard",
        [Settings]  = UP_DOWN_ARROW_STR " Settings",
        [About]     = "^ About",
    };
    static constexpr State MENU_TRANSITION_STATES[NumPositions] = {
        [StartGame] = {
            &gameUpdate,
            0,
            true,
            {
                .game = {
                    {0, 0},
                    0,
                    0,
                    1,
                    0,
                    0,
                    0,
                }
            }
        },
        [Leaderboard] = {
            &leaderboardUpdate,
            0,
            true,
            {
                .leaderboard = {
                    0,
                }
            }
        },
        [Settings] = {
            &settingsUpdate,
            0,
            true,
            {}
        },
        [About] = {
            &aboutUpdate,
            0,
            true,
            {
                .about = {
                    0,
                    0,
                    0,
                    nullptr,
                    nullptr,
                }
            }
        },
    };
    /* clang-format on */

    auto& state = gameController.state;
    auto& params = gameController.state.params.mainMenu;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "> MAIN MENU");
        printfLCD(1, STR_FMT, MENU_DESCRIPTORS[params.pos]);
    }

    highlightMovement(input.joyDir);

    const i8 delta = input.joyDir == JoystickController::Direction::Up
        ? -1
        : (input.joyDir == JoystickController::Direction::Down ? 1 : 0);
    const auto newPos = i8(Tiny::clamp(params.pos + delta, 0, NumPositions - 1));

    if (newPos != params.pos) {
        params.pos = newPos;

        printfLCD(1, STR_FMT, MENU_DESCRIPTORS[params.pos]);
    }

    if (input.joyDir == JoystickController::Direction::Right) {
        state = MENU_TRANSITION_STATES[params.pos];
        state.beginTs = input.currentTs;
    }
}

void gameUpdate(const Input& input)
{
    static constexpr u32 DEFAULT_TIME = 500;
    static constexpr i16 NUM_REVIEWS_LIMIT = 4;

    enum class State : u8 {
        GenerateLevel = 0,
        ShowLevel,
        Playing,
    };
    using Position = GameController::Position;

    auto& lc = gameController.matrix.controller;
    auto& state = gameController.state;
    auto& params = gameController.state.params.game;

    const auto maxReviews
        = Tiny::clamp(NUM_REVIEWS_LIMIT - params.level / 4, 1, NUM_REVIEWS_LIMIT);
    if (state.entry) {
        state.entry = false;

        switch (params.subState) {
        case u8(State::GenerateLevel):
            printfLCD(0, "%-8s%8s", "Score", "Reviews");
            printfLCD(1, "%-8d%8d", params.score, maxReviews - params.usedReviews);

            randomSeed(micros());
            Tiny::shuffle(matrixRowIndices);
            Tiny::shuffle(matrixColIndices);

            lc.clearDisplay(0);
            params.subState = u8(State::ShowLevel);
            params.player = { i8(matrixColIndices[0]), i8(matrixRowIndices[0]) };

            break;
        case u8(State::ShowLevel):
            lc.clearDisplay(0);
            printfLCD(1, "%-8d%8d", params.score, maxReviews - params.usedReviews);
            break;
        case u8(State::Playing):
            lc.setLed(0, params.player.y, params.player.x, true);
            break;
        default:
            UNREACHABLE;
        }
    }

    switch (params.subState) {
    case u8(State::ShowLevel): {
        const auto onTime = DEFAULT_TIME / 2;
        const u32 intervalNum = (input.currentTs - state.beginTs) / onTime;
        const auto oddInterval = intervalNum % 2;
        if (oddInterval && ((intervalNum + 1) / 2) == (params.tileIdx + 1u)) {
            if (params.tileIdx < params.level)
                lc.setLed(0, matrixRowIndices[params.tileIdx],
                    matrixColIndices[params.tileIdx], true);

            ++params.tileIdx;
        }

        if (params.tileIdx == params.level + 1) {
            state.entry = true;
            params.subState = u8(State::Playing);
        }

        break;
    }
    case u8(State::Playing): {
        if (!params.captured && params.usedReviews < maxReviews
            && input.joyPress == JoystickController::Press::Long) {
            highlightPress(input.joyPress);

            state.entry = true;
            state.beginTs = input.currentTs;
            params.player = { i8(matrixColIndices[0]), i8(matrixRowIndices[0]) };
            params.tileIdx = 0;
            params.subState = u8(State::ShowLevel);
            ++params.usedReviews;
            break;
        }

        highlightMovement(input.joyDir);

        const auto oldPos = params.player;
        switch (input.joyDir) {
        case JoystickController::Direction::Up:
            ++params.player.y;
            break;
        case JoystickController::Direction::Down:
            --params.player.y;
            break;
        case JoystickController::Direction::Left:
            ++params.player.x;
            break;
        case JoystickController::Direction::Right:
            --params.player.x;
            break;
        default:
            break;
        }

        params.player = params.player.clamp(0, GameController::MATRIX_SIZE - 1);

        if (oldPos != params.player) {
            const auto yTileIdx = Tiny::find(matrixRowIndices, oldPos.y);
            const auto xTileIdx = Tiny::find(matrixColIndices, oldPos.x);

            if (xTileIdx != yTileIdx || xTileIdx >= params.level || xTileIdx < params.captured)
                lc.setLed(0, oldPos.y, oldPos.x, false);

            lc.setLed(0, params.player.y, params.player.x, true);
        }

        if (input.joyPress == JoystickController::Press::Short) {
            highlightPress(input.joyPress);

            if (params.player
                == Position {
                    i8(matrixColIndices[params.captured]),
                    i8(matrixRowIndices[params.captured]),
                }) {
                ++params.captured;
            } else {
                const auto score = params.score;
                lc.clearDisplay(0);
                state = { &gameOverUpdate, input.currentTs, true, {} };
                state.params.gameOver.score = score;
                break;
            }

            if (params.captured == params.level) {
                state.entry = true;
                state.beginTs = input.currentTs;
                params = {
                    {},
                    0,
                    u8(State::GenerateLevel),
                    u8(params.level + 1),
                    0,
                    u8(params.score + 1),
                    0,
                };
            }
        }

        break;
    }
    default:
        break;
    }
}

void settingsUpdate(const Input& input)
{
    enum SettingsPosition : u8 {
        Contrast = 0,
        Brightness,
        Intensity,
        DefaultState,
        NumPositions,
    };

    /* clang-format off */
    static constexpr const char* SETTINGS_DESCRIPTORS[NumPositions] = {
        [Contrast]   = DOWN_ARROW_STR " Contrast",
        [Brightness] = UP_DOWN_ARROW_STR " Brightness",
        [Intensity] = UP_DOWN_ARROW_STR " Intensity",
        [DefaultState] = "^ Default state",
    };
    static constexpr State SETTING_TRANSITION_STATES[NumPositions] = {
        [Contrast] = {
            &sliderUpdate,
            0,
            true,
            {
                .slider = {
                    "< CONTRAST",
                    &gameController.lcd.contrast,
                    0,
                    255,
                    10,
                    &refreshContrast
                }
            }
        },
        [Brightness] = {
            &sliderUpdate,
            0,
            true,
            {
                .slider = {
                    "< BRIGHTNESS",
                    &gameController.lcd.brightness,
                    0,
                    255,
                    10,
                    &refreshBrightness
                }
            }
        },
        [Intensity] = {
            &sliderUpdate,
            0,
            true,
            {
                .slider = {
                    "< INTENSITY",
                    &gameController.matrix.intensity,
                    0,
                    15,
                    1,
                    &refreshIntensity
                }
            }
        },
        [DefaultState] = {
            &setDefaultState,
            {},
            {},
            {}
        }
    };
    /* clang-format on */

    auto& state = gameController.state;
    auto& params = gameController.state.params.settings;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "<> SETTINGS");
        printfLCD(1, STR_FMT, SETTINGS_DESCRIPTORS[params.pos]);

        saveToStorage();
    }

    highlightMovement(input.joyDir);

    const i8 delta = input.joyDir == JoystickController::Direction::Up
        ? -1
        : (input.joyDir == JoystickController::Direction::Down ? 1 : 0);
    const auto newPos = i8(Tiny::clamp(params.pos + delta, 0, NumPositions - 1));

    if (newPos != params.pos) {
        params.pos = newPos;

        printfLCD(1, STR_FMT, SETTINGS_DESCRIPTORS[params.pos]);
    }

    if (input.joyDir == JoystickController::Direction::Right)
        state = SETTING_TRANSITION_STATES[params.pos];
    if (input.joyDir == JoystickController::Direction::Left)
        state = DEFAULT_MENU_STATE;
}

void aboutUpdate(const Input& input)
{
    enum State : u8 {
        Disengaged = 0,
        Engaged,
    };

    enum AboutPosition : i8 {
        GameName,
        Author,
        GitLink,
        NumPositions,
    };

    static constexpr const char* DESCRIPTORS[NumPositions] = {
        [GameName] = DOWN_ARROW_STR " Game Name",
        [Author] = UP_DOWN_ARROW_STR " Author",
        [GitLink] = "^ Github Link",
    };
    static constexpr Tiny::Pair<Tiny::String, Tiny::String> CONTENT[NumPositions] = {
        [GameName] = { "Game Name", "Remember" },
        [Author] = { "Author", "Nicula Ionut 334" },
        [GitLink] = { "Git Link", "github.com/niculaionut/remember" },
    };

    auto& state = gameController.state;
    auto& params = gameController.state.params.about;

    if (state.entry) {
        state.entry = false;

        switch (params.subState) {
        case Disengaged:
            printfLCD(0, "<> %-13s", "ABOUT");
            printfLCD(1, STR_FMT, DESCRIPTORS[params.pos]);
            break;
        case Engaged:
            printfLCD(0, "< %-14s", params.header->ptr);
            printfLCD(1, STR_FMT, params.content->ptr);
            break;
        }
    }

    highlightMovement(input.joyDir);

    switch (params.subState) {
    case Disengaged: {
        const auto oldPos = params.pos;

        const i32 delta = input.joyDir == JoystickController::Direction::Up
            ? -1
            : (input.joyDir == JoystickController::Direction::Down ? 1 : 0);
        params.pos = Tiny::clamp(i8(params.pos + delta), i8(0), i8(NumPositions - 1));

        if (params.pos != oldPos)
            printfLCD(1, STR_FMT, DESCRIPTORS[params.pos]);

        if (input.joyDir == JoystickController::Direction::Left)
            state = DEFAULT_MENU_STATE;

        if (input.joyDir == JoystickController::Direction::Right) {
            state.entry = true;
            params.subState = Engaged;
            params.header = &CONTENT[params.pos].first;
            params.content = &CONTENT[params.pos].second;
        }

        break;
    }
    case Engaged: {
        const auto oldShift = params.shift;
        const i16 delta = input.joyDir == JoystickController::Direction::Up
            ? -5
            : (input.joyDir == JoystickController::Direction::Down ? 5 : 0);
        params.shift = Tiny::clamp(params.shift + delta, i16(0), i16(params.content->len - 1));

        if (params.shift != oldShift)
            printfLCD(1, STR_FMT, params.content->ptr + params.shift);

        if (input.joyDir == JoystickController::Direction::Left) {
            state.entry = true;
            params.subState = Disengaged;
        }
        break;
    }
    }
}

void sliderUpdate(const Input& input)
{
    auto& state = gameController.state;
    auto& params = gameController.state.params.slider;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, params.description);
        printfLCD(1, "%-10c%6d", UP_DOWN_ARROW, *params.value);
    }

    highlightMovement(input.joyDir);

    const i32 delta = input.joyDir == JoystickController::Direction::Up
        ? 1
        : (input.joyDir == JoystickController::Direction::Down ? -1 : 0);
    const i32 newValue
        = Tiny::clamp(*params.value + params.step * delta, params.min, params.max);

    if (*params.value != newValue) {
        *params.value = newValue;
        printfLCD(1, "%-10c%6d", UP_DOWN_ARROW, newValue);
        params.callback(newValue);
    }

    if (input.joyDir == JoystickController::Direction::Left)
        state = { &settingsUpdate, 0, true, {} };
}

void nameSelectionUpdate(const Input& input)
{
    static constexpr Tiny::String NAME_ALPHABET = " ABCDEFGHIJKLMNOPRSTUVWXYZ0123456789";

    auto& state = gameController.state;
    auto& params = gameController.state.params.nameSelection;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "Your name:");
        printfLCD(1, STR_FMT, currentPlayer.name);

        gameController.lcd.controller.setCursor(0, 1);
        gameController.lcd.controller.blink();
    }

    highlightMovement(input.joyDir);

    i8 delta = input.joyDir == JoystickController::Direction::Left
        ? -1
        : (input.joyDir == JoystickController::Direction::Right ? 1 : 0);
    const auto oldPos = params.pos;

    params.pos = i8(params.pos + delta);
    params.pos
        = Tiny::clamp(params.pos, i8(0), i8(GameController::LeaderboardEntry::NAME_SIZE - 1));
    if (params.pos != oldPos)
        gameController.lcd.controller.setCursor(u8(params.pos), 1);

    delta = input.joyDir == JoystickController::Direction::Down
        ? -1
        : (input.joyDir == JoystickController::Direction::Up ? 1 : 0);
    if (delta) {
        i16 currentCharIdx = 0;
        for (i8 i = 0; i < i8(NAME_ALPHABET.len); ++i) {
            if (currentPlayer.name[params.pos] == NAME_ALPHABET.ptr[i])
                currentCharIdx = i;
        }

        currentCharIdx = Tiny::clamp(currentCharIdx + delta, 0, i16(NAME_ALPHABET.len - 1));
        const char letter = NAME_ALPHABET.ptr[currentCharIdx];

        currentPlayer.name[params.pos] = letter;
        gameController.lcd.controller.print(letter);
        gameController.lcd.controller.setCursor(u8(params.pos), 1);
    }

    if (u8(input.joyPress)) {
        for (i8 i = GameController::LEADERBOARD_SIZE - 1; i >= params.rank + 1; --i)
            gameController.leaderboard[i] = gameController.leaderboard[i - 1];
        gameController.leaderboard[params.rank] = currentPlayer;

        highlightPress(input.joyPress);
        saveToStorage();

        gameController.lcd.controller.noBlink();
        state = DEFAULT_MENU_STATE;
    }
}

void leaderboardUpdate(const Input& input)
{
    auto& state = gameController.state;
    auto& params = gameController.state.params.leaderboard;

    if (state.entry) {
        state.entry = false;

        auto& entry = gameController.leaderboard[state.params.leaderboard.pos];
        printfLCD(0, STR_FMT, UP_DOWN_ARROW_STR "LEADERBOARD <");
        printfLCD(
            1, "%1d. %-10s %2d", state.params.leaderboard.pos + 1, entry.name, entry.score);
    }

    highlightMovement(input.joyDir);

    const i8 delta = input.joyDir == JoystickController::Direction::Up
        ? -1
        : (input.joyDir == JoystickController::Direction::Down ? 1 : 0);
    const auto newPos
        = i8(Tiny::clamp(params.pos + delta, 0, GameController::LEADERBOARD_SIZE - 1));

    if (newPos != params.pos) {
        params.pos = newPos;

        auto& entry = gameController.leaderboard[state.params.leaderboard.pos];
        printfLCD(
            1, "%1d. %-10s %2d", state.params.leaderboard.pos + 1, entry.name, entry.score);
    }

    if (input.joyDir == JoystickController::Direction::Left)
        state = DEFAULT_MENU_STATE;
}

void saveToStorage()
{
    size_t eepromAddr = 0;
    for (const auto& data : STORAGE_DATA) {
        writeEEPROM(eepromAddr, data.addr, data.size);
        eepromAddr += data.size;
    }
}

void highlightMovement(const JoystickController::Direction joyDir)
{
    if (u8(joyDir))
        tone(MelodyPlayer::BUZZER_PIN, NOTE_FS3, INPUT_SOUND_DUR);
}

void highlightPress(const JoystickController::Press joyPress)
{
    if (u8(joyPress))
        tone(MelodyPlayer::BUZZER_PIN, NOTE_FS7, INPUT_SOUND_DUR);
}

GameController::GameController()
    : lcd({ { RS_PIN, ENABLE_PIN, D4, D5, D6, D7 }, {}, {} })
    , matrix({ { DIN_PIN, CLOCK_PIN, LOAD_PIN, 1 }, DEFAULT_MATRIX_INTENSITY })
{
}

void GameController::init()
{
    /* Fill index lists */
    Tiny::iota(matrixRowIndices);
    Tiny::iota(matrixColIndices);

    /* Read game info/settings from storage */
    size_t eepromAddr = 0;
    for (const auto& data : STORAGE_DATA) {
        readEEPROM(eepromAddr, data.addr, data.size);
        eepromAddr += data.size;
    }

    /* Initialize the matrix display */
    matrix.controller.shutdown(0, false);
    matrix.controller.setIntensity(0, i16(matrix.intensity));
    matrix.controller.clearDisplay(0);

    /* Initialize the LCD */
    lcd.controller.begin(NUM_COLS, NUM_ROWS);

    pinMode(CONTRAST_PIN, OUTPUT);
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    analogWrite(CONTRAST_PIN, i16(lcd.contrast));
    analogWrite(BRIGHTNESS_PIN, i16(lcd.brightness));

    for (auto& specialChar : SPECIAL_CHARS)
        lcd.controller.createChar(u8(specialChar.id), specialChar.data);

    lcd.controller.clear();

    /* Initialize the default state */
    state = { &greetUpdate, 0, true, {} };
}

void GameController::update(const Input& input) { state.updateFunc(input); }
