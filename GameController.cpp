#include "GameController.hpp"

using State = GameController::State;

constexpr u8 GameController::DEFAULT_CONTRAST;
constexpr u8 GameController::DEFAULT_BRIGHTNESS;

GameController gameController;

static constexpr const char* STR_FMT = "%-16s";
static constexpr const char* INT_FMT = "%-16d";
static constexpr u8 PRINTF_BUFSIZE = 17;
static char printfBuffer[PRINTF_BUFSIZE] = {};

template <typename... Ts> static void printfLCD(u8, const char*, Ts&&...);

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

/* clang-format off */
static constexpr Tiny::Pair<void*, u16> IN_STORAGE[] = {
    { &gameController.lcd.contrast,   sizeof(gameController.lcd.contrast)   },
    { &gameController.lcd.brightness, sizeof(gameController.lcd.brightness) },
    { &gameController.leaderboard,    sizeof(gameController.leaderboard)    },
};
static constexpr State DEFAULT_MENU_STATE = {
    &mainMenuUpdate,
    0,
    true,
    { .mainMenu = { 0 } },
};
/* clang-format on */

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

        printfLCD(0, STR_FMT, "HAVE FUN!");
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
                    { 0, 0 },
                    { 0, 0 },
                    255
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
    auto& lc = gameController.lc;
    auto& state = gameController.state;
    auto& params = gameController.state.params.game;

    if (state.entry) {
        state.entry = false;

        randomSeed(micros());

        printfLCD(0, STR_FMT, "PLAYING");
        printfLCD(1, INT_FMT, params.score);

        lc.setLed(0, params.player.y, params.player.x, true);
    }

    const auto oldPos = params.player;
    switch (input.joyDir) {
    case JoystickController::Direction::None:
        break;
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
        UNREACHABLE;
    }

    if (params.player != oldPos) {
        lc.setLed(0, oldPos.y, oldPos.x, false);
        lc.setLed(0, params.player.y, params.player.x, true);
    }

    if (params.player == params.food) {
        ++params.score;

        printfLCD(1, INT_FMT, params.score);

        while (params.food == params.player)
            params.food = {
                i8(random(GameController::MATRIX_SIZE)),
                i8(random(GameController::MATRIX_SIZE)),
            };

        lc.setLed(0, params.food.y, params.food.x, true);
    }

    if (params.player != params.player.clamp(0, GameController::MATRIX_SIZE - 1)) {
        lc.clearDisplay(0);

        const auto score = params.score;
        state = { &gameOverUpdate, input.currentTs, true, {} };

        /*
         *  This union member needs to be assigned separately because of an internal compiler
         *  error. See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59832
         */
        state.params.gameOver.score = score;
    }
}

void settingsUpdate(const Input& input)
{
    enum SettingsPosition : u8 {
        Contrast = 0,
        Brightness,
        NumPositions,
    };

    /* clang-format off */
    static constexpr const char* SETTINGS_DESCRIPTORS[NumPositions] = {
        [Contrast]   = ">Contrast",
        [Brightness] = ">Brightness",
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
    };
    /* clang-format on */

    auto& state = gameController.state;
    auto& params = gameController.state.params.settings;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "SETTINGS");
        printfLCD(1, STR_FMT, SETTINGS_DESCRIPTORS[params.pos]);

        size_t eepromAddr = 0;
        for (auto pair : IN_STORAGE) {
            writeEEPROM(eepromAddr, pair.first, pair.second);
            eepromAddr += pair.second;
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
    static constexpr const char* gitLink = "github.com/niculaionut/remember";
    static constexpr u32 gitLinkLen = strlen(gitLink);

    auto& state = gameController.state;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, STR_FMT, "REMEMBER");
        printfLCD(1, STR_FMT, "github.com/niculaionut/remember");
    }

    const u32 intervalNum = (input.currentTs - state.beginTs) / SCROLL_STEP;
    const auto oddInterval = intervalNum % 2;
    if (oddInterval)
        printfLCD(1, STR_FMT, gitLink + Tiny::clamp((intervalNum + 1) / 2, 0ul, gitLinkLen));

    if (intervalNum / 2 > gitLinkLen)
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

GameController::GameController()
    : lcd({ { RS_PIN, ENABLE_PIN, D4, D5, D6, D7 }, {}, {} })
    , lc(DIN_PIN, CLOCK_PIN, LOAD_PIN, 1)
{
}

void GameController::init()
{
    size_t eepromAddr = 0;
    for (auto pair : IN_STORAGE) {
        readEEPROM(eepromAddr, pair.first, pair.second);
        eepromAddr += pair.second;
    }

    lc.shutdown(0, false);
    lc.setIntensity(0, DEFAULT_MATRIX_BRIGHTNESS);
    lc.clearDisplay(0);

    lcd.controller.begin(NUM_COLS, NUM_ROWS);
    pinMode(CONTRAST_PIN, OUTPUT);
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    analogWrite(CONTRAST_PIN, i16(lcd.contrast));
    analogWrite(BRIGHTNESS_PIN, i16(lcd.brightness));

    state = { &greetUpdate, millis(), true, {} };
}

void GameController::update(const Input& input) { state.updateFunc(input); }
