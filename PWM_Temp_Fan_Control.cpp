#include "LCD_DISCO_F429ZI.h"  // LCD library
#include "TS_DISCO_F429ZI.h"   // Touchscreen library
#include "mbed.h"              // Mbed OS library
#include <math.h>              // For ceil()


// These define the LCD dimensions and button regions in pixels
#define SCREEN_W 240           // Total width of the LCD screen
#define SCREEN_H 320           // Total height of the LCD screen
#define PLUS_BTN_X_START 0     // Left edge of the plus button
#define PLUS_BTN_X_END 100     // Right edge of the plus button
#define PLUS_BTN_Y_START 220   // Top edge of the plus button
#define PLUS_BTN_Y_END 320     // Bottom edge of the plus button (bottom of screen)
#define MINUS_BTN_X_START 140  // Left edge of the minus button
#define MINUS_BTN_X_END 240    // Right edge of the minus button (right of screen)
#define MINUS_BTN_Y_START 220  // Top edge of the minus button
#define MINUS_BTN_Y_END 320    // Bottom edge of the minus button

// Hardware setup
LCD_DISCO_F429ZI display;      // Object to control the LCD screen
TS_DISCO_F429ZI touchScreen;   // Object to read touchscreen input
PwmOut fanMotor(PA_6);        // PWM output on pin PD_14 to control fan speed
AnalogIn tempSensor(PA_0);     // Analog input on pin PA_0 to read LM35 temperature sensor voltage

// Global variables
float tempCurrent;             // Stores the current temperature in °C, updated from sensor
float tempLimit;               // Stores the user-set temperature threshold in °C
int fanLevel = 0;              // Fan PWM pulse width in microseconds (0-256 µs), starts at 0 (off)
bool plusTouched = false;      // Flag: true when plus button is pressed, triggers feedback
bool minusTouched = false;     // Flag: true when minus button is pressed, triggers feedback

// Timing objects
Ticker fanRampTicker;          // Periodic timer to gradually increase fan speed
Ticker tempMonitorTicker;      // Periodic timer to check temperature and control fan
Timer touchCooldown;           // Timer to debounce touchscreen input (prevent rapid presses)

// Draw buttons with feedback
void renderButtons() {
    // If neither button is pressed, draw them in their normal state (black)
    if (plusTouched == false && minusTouched == false) {
        // Draw plus button as a cross
        // Horizontal bar: offset 15 from left, 35 from top, width adjusted to fit within button
        display.FillRect(PLUS_BTN_X_START + 15, PLUS_BTN_Y_START + 35, PLUS_BTN_X_END - PLUS_BTN_X_START - 30, 25);
        // Vertical bar: offset 35 from left, 15 from top, height adjusted to fit
        display.FillRect(PLUS_BTN_X_START + 35, PLUS_BTN_Y_START + 15, 25, PLUS_BTN_Y_END - PLUS_BTN_Y_START - 30);
        // Draw minus button as a horizontal line
        // Offset 15 from left, 35 from top, width adjusted to fit
        display.FillRect(MINUS_BTN_X_START + 15, MINUS_BTN_Y_START + 35, MINUS_BTN_X_END - MINUS_BTN_X_START - 30, 25);
    }
    // If plus button was pressed, flash it red
    else if (plusTouched) {
        plusTouched = false; // Reset flag immediately so it only flashes once
        for (int j = 0; j < 800; j++) { // Loop 800 times to create a blocking flash effect
            display.SetTextColor(LCD_COLOR_RED); // Set color to red for feedback
            // Redraw plus button in red
            display.FillRect(PLUS_BTN_X_START + 15, PLUS_BTN_Y_START + 35, PLUS_BTN_X_END - PLUS_BTN_X_START - 30, 25);
            display.FillRect(PLUS_BTN_X_START + 35, PLUS_BTN_Y_START + 15, 25, PLUS_BTN_Y_END - PLUS_BTN_Y_START - 30);
            display.SetTextColor(LCD_COLOR_BLACK); // Switch back to black
            // Redraw minus button to ensure it stays black
            display.FillRect(MINUS_BTN_X_START + 15, MINUS_BTN_Y_START + 35, MINUS_BTN_X_END - MINUS_BTN_X_START - 30, 25);
        } // This blocks execution, making the UI unresponsive briefly (~800ms depending on CPU speed)
    }
    // If minus button was pressed, flash it red
    else if (minusTouched) {
        minusTouched = false; // Reset flag immediately
        for (int j = 0; j < 800; j++) { // Blocking loop for flash effect
            // Redraw plus button in black to keep it normal
            display.FillRect(PLUS_BTN_X_START + 15, PLUS_BTN_Y_START + 35, PLUS_BTN_X_END - PLUS_BTN_X_START - 30, 25);
            display.FillRect(PLUS_BTN_X_START + 35, PLUS_BTN_Y_START + 15, 25, PLUS_BTN_Y_END - PLUS_BTN_Y_START - 30);
            display.SetTextColor(LCD_COLOR_RED); // Set color to red
            // Redraw minus button in red
            display.FillRect(MINUS_BTN_X_START + 15, MINUS_BTN_Y_START + 35, MINUS_BTN_X_END - MINUS_BTN_X_START - 30, 25);
            display.SetTextColor(LCD_COLOR_BLACK); // Reset to black
        } // Blocks execution similarly
    }
}

// Ramp up fan speed
void rampUpFan() {
    // Gradually increase fan speed until it reaches max (256 µs)
    if (fanLevel < 256) {
        fanLevel += 2;          // Increment by 2 µs each time (gradual increase)
        fanMotor.pulsewidth_us(fanLevel); // Set new PWM pulse width
    }
}

// Activate fan
void startFan() {
    // Attach ticker to call rampUpFan every 100ms, starting the fan if not already running
    fanRampTicker.attach(&rampUpFan, 0.1); // 0.1s = 100ms interval
}

// Deactivate fan
void stopFan() {
    fanLevel = 0;              // Reset fan speed to 0 (off)
    fanRampTicker.detach();    // Stop the ticker from calling rampUpFan
    fanMotor.pulsewidth_us(fanLevel); // Set PWM to 0 µs
}

// Convert ADC to temp (same flaw as yours)
float getTempFromVoltage(float volts) {
    // Convert ADC reading (0.0-1.0) to temperature
    
    return (volts * 100); // this conveninetly cancels out to volts * 100 bc reference votl of 3 and gain of 3
}

// Measure and display temperature
void checkTemperature() {
    float volts = tempSensor.read(); // Read ADC value from PA_0 (returns 0.0 to 1.0)
    tempCurrent = getTempFromVoltage(volts); // Convert to temperature (flawed but matches your code)

    display.SetFont(&Font24);        // Set font size to large (24-point) for readability
    display.SetTextColor(LCD_COLOR_DARKBLUE); // Set text color for temp display

    char tempText[20];               // Buffer to hold formatted temperature string
    // Format current temperature with one decimal place
    int tempVal = (int)(tempCurrent * 10); // Scale to integer for decimal precision
    snprintf(tempText, sizeof(tempText), "Temp: %d.%dC", tempVal / 10, tempVal % 10);
    display.DisplayStringAt(0, 45, (uint8_t *)tempText, CENTER_MODE); // Show centered at y=45

    // Format threshold temperature
    tempVal = (int)(tempLimit * 10);
    snprintf(tempText, sizeof(tempText), "Limit: %d.%dC", tempVal / 10, tempVal % 10);
    display.DisplayStringAt(0, 85, (uint8_t *)tempText, CENTER_MODE); // Show centered at y=85
}

// Monitor temp and control fan
void monitorFan() {
    // Check if current temp exceeds threshold
    if (tempLimit < tempCurrent) {
        startFan();               // Start fan if temp is too high
    } else {
        stopFan();                // Stop fan if temp is below threshold
    }
}

// Main program
int main() {
    TS_StateTypeDef touchState;    // Structure to store touchscreen state (X, Y, touch detected)
    uint16_t touchRawX, touchRawY; // Variables for raw touch coordinates

    fanMotor.period_us(256);       // Set PWM period to 256 µs (frequency ~3.9 kHz)
    fanMotor.pulsewidth_us(0);     // Start with fan off (0 µs pulse width)
    display.SetFont(&Font24);      // Set default font size
    display.Clear(LCD_COLOR_WHITE); // Clear screen to white at startup
    display.SetTextColor(LCD_COLOR_DARKBLUE); // Set default text color

    checkTemperature();            // Get initial temperature reading
    tempLimit = ceil(tempCurrent) + 1.0f; // Set initial threshold 1°C above current temp, rounded up

    tempMonitorTicker.attach(&monitorFan, 1.0); // Check temp every 1 second

    renderButtons();               // Draw buttons at startup
    touchCooldown.start();         // Start debounce timer

    while (true) {                 // Infinite loop to run continuously
        touchScreen.GetState(&touchState); // Update touchscreen state
        // Check if touch is detected and debounce time (500ms) has passed
        if (touchState.TouchDetected && touchCooldown.read_ms() > 500) {
            touchCooldown.reset(); // Reset timer for next debounce
            touchRawX = touchState.X; // Get raw X coordinate
            touchRawY = touchState.Y; // Get raw Y coordinate

            // Check for plus button press (incorrect coordinates from your original)
            if (touchRawX < 120 && touchRawY < 160) { // Checks top-left, not matching button region
                plusTouched = true;    // Flag plus button as pressed
                tempLimit += 0.5;      // Increase threshold by 0.5°C
            }
            // Check for minus button press (incorrect coordinates)
            else if (touchRawX > 120 && touchRawY < 160) { // Checks top-right, wrong Y range
                minusTouched = true;   // Flag minus button as pressed
                tempLimit -= 0.5;      // Decrease threshold by 0.5°C
            }
        }
        checkTemperature();        // Update temperature display every loop
        renderButtons();           // Redraw buttons every loop (handles flashing if pressed)
    }
}