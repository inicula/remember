#include "DisplayController.h"

using State = DisplayController::State;

constexpr u8 DisplayController::DEFAULT_CONTRAST;
constexpr u8 DisplayController::DEFAULT_BRIGHTNESS;

DisplayController displayController;

static void refreshContrast(const void*);
static void refreshBrightness(const void*);
static void greetUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void gameOverUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void mainMenuUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void startGameUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void settingsUpdate(u32, JoystickController::Press, JoystickController::Direction);
static void aboutUpdate(u32, JoystickController::Press, JoystickController::Direction);
template <i32 DIFF = 10>
static void sliderUpdate(u32, JoystickController::Press, JoystickController::Direction);

static constexpr Tiny::Pair<void*, u16> SETTINGS_FROM_STORAGE[] = {
    { &displayController.contrast, sizeof(displayController.contrast) },
    { &displayController.brightness, sizeof(displayController.brightness) },
};
static constexpr State DEFAULT_MENU_STATE
    = { &mainMenuUpdate, 0, true, { .mainMenu = { 0 } } };

static void eepromRead(void* addr, size_t eepromBaseAddr, size_t count)
{
    u8 buffer[count];

    for (size_t i = 0; i < count; ++i)
        buffer[i] = EEPROM.read(i16(eepromBaseAddr + i));

    memcpy(addr, &buffer[0], count);
}

static void eepromWrite(const void* addr, size_t eepromBaseAddr, size_t count)
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

    auto& lcd = displayController.lcd;
    auto& state = displayController.state;

    if (state.entry) {
        state.entry = false;

        lcd.clear();
        lcd.print("HAVE FUN!");
    }

    if (currentTs - state.timestamp > DURATION)
        state = DEFAULT_MENU_STATE;
}

void gameOverUpdate(u32 currentTs, JoystickController::Press, JoystickController::Direction)
{
    static constexpr u32 DURATION = 5000;

    auto& lcd = displayController.lcd;
    auto& state = displayController.state;
    auto& params = displayController.state.params.gameOver;

    if (state.entry) {
        state.entry = false;

        lcd.clear();
        lcd.print("GAME OVER");
        lcd.setCursor(0, 1);
        lcd.print("SCORE: ");
        lcd.print(params.score);
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

    static constexpr const char* MENU_DESCRIPTORS[NumPositions] = {
        [StartGame] = ">Start Game     ",
        [Settings]  = ">Settings       ",
        [About]     = ">About          ",
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

    auto& lcd = displayController.lcd;
    auto& state = displayController.state;
    auto& params = displayController.state.params.mainMenu;

    if (state.entry) {
        state.entry = false;

        lcd.clear();
        lcd.print("MAIN MENU");
        lcd.setCursor(0, 1);
        lcd.print(MENU_DESCRIPTORS[params.pos]);
    }

    const i8 delta = joyDir == JoystickController::Direction::Up
        ? 1
        : (joyDir == JoystickController::Direction::Down ? -1 : 0);
    const auto newPos = i8(Tiny::clamp(params.pos + delta, 0, NumPositions - 1));

    if (newPos != params.pos) {
        params.pos = newPos;

        lcd.setCursor(0, 1);
        lcd.print(MENU_DESCRIPTORS[params.pos]);
    }

    if (joyDir == JoystickController::Direction::Right) {
        state = MENU_TRANSITION_STATES[params.pos];
        state.timestamp = currentTs;
    }
}

void startGameUpdate(
    u32 currentTs, JoystickController::Press, JoystickController::Direction joyDir)
{
    auto& lcd = displayController.lcd;
    auto& lc = displayController.lc;
    auto& state = displayController.state;
    auto& params = displayController.state.params.game;

    if (state.entry) {
        state.entry = false;

        randomSeed(micros());

        lcd.clear();
        lcd.print("PLAYING");
        lcd.setCursor(0, 1);
        lcd.print(params.score);
        lcd.print("  ");

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

        lcd.setCursor(0, 1);
        lcd.print(params.score);
        lcd.print("  ");

        while (params.food == params.player)
            params.food = {
                i8(random(DisplayController::MATRIX_SIZE)),
                i8(random(DisplayController::MATRIX_SIZE)),
            };

        lc.setLed(0, params.food.y, params.food.x, true);
    }

    if(params.player != params.player.clamp(0, DisplayController::MATRIX_SIZE - 1)) {
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

    static constexpr const char* SETTINGS_DESCRIPTORS[NumPositions] = {
        [Contrast]   = ">Contrast       ",
        [Brightness] = ">Brightness     ",
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

    auto& lcd = displayController.lcd;
    auto& state = displayController.state;
    auto& params = displayController.state.params.settings;

    if (state.entry) {
        state.entry = false;

        lcd.clear();
        lcd.print("Settings");
        lcd.setCursor(0, 1);
        lcd.print(SETTINGS_DESCRIPTORS[params.pos]);

        size_t eepromAddr = 0;
        for (auto pair : SETTINGS_FROM_STORAGE) {
            eepromWrite(pair.first, eepromAddr, pair.second);
            eepromAddr += pair.second;
        }
    }

    const i8 delta = joyDir == JoystickController::Direction::Up
        ? 1
        : (joyDir == JoystickController::Direction::Down ? -1 : 0);
    const auto newPos = i8(Tiny::clamp(params.pos + delta, 0, NumPositions - 1));

    if (newPos != params.pos) {
        params.pos = newPos;

        lcd.setCursor(0, 1);
        lcd.print(SETTINGS_DESCRIPTORS[params.pos]);
    }

    if (joyDir == JoystickController::Direction::Right)
        state = SETTING_TRANSITION_STATES[params.pos];
    if (joyDir == JoystickController::Direction::Left)
        state = DEFAULT_MENU_STATE;
}

void aboutUpdate(u32 currentTs, JoystickController::Press, JoystickController::Direction)
{
    static constexpr u32 DURATION = 3000;

    auto& lcd = displayController.lcd;
    auto& state = displayController.state;

    if (state.entry) {
        state.entry = false;

        lcd.clear();
        lcd.print("QUASI-SNAKE");
        lcd.setCursor(0, 1);
        lcd.print("Nicula Ionut 334");
    }

    if (currentTs - state.timestamp > DURATION)
        state = DEFAULT_MENU_STATE;
}

template <i32 DIFF>
void sliderUpdate(u32, JoystickController::Press, JoystickController::Direction joyDir)
{
    auto& lcd = displayController.lcd;
    auto& state = displayController.state;
    auto& params = displayController.state.params.slider;

    if (state.entry) {
        state.entry = false;

        lcd.clear();
        lcd.print(params.description);
        lcd.setCursor(0, 1);
        lcd.print(*params.value);
        lcd.print("   ");
    }

    const i32 delta = joyDir == JoystickController::Direction::Up
        ? 1
        : (joyDir == JoystickController::Direction::Down ? -1 : 0);
    const auto newValue = Tiny::clamp(*params.value - DIFF * delta, params.min, params.max);

    if (*params.value != newValue) {
        *params.value = newValue;

        lcd.setCursor(0, 1);
        lcd.print(*params.value);
        lcd.print("   ");

        params.callback(params.value);
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
        eepromRead(pair.first, eepromAddr, pair.second);
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
