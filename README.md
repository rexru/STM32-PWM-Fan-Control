# 🌬️ STM32F429ZI – PWM Fan Control with Touchscreen & Temperature Monitoring  

## 📖 About  
This project implements a **temperature-controlled fan system** using the STM32F429ZI Discovery board. An **LM35 temperature sensor** provides live readings, which are displayed on the onboard LCD alongside a user-set temperature threshold. If the measured temperature exceeds the threshold, a **PWM-driven fan** gradually ramps up in speed until the system cools down.  

The threshold can be adjusted interactively through the **touchscreen interface**, using on-screen “plus” and “minus” buttons. The design showcases **PWM motor control, touchscreen input, and real-time temperature monitoring** on an STM32 platform.  

---

## ✨ Features  
- **Temperature Monitoring**  
  - Reads live temperature from an LM35 sensor via ADC input.  
  - Displays both current temperature and adjustable threshold on the LCD.  

- **Fan Control**  
  - Fan speed controlled via PWM signal on pin `PA_6`.  
  - Smooth ramp-up effect when cooling is required.  
  - Automatic shutdown when temperature falls below the threshold.  

- **Touchscreen Interface**  
  - On-screen “+” and “–” buttons to adjust the threshold (±0.5 °C steps).  
  - Visual button feedback with color flashing when pressed.  

- **Interrupt-Driven Tasks**  
  - `Ticker` for periodic fan ramp-up.  
  - `Ticker` for periodic temperature checks.  
  - `Timer` for touchscreen debounce (prevents accidental rapid presses).  

---

## 🛠️ Hardware  
- **Microcontroller**: STM32F429ZI Discovery Board  
- **Sensors**: LM35 analog temperature sensor (connected to pin `PA_0`)  
- **Actuator**: PWM-controlled fan (driven from pin `PA_6`)  
- **Display & Input**:  
  - 2.41" QVGA LCD (built-in)  
  - Resistive touchscreen (built-in)  

---

## ⚙️ System Behavior  
1. At startup, the fan is **off** and the system sets the threshold to **current temperature + 1 °C**.  
2. The LCD shows:  
   - **Current temperature** (°C)  
   - **Threshold temperature** (°C)  
3. If the temperature exceeds the threshold:  
   - Fan starts and ramps up gradually until maximum PWM duty cycle is reached.  
4. If the temperature drops below the threshold:  
   - Fan shuts off.  
5. User can increase or decrease the threshold via touchscreen buttons.  

---

## ▶️ How to Run  
1. Connect the LM35 temperature sensor to `PA_0` (analog input).  
2. Connect the fan to `PA_6` (PWM output) with a suitable driver circuit.  
3. Flash the program onto the STM32F429ZI Discovery board.  
4. Power on the board — the LCD will display **temperature and threshold**.  
5. Use the touchscreen to raise or lower the fan activation threshold.  

---


