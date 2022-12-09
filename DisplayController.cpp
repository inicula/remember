#include "DisplayController.h"

using State = DisplayController::State;

constexpr u8 DisplayController::DEFAULT_CONTRAST;
constexpr u8 DisplayController::DEFAULT_BRIGHTNESS;

DisplayController displayController;

static constexpr u8 PRINTF_BUFSIZE = 17;
static char printfBuffer[PRINTF_BUFSIZE] = {};

template <typename... Ts> static void printfLCD(u8, const char*, Ts&&...);

static void readEEPROM(void*, size_t, size_t);
static void writeEEPROM(const void*, size_t, size_t);
static void refreshContrast(const void*);
static void refreshBrightness(const void*);
static void greetUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void gameOverUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void mainMenuUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void startGameUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void settingsUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void aboutUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void sliderUpdate(u32, JoystickController::Press, JoystickController::Direction);

static constexpr Tiny::Pair<void*, u16> SETTINGS_FROM_STORAGE[] = {
    { &displayController.contrast, sizeof(displayController.contrast) },
    { &displayController.brightness, sizeof(displayController.brightness) },
};
static constexpr State DEFAULT_MENU_STATE
    = { &mainMenuUpdate, 0, true, { .mainMenu = { 0 } } };

template <typename... Ts> static void printfLCD(u8 row, const char* fmt, Ts&&... args)
{
    snprintf(&printfBuffer[0], PRINTF_BUFSIZE, fmt, args...);

    displayController.lcd.setCursor(0, row);
    displayController.lcd.print(&printfBuffer[0]);
}

static void readEEPROM(void* addr, size_t eepromBaseAddr, size_t count)
{
    u8 buffer[count];

    for (size_t i = 0; i < count; ++i)
        buffer[i] = EEPROM.read(i16(eepromBaseAddr + i));

    memcpy(addr, &buffer[0], count);
}

static void writeEEPROM(const void* addr, size_t eepromBaseAddr, size_t count)
{
    u8 buffer[count];
    memcpy(&buffer[0], addr, count);

    for (size_t i = 0; i < count; ++i)
        EEPROM.update(i16(eepromBaseAddr + i), buffer[i]);
}

void refreshContrast(const void* data)
{
    analogWrite(DisplayController::CONTRAST_PIN, int(*(const i32*)(data)));
}

void refreshBrightness(const void* data)
{
    analogWrite(DisplayController::BRIGHTNESS_PIN, int(*(const i32*)(data)));
}

void greetUpdate(u32 currentTs, JoystickController::Press, JoystickController::Direction)
{
    static constexpr u32 DURATION = 5000;

    auto& state = displayController.state;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, "%-16s", "HAVE FUN!");
    }

    if (currentTs - state.timestamp > DURATION)
        state = DEFAULT_MENU_STATE;
}

void gameOverUpdate(u32 currentTs, JoystickController::Press, JoystickController::Direction)
{
    static constexpr u32 DURATION = 5000;

    auto& state = displayController.state;
    auto& params = displayController.state.params.gameOver;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, "%-16s", "GAME OVER!");
        printfLCD(1, "%-10s%-6d", "SCORE:", params.score);
    }

    if (currentTs - state.timestamp > DURATION)
        state = DEFAULT_MENU_STATE;
}

void mainMenuUpdate(
    u32 currentTs, JoystickController::Press, JoystickController::Direction joyDir)
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

    auto& state = displayController.state;
    auto& params = displayController.state.params.mainMenu;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, "%-16s", "MAIN MENU");
        printfLCD(1, "%-16s", MENU_DESCRIPTORS[params.pos]);
    }

    const i8 delta = joyDir == JoystickController::Direction::Up
        ? 1
        : (joyDir == JoystickController::Direction::Down ? -1 : 0);
    const auto newPos = i8(Tiny::clamp(params.pos + delta, 0, NumPositions - 1));

    if (newPos != params.pos) {
        params.pos = newPos;

        printfLCD(1, "%-16s", MENU_DESCRIPTORS[params.pos]);
    }

    if (joyDir == JoystickController::Direction::Right) {
        state = MENU_TRANSITION_STATES[params.pos];
        state.timestamp = currentTs;
    }
}

void startGameUpdate(
    u32 currentTs, JoystickController::Press, JoystickController::Direction joyDir)
{
    auto& lc = displayController.lc;
    auto& state = displayController.state;
    auto& params = displayController.state.params.game;

    if (state.entry) {
        state.entry = false;

        randomSeed(micros());

        printfLCD(0, "%-16s", "PLAYING");
        printfLCD(1, "%-16d", params.score);

        lc.setLed(0, params.player.y, params.player.x, true);
    }

    const auto oldPos = params.player;
    switch (joyDir) {
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

        printfLCD(1, "%-16d", params.score);

        while (params.food == params.player)
            params.food = {
                i8(random(DisplayController::MATRIX_SIZE)),
                i8(random(DisplayController::MATRIX_SIZE)),
            };

        lc.setLed(0, params.food.y, params.food.x, true);
    }

    if (params.player != params.player.clamp(0, DisplayController::MATRIX_SIZE - 1)) {
        lc.clearDisplay(0);

        const auto score = params.score;
        state = { gameOverUpdate, currentTs, true, {} };
        state.params.gameOver.score
            = score; /* Separately, otherwise internal compiler error */
    }
}

void settingsUpdate(u32, JoystickController::Press, JoystickController::Direction joyDir)
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
                    "Contrast",
                    &displayController.contrast,
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
                    "Brightness",
                    &displayController.brightness,
                    0,
                    255,
                    &refreshBrightness
                }
            }
        },
    };
    /* clang-format on */

    auto& state = displayController.state;
    auto& params = displayController.state.params.settings;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, "%-16s", "SETTINGS");
        printfLCD(1, "%-16s", SETTINGS_DESCRIPTORS[params.pos]);

        size_t eepromAddr = 0;
        for (auto pair : SETTINGS_FROM_STORAGE) {
            writeEEPROM(pair.first, eepromAddr, pair.second);
            eepromAddr += pair.second;
        }
    }

    const i8 delta = joyDir == JoystickController::Direction::Up
        ? 1
        : (joyDir == JoystickController::Direction::Down ? -1 : 0);
    const auto newPos = i8(Tiny::clamp(params.pos + delta, 0, NumPositions - 1));

    if (newPos != params.pos) {
        params.pos = newPos;

        printfLCD(1, "%-16s", SETTINGS_DESCRIPTORS[params.pos]);
    }

    if (joyDir == JoystickController::Direction::Right)
        state = SETTING_TRANSITION_STATES[params.pos];
    if (joyDir == JoystickController::Direction::Left)
        state = DEFAULT_MENU_STATE;
}

void aboutUpdate(u32 currentTs, JoystickController::Press, JoystickController::Direction)
{
    static constexpr u32 DURATION = 3000;

    auto& state = displayController.state;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, "%-16s", "QUASI_SNAKE");
        printfLCD(1, "%-16s", "Nicula Ionut 334");
    }

    if (currentTs - state.timestamp > DURATION)
        state = DEFAULT_MENU_STATE;
}

void sliderUpdate(u32, JoystickController::Press, JoystickController::Direction joyDir)
{
    static constexpr i32 STEP = 10;

    auto& state = displayController.state;
    auto& params = displayController.state.params.slider;

    if (state.entry) {
        state.entry = false;

        printfLCD(0, "%-16s", params.description);
        printfLCD(1, "%-16d", *params.value);
    }

    const i32 delta = joyDir == JoystickController::Direction::Up
        ? 1
        : (joyDir == JoystickController::Direction::Down ? -1 : 0);
    const auto newValue = Tiny::clamp(*params.value - STEP * delta, params.min, params.max);

    if (*params.value != newValue) {
        *params.value = newValue;
        printfLCD(1, "%-16d", newValue);
        params.callback(&newValue);
    }

    if (joyDir == JoystickController::Direction::Left)
        state = { &settingsUpdate, 0, true, {} };
}

DisplayController::DisplayController()
    : lcd(RS_PIN, ENABLE_PIN, D4, D5, D6, D7)
    , lc(DIN_PIN, CLOCK_PIN, LOAD_PIN, 1)
{
}

void DisplayController::init()
{
    size_t eepromAddr = 0;
    for (auto pair : SETTINGS_FROM_STORAGE) {
        readEEPROM(pair.first, eepromAddr, pair.second);
        eepromAddr += pair.second;
    }

    lc.shutdown(0, false);
    lc.setIntensity(0, DEFAULT_MATRIX_BRIGHTNESS);
    lc.clearDisplay(0);

    lcd.begin(NUM_COLS, NUM_ROWS);
    pinMode(CONTRAST_PIN, OUTPUT);
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    analogWrite(CONTRAST_PIN, i16(contrast));
    analogWrite(BRIGHTNESS_PIN, i16(brightness));

    state = { greetUpdate, millis(), true, {} };
}

void DisplayController::update(
    u32 currentTs, JoystickController::Press joyPress, JoystickController::Direction joyDir)
{
    state.updateFunc(currentTs, joyPress, joyDir);
}
