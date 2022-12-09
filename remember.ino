#include "Arduino.h"
#include "GameController.hpp"
#include "EEPROM.h"
#include "LedControl.h"
#include "LiquidCrystal.h"

static JoystickController joystickController;

void setup()
{
    joystickController.init();
    gameController.init();
}

void loop()
{
    const auto currentTs = millis();
    const auto joyPress = joystickController.getButtonValue(currentTs);
    const auto joyDir = joystickController.getDirection();

    gameController.update({ currentTs, joyPress, joyDir });
}

int main()
{
    init();
    setup();
    for (;;)
        loop();
}
