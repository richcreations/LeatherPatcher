# LeatherPatcher - Stepper Motor Controller

**Author:** Richard Simpson  
**Board:** Arduino Nano (ATmega328P)  
**Driver:** DRV8825 (full-step mode)  
**Motor:** Generic 1.8° NEMA 17  

LeatherPatcher is a controller for leather sewing machines driven by a stepper motor.  
It uses an Arduino Nano, a DRV8825 driver, and a 0–5 V analog pedal input.  
The controller allows smooth, precise low-speed operation for leatherwork, while still supporting higher speeds when needed.  

---

## Features

- **Safety-first startup**: motor always disabled on power-up until pedal is held at stop for ≥0.5 seconds.  
- **Three pedal zones**:  
  - Stop zone = motor disabled  
  - Hold zone = motor enabled, speed 0 (needle locked)  
  - Throttle zone = sewing speed proportional to pedal  
- **Logarithmic speed curve (default)**: smooth, precise control at low speeds.  
- **Full-step mode only**: DRV8825 M0/M1/M2 tied low.  
- **Configurable parameters**: all tuning done with `#define`s in a single config section.  
- **LED status feedback**: built-in LED (pin 13) shows system state.  
- **AccelStepper library**: handles acceleration and step timing.  
- **RunningAverage library**: filters analog input for stable control.  

---

## Getting Started (Arduino IDE)

Follow these steps to build and upload LeatherPatcher:

1. **Install Arduino IDE**  
   Download from [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software) and install it.  

2. **Open the project**  
   - Place the `LeatherPatcher.ino`, `README.md`, and `LICENSE.md` files in a folder named `LeatherPatcher`.  
   - Double-click `LeatherPatcher.ino` to open it in Arduino IDE.  

3. **Select your board and port**  
   - In Arduino IDE, go to **Tools → Board** and select **Arduino Nano**.  
   - Go to **Tools → Processor** and pick **ATmega328P (Old Bootloader)** if a normal Nano doesn’t upload.  
   - Under **Tools → Port**, select the COM port your Nano is connected to.  

4. **Install required libraries**  
   In Arduino IDE, go to **Sketch → Include Library → Manage Libraries…**  
   - Search for **AccelStepper** (by Mike McCauley) and install it.  
   - Search for **RunningAverage** (by Rob Tillaart) and install it.  

5. **Upload to Arduino**  
   - Click the ✔ Verify button to check your changes.  
   - Click the → Upload button to send it to the Nano.  
   - When done, the built-in LED will show status according to the pedal zones.  

---

## Safety Rules (Non-Configurable)

These rules are hard-coded and cannot be turned off:

- Motor always starts **disabled** at power-on.  
- Pedal must be at stop for ≥0.5 seconds to arm.  
- Minimum enforced arming time: 250 ms.  
- Motor disables when pedal held at stop for `DISABLE_DELAY_MS` (default 100 ms).  

---

## Pinout

- **STEP** → D5  
- **DIR** → D2  
- **ENABLE** → D8  
- **ANALOG pedal input** → A7  
- **Status LED** → Built-in pin 13  

DRV8825 must be wired for **full-step mode** (M0, M1, M2 = LOW).  

---

## LED Status Codes

- **Startup/arming:** slow blink (1 Hz) — waiting for pedal at stop.  
- **Armed, pedal at stop:** double blink every second.  
- **Hold zone (needle locked):** fast blink (2 Hz).  
- **Running (pedal pressed, sewing):** LED solid ON.  
- **Disabled (driver cut):** LED OFF.  

---

## Pedal Zones

The pedal is divided into three regions. You can adjust their widths  
with `ANALOG_DEADBAND_COUNTS`, `HOLD_BAND_COUNTS`, `ANALOG_MIN_COUNTS`,  
and `ANALOG_MAX_COUNTS`.

| Pedal Position (percent) | Zone          | Behavior                                   | LED Feedback        |
|---------------------------|---------------|--------------------------------------------|---------------------|
| 0% → Deadband             | **Stop**      | Motor disabled, no torque. Used for arming. | Double blink (armed) |
| Slightly pressed          | **Hold**      | Motor enabled, speed=0. Needle held firm.   | Fast blink (2 Hz)    |
| Further pressed           | **Throttle**  | Pedal controls sewing speed.                | Solid ON             |

- The **Stop** zone ensures safety when the pedal is all the way up.  
- The **Hold** zone keeps the needle down so you can rotate fabric safely.  
- The **Throttle** zone runs the motor, speed proportional to pedal.  

---

## Tuning Guide

Here are the most common adjustments you might want to make.  
Open **LeatherPatcher.ino** in the Arduino IDE, scroll to the  
`=== USER CONFIGURATION ===` section, and change the numbers there.  
After editing, click ✔ Verify, then Upload.  

---

### If the machine creeps when your foot is off the pedal:
Increase **`ANALOG_DEADBAND_COUNTS`**.  
This widens the "off" zone so tiny pedal voltage drift doesn’t start the motor.  
Try values around 30–50 if you see unwanted creeping.

---

### If the needle does not stay locked down when you let up slightly:
Adjust **`HOLD_BAND_COUNTS`**.  
This controls how wide the “hold zone” is (pedal down just a little).  
Increase it if it’s hard to find the hold spot, decrease if the machine  
holds too soon.

---

### If you can’t reach full speed even with pedal floored:
Lower **`ANALOG_MAX_COUNTS`**.  
This tells the system what “full pedal down” looks like.  
If your pedal only reaches ~4.8V, the ADC may never hit 1023.  
Setting this closer to your actual max (e.g. 980) ensures you get  
full motor speed at the end of the pedal.

---

### If the machine moves when the pedal is fully up:
Raise **`ANALOG_MIN_COUNTS`**.  
This trims out the resting voltage of the pedal.  
Example: If rest value is 15 instead of 0, set `ANALOG_MIN_COUNTS 15`.  
That way, pedal-up always maps to zero.

---

### If the pedal feels too jumpy at low speeds:
Increase **`LOG_CURVE_K`**.  
This makes the response curve more gentle near zero.  
Start around 20 (default), try 30–40 for extra fine control.

---

### If the pedal feels too sluggish to respond:
Decrease **`LOG_CURVE_K`**, or switch **`INPUT_CURVE`** to `1` (quadratic).  
This makes speed rise faster with small pedal movement.  
Good if you want a snappier response.

---

### If the machine does not stop quickly enough:
Lower **`DISABLE_DELAY_MS`**.  
Default is 100 ms (1/10 second).  
Dropping it to 50 makes the stop snappier.  
Raising it makes the stop softer but can feel unresponsive.

---

### If the machine feels too slow even with pedal floored:
Raise **`MAX_SPEED_SPS`**.  
This increases the top motor speed.  
Go up gradually — too high and the motor may stall.  
Tip: If it stalls, drop the number back down.

---

### If speed ramps too suddenly or stalls when accelerating:
Lower **`MAX_ACCEL_SPS2`**.  
This controls how fast the motor changes speed.  
High values = quick response but risk stalls.  
Lower values = smoother, more forgiving.

---

### If response is jittery when holding a steady pedal:
Increase **`ANALOG_FILTER_SAMPLES`**.  
This averages more readings for smoother input.  
Default 8 is good. Try 12 or 16 if your pedal is noisy.  
But don’t go too high, or the response will feel “laggy.”  

---

## Notes

- Always set the **DRV8825 current limit** correctly for your NEMA 17 motor.  
- 24 V supply recommended for higher speeds and torque stability.  
- If more flywheel RPM is needed, consider a **larger pulley** on the motor to reduce the ~17.5:1 ratio.  
- This controller assumes **single-direction operation**.  
- **Important safety note:** Always test the system with the motor **not connected to the sewing machine belt** first.  
  Confirm that the pedal zones, LED feedback, and motor behavior are correct before attaching the belt to the flywheel.  
  This avoids unexpected motion of the machine during initial setup and tuning.  

---

## License

This project is licensed under the  
[Creative Commons Attribution-ShareAlike 4.0 International License (CC BY-SA 4.0)](https://creativecommons.org/licenses/by-sa/4.0/).  

© Richard Simpson. You are free to share and adapt this work, even commercially, under the terms of the license. Attribution and ShareAlike are required.
