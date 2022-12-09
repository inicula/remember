#include "Arduino.h"
#include "DisplayController.h"
#include "EEPROM.h"
#include "LedControl.h"
#include "LiquidCrystal.h"

static JoystickController joystickController;

void setup()
{
    joystickController.init();
    displayController.init();
}

void loop()
{
    const auto currentTs = millis();
    const auto joyPress = joystickController.getButtonValue(currentTs);
    const auto joyDir = joystickController.getDirection();

    displayController.update(currentTs, joyPress, joyDir);
}

int main()
{
    init();
    setup();
    for (;;)
        loop();
}
