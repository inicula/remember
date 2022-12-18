#include "GameController.hpp"

/* Typedefs */
using State = GameController::State;

/* Structs */
struct StorageEntry {
    void* addr;
    const void* defaultAddr;
    u16 size;
};

/* Static constexpr class variables */
constexpr u8 GameController::DEFAULT_CONTRAST;
constexpr u8 GameController::DEFAULT_BRIGHTNESS;
constexpr GameController::LeaderboardEntry GameController::DEFAULT_LEADERBOARD[];

/* Template function declarations */
template <typename... Ts> static void printfLCD(u8, const char*, Ts&&...);

/* Function declarations */
static void readEEPROM(size_t, void*, size_t);
static void writeEEPROM(size_t, const void*, size_t);
static void refreshContrast(i32);
static void refreshBrightness(i32);
static void greetUpdate(const Input&);
static void gameOverUpdate(const Input&);
static void mainMenuUpdate(const Input&);
static void startGameUpdate(const Input&);
static void settingsUpdate(const Input&);
static void aboutUpdate(const Input&);
static void sliderUpdate(const Input&);
static void highScoreUpdate(const Input&);
static void setDefaultState(const Input&);
static void nameSelectionUpdate(const Input&);

/* Extern variables */
GameController gameController;

/* Constexpr variables */
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

/* Static variables */
static char printfBuffer[PRINTF_BUFSIZE] = {};
static GameController::LeaderboardEntry currentPlayer = { "         ", 0 };
static Tiny::Array<u8, GameController::MATRIX_SIZE> matrixRowIndices = {};
static Tiny::Array<u8, GameController::MATRIX_SIZE> matrixColIndices = {};

template <typename... Ts> static void printfLCD(u8 row, const char* fmt, Ts&&... args)
{
    snprintf(&printfBuffer[0], PRINTF_BUFSIZE, fmt, args...);

    gameController.lcd.controller.setCursor(0, row);
    gameController.lcd.controller.print(&printfBuffer[0]);
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

void greetUpdate(const Input& input)
{
    static constexpr u32 DURATION = 5000;

    auto& state = gameController.state;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "REMEMBER");
        printfLCD(1, STR_FMT, "A Memory Game");
    }

    if (input.currentTs - state.beginTs > DURATION)
        state = DEFAULT_MENU_STATE;
}

void gameOverUpdate(const Input& input)
{
    static constexpr u32 DURATION = 5000;

    auto& state = gameController.state;
    auto& params = gameController.state.params.gameOver;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "GAME OVER!");
        printfLCD(1, "%-10s%6d", "Score:", params.score);
    }

    if (input.currentTs - state.beginTs > DURATION)
        state = DEFAULT_MENU_STATE;
}

void mainMenuUpdate(const Input& input)
{
    enum MenuPosition : u8 {
        StartGame = 0,
        Settings,
        About,
        NumPositions,
    };

    /* clang-format off */
    static constexpr const char* MENU_DESCRIPTORS[NumPositions] = {
        [StartGame] = ">Start Game",
        [Settings]  = ">Settings",
        [About]     = ">About",
    };
    static constexpr State MENU_TRANSITION_STATES[NumPositions] = {
        [StartGame] = {
            &startGameUpdate,
            0,
            true,
            {
                .game = {
                    {0, 0},
                    0,
                    0,
                    1,
                    0,
                    1,
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
            {}
        },
    };
    /* clang-format on */

    auto& state = gameController.state;
    auto& params = gameController.state.params.mainMenu;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "MAIN MENU");
        printfLCD(1, STR_FMT, MENU_DESCRIPTORS[params.pos]);
    }

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

void startGameUpdate(const Input& input)
{
    static constexpr u32 DEFAULT_TIME = 5000;
    static constexpr u32 LEVEL_TIME_DELTA = 100;

    enum class State : u8 {
        PreGame = 0,
        Game,
    };

    auto& lc = gameController.lc;
    auto& state = gameController.state;
    auto& params = gameController.state.params.game;

    if (state.entry) {
        state.entry = false;

        randomSeed(micros());

        printfLCD(0, STR_FMT, "Score/Remaining");
        printfLCD(1, "%-8d/%7d", params.score, params.remaining);

        Tiny::shuffle(matrixRowIndices);
        Tiny::shuffle(matrixColIndices);
    }

    if (params.subState == u8(State::PreGame)) {
        const auto onTime = (DEFAULT_TIME - params.level * LEVEL_TIME_DELTA) / 2;
        const u32 intervalNum = (input.currentTs - state.beginTs) / onTime;
        const auto oddInterval = intervalNum % 2;
        if (oddInterval && ((intervalNum + 1) / 2) == (params.tileIdx + 1)) {
            if (params.tileIdx < params.level)
                lc.setLed(0, matrixRowIndices[params.tileIdx],
                    matrixColIndices[params.tileIdx], true);

            ++params.tileIdx;
        }

        if (params.tileIdx == params.level + 1) {
            params.subState = u8(State::Game);
            lc.clearDisplay(0);
        }
    }
}

void settingsUpdate(const Input& input)
{
    enum SettingsPosition : u8 {
        Contrast = 0,
        Brightness,
        DefaultState,
        NumPositions,
    };

    /* clang-format off */
    static constexpr const char* SETTINGS_DESCRIPTORS[NumPositions] = {
        [Contrast]   = ">Contrast",
        [Brightness] = ">Brightness",
        [DefaultState] = ">Default state",
    };
    static constexpr State SETTING_TRANSITION_STATES[NumPositions] = {
        [Contrast] = {
            &sliderUpdate,
            0,
            true,
            {
                .slider = {
                    "CONTRAST",
                    &gameController.lcd.contrast,
                    0,
                    255,
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
                    "BRIGHTNESS",
                    &gameController.lcd.brightness,
                    0,
                    255,
                    &refreshBrightness
                }
            }
        },
        [DefaultState] = {
            &setDefaultState,
        }
    };
    /* clang-format on */

    auto& state = gameController.state;
    auto& params = gameController.state.params.settings;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "SETTINGS");
        printfLCD(1, STR_FMT, SETTINGS_DESCRIPTORS[params.pos]);

        size_t eepromAddr = 0;
        for (const auto& data : STORAGE_DATA) {
            writeEEPROM(eepromAddr, data.addr, data.size);
            eepromAddr += data.size;
        }
    }

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
    static constexpr u32 SCROLL_STEP = 250;
    static constexpr Tiny::String GIT_LINK = "github.com/niculaionut/remember";

    auto& state = gameController.state;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "REMEMBER");
        printfLCD(1, STR_FMT, "github.com/niculaionut/remember");
    }

    const u32 intervalNum = (input.currentTs - state.beginTs) / SCROLL_STEP;
    const auto oddInterval = intervalNum % 2;
    if (oddInterval)
        printfLCD(
            1, STR_FMT, GIT_LINK.ptr + Tiny::clamp((intervalNum + 1) / 2, 0u, GIT_LINK.len));

    if (intervalNum / 2 > GIT_LINK.len)
        state = DEFAULT_MENU_STATE;
}

void sliderUpdate(const Input& input)
{
    static constexpr i32 STEP = 10;

    auto& state = gameController.state;
    auto& params = gameController.state.params.slider;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, params.description);
        printfLCD(1, "%-10s%6d", "Up/Down", *params.value);
    }

    const i32 delta = input.joyDir == JoystickController::Direction::Up
        ? 1
        : (input.joyDir == JoystickController::Direction::Down ? -1 : 0);
    const i32 newValue = Tiny::clamp(*params.value + STEP * delta, params.min, params.max);

    if (*params.value != newValue) {
        *params.value = newValue;
        printfLCD(1, "%-10s%6d", "Up/Down", newValue);
        params.callback(newValue);
    }

    if (input.joyDir == JoystickController::Direction::Left)
        state = { &settingsUpdate, 0, true, {} };
}

void highScoreUpdate(const Input& input)
{
    static constexpr u32 SCROLL_STEP = 250;
    static constexpr Tiny::String CONGRATS_MSG = "CONGRATS! YOUR LEADERBOARD POSITION:";

    auto& state = gameController.state;
    auto& params = gameController.state.params.slider;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, CONGRATS_MSG.ptr);
    }

    const u32 intervalNum = (input.currentTs - state.beginTs) / SCROLL_STEP;
    const auto oddInterval = intervalNum % 2;
    if (oddInterval)
        printfLCD(0, STR_FMT,
            CONGRATS_MSG.ptr + Tiny::clamp((intervalNum + 1) / 2, 0u, CONGRATS_MSG.len));

    if (intervalNum / 2 > CONGRATS_MSG.len)
        state = DEFAULT_MENU_STATE;
}

void setDefaultState(const Input&)
{
    size_t eepromAddr = 0;
    for (const auto& data : STORAGE_DATA) {
        writeEEPROM(eepromAddr, data.defaultAddr, data.size);
        memcpy(data.addr, data.defaultAddr, data.size);
        eepromAddr += data.size;
    }

    refreshBrightness(gameController.lcd.contrast);
    refreshBrightness(gameController.lcd.brightness);

    gameController.state = { &settingsUpdate, 0, true, {} };
}

void nameSelectionUpdate(const Input& input)
{
    static constexpr Tiny::String NAME_ALPHABET = " ABCDEFGHIJKLMNOPRSTUVWXYZ0123456789";

    auto& state = gameController.state;
    auto& params = gameController.state.params.nameSelection;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "Your name:");
        gameController.lcd.controller.setCursor(0, 1);
        gameController.lcd.controller.noAutoscroll();
        gameController.lcd.controller.blink();
    }

    i8 delta = input.joyDir == JoystickController::Direction::Left
        ? -1
        : (input.joyDir == JoystickController::Direction::Right ? 1 : 0);
    const auto oldPos = params.pos;

    params.pos = i8(params.pos + delta);
    params.pos = Tiny::clamp(params.pos, 0, GameController::LeaderboardEntry::NAME_SIZE - 1);
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
}

GameController::GameController()
    : lcd({ { RS_PIN, ENABLE_PIN, D4, D5, D6, D7 }, {}, {} })
    , lc(DIN_PIN, CLOCK_PIN, LOAD_PIN, 1)
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
    lc.shutdown(0, false);
    lc.setIntensity(0, DEFAULT_MATRIX_BRIGHTNESS);
    lc.clearDisplay(0);

    /* Initialize the LCD */
    lcd.controller.begin(NUM_COLS, NUM_ROWS);
    pinMode(CONTRAST_PIN, OUTPUT);
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    analogWrite(CONTRAST_PIN, i16(lcd.contrast));
    analogWrite(BRIGHTNESS_PIN, i16(lcd.brightness));

    /* Initialize the default state */
    state = { &greetUpdate, millis(), true, {} };
}

void GameController::update(const Input& input) { state.updateFunc(input); }
