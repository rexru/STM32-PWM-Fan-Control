#include "LCD_DISCO_F429ZI.h"   // LCD driver
#include "TS_DISCO_F429ZI.h"    // Touchscreen driver
#include "mbed.h"               // Mbed OS
#include <math.h>               // For ceil()

/* -------------------------------
   Screen and UI button definitions
---------------------------------*/
#define SCREEN_W 240            // LCD width (pixels)
#define SCREEN_H 320            // LCD height (pixels)

// Plus button coordinates (bottom-left area)
#define PLUS_BTN_X_START 0
#define PLUS_BTN_X_END 100
#define PLUS_BTN_Y_START 220
#define PLUS_BTN_Y_END 320

// Minus button coordinates (bottom-right area)
#define MINUS_BTN_X_START 140
#define MINUS_BTN_X_END 240
#define MINUS_BTN_Y_START 220
#define MINUS_BTN_Y_END 320

/* -------------------------------
   Hardware configuration
---------------------------------*/
LCD_DISCO_F429ZI display;        // LCD object
TS_DISCO_F429ZI touchScreen;     // Touchscreen object
PwmOut fanMotor(PA_6);           // Fan motor (PWM output pin)
AnalogIn tempSensor(PA_0);       // LM35 temperature sensor (ADC input pin)

/* -------------------------------
   Global variables
---------------------------------*/
float tempCurrent;               // Current temperature (°C)
float tempLimit;                 // User-set threshold (°C)
int fanLevel = 0;                // PWM pulse width (0–256 µs)
bool plusTouched = false;        // True if "+" button pressed
bool minusTouched = false;       // True if "–" button pressed

/* -------------------------------
   Timers and periodic tasks
---------------------------------*/
Ticker fanRampTicker;            // Gradually increases fan speed
Ticker tempMonitorTicker;        // Monitors temp and controls fan
Timer touchCooldown;             // Debounces touchscreen inputs

/* -------------------------------
   UI Rendering
---------------------------------*/
void renderButtons() {
    // Normal state: draw buttons in black
    if (!plusTouched && !minusTouched) {
        // "+" button
        display.FillRect(PLUS_BTN_X_START + 15, PLUS_BTN_Y_START + 35,
                         PLUS_BTN_X_END - PLUS_BTN_X_START - 30, 25);
        display.FillRect(PLUS_BTN_X_START + 35, PLUS_BTN_Y_START + 15,
                         25, PLUS_BTN_Y_END - PLUS_BTN_Y_START - 30);

        // "–" button
        display.FillRect(MINUS_BTN_X_START + 15, MINUS_BTN_Y_START + 35,
                         MINUS_BTN_X_END - MINUS_BTN_X_START - 30, 25);
    }
    // Flash "+" button in red for feedback
    else if (plusTouched) {
        plusTouched = false;
        for (int j = 0; j < 800; j++) {
            display.SetTextColor(LCD_COLOR_RED);
            display.FillRect(PLUS_BTN_X_START + 15, PLUS_BTN_Y_START + 35,
                             PLUS_BTN_X_END - PLUS_BTN_X_START - 30, 25);
            display.FillRect(PLUS_BTN_X_START + 35, PLUS_BTN_Y_START + 15,
                             25, PLUS_BTN_Y_END - PLUS_BTN_Y_START - 30);

            display.SetTextColor(LCD_COLOR_BLACK);
            display.FillRect(MINUS_BTN_X_START + 15, MINUS_BTN_Y_START + 35,
                             MINUS_BTN_X_END - MINUS_BTN_X_START - 30, 25);
        }
    }
    // Flash "–" button in red for feedback
    else if (minusTouched) {
        minusTouched = false;
        for (int j = 0; j < 800; j++) {
            display.FillRect(PLUS_BTN_X_START + 15, PLUS_BTN_Y_START + 35,
                             PLUS_BTN_X_END - PLUS_BTN_X_START - 30, 25);
            display.FillRect(PLUS_BTN_X_START + 35, PLUS_BTN_Y_START + 15,
                             25, PLUS_BTN_Y_END - PLUS_BTN_Y_START - 30);

            display.SetTextColor(LCD_COLOR_RED);
            display.FillRect(MINUS_BTN_X_START + 15, MINUS_BTN_Y_START + 35,
                             MINUS_BTN_X_END - MINUS_BTN_X_START - 30, 25);

            display.SetTextColor(LCD_COLOR_BLACK);
        }
    }
}

/* -------------------------------
   Fan control
---------------------------------*/

// Gradually ramps up fan speed
void rampUpFan() {
    if (fanLevel < 256) {
        fanLevel += 2;
        fanMotor.pulsewidth_us(fanLevel);
    }
}

// Start fan with gradual ramp
void startFan() {
    fanRampTicker.attach(&rampUpFan, 0.1); // Every 100 ms
}

// Stop fan immediately
void stopFan() {
    fanLevel = 0;
    fanRampTicker.detach();
    fanMotor.pulsewidth_us(fanLevel);
}

/* -------------------------------
   Temperature handling
---------------------------------*/

// Convert sensor voltage to °C
float getTempFromVoltage(float volts) {
    // Simplified conversion: LM35 output is 10 mV/°C
    return (volts * 100);
}

// Read and display current temperature & threshold
void checkTemperature() {
    float volts = tempSensor.read(); // 0.0–1.0 (ADC normalized)
    tempCurrent = getTempFromVoltage(volts);

    display.SetFont(&Font24);
    display.SetTextColor(LCD_COLOR_DARKBLUE);

    char tempText[20];

    // Display current temperature
    int tempVal = (int)(tempCurrent * 10);
    snprintf(tempText, sizeof(tempText), "Temp: %d.%dC", tempVal / 10, tempVal % 10);
    display.DisplayStringAt(0, 45, (uint8_t *)tempText, CENTER_MODE);

    // Display set threshold
    tempVal = (int)(tempLimit * 10);
    snprintf(tempText, sizeof(tempText), "Limit: %d.%dC", tempVal / 10, tempVal % 10);
    display.DisplayStringAt(0, 85, (uint8_t *)tempText, CENTER_MODE);
}

// Enable/disable fan based on temperature
void monitorFan() {
    if (tempCurrent > tempLimit) {
        startFan();
    } else {
        stopFan();
    }
}

/* -------------------------------
   Main program
---------------------------------*/
int main() {
    TS_StateTypeDef touchState;
    uint16_t touchRawX, touchRawY;

    // Configure PWM for fan
    fanMotor.period_us(256);       // ~3.9 kHz
    fanMotor.pulsewidth_us(0);     // Start with fan off

    // LCD setup
    display.SetFont(&Font24);
    display.Clear(LCD_COLOR_WHITE);
    display.SetTextColor(LCD_COLOR_DARKBLUE);

    // Initial readings
    checkTemperature();
    tempLimit = ceil(tempCurrent) + 1.0f; // Default: 1 °C above current temp
    tempMonitorTicker.attach(&monitorFan, 1.0); // Check every second

    renderButtons();
    touchCooldown.start();

    while (true) {
        // Read touchscreen input
        touchScreen.GetState(&touchState);

        if (touchState.TouchDetected && touchCooldown.read_ms() > 500) {
            touchCooldown.reset();
            touchRawX = touchState.X;
            touchRawY = touchState.Y;

            // Detect button presses
            if (touchRawX < 120 && touchRawY < 160) { // (⚠️ Region should be corrected)
                plusTouched = true;
                tempLimit += 0.5;
            } 
            else if (touchRawX > 120 && touchRawY < 160) {
                minusTouched = true;
                tempLimit -= 0.5;
            }
        }

        checkTemperature();
        renderButtons();
    }
}
