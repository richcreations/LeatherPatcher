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
- **Deadband with disable**: pedal near stop disables the motor after a short delay.  
- **Logarithmic speed curve (default)**: smooth, precise control at low speeds.  
- **Full-step mode only**: DRV8825 M0/M1/M2 tied low.  
- **Configurable parameters**: all tuning done with `#define`s in a single config section.  
- **LED status feedback**: built-in LED (pin 13) shows system state.  
- **AccelStepper library**: handles acceleration and step timing.  
- **RunningAverage library**: filters analog input for stable control.  

---

## Safety Rules (Non-Configurable)

These rules are hard-coded and cannot be turned off:

- Motor always starts **disabled** at power-on.  
- Pedal must be at stop for ≥0.5 seconds to arm.  
- Minimum enforced arming time: 250 ms.  
- Motor disables when pedal held at stop for `DISABLE_DELAY_MS` (default 100 ms).  

---

## Required Libraries

Install these in Arduino IDE using **Sketch → Include Library → Manage Libraries…**

- **AccelStepper** (by Mike McCauley)  
- **RunningAverage** (by Rob Tillaart)  

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
- **Running (pedal pressed):** LED solid ON.  
- **Disabled by pedal deadband:** LED OFF.  

---

## Configuration

All tunables are at the top of the sketch under **USER CONFIGURATION**.  
Key options:  

- **Pins**: `STEP_PIN`, `DIR_PIN`, `ENABLE_PIN`, `ANALOG_PIN`.  
- **Direction**: `REVERSE_DIRECTION` flips rotation in software.  
- **Max speed**: `MAX_SPEED_SPS` (steps per second at full pedal).  
- **Acceleration**: `MAX_ACCEL_SPS2` (steps/s²).  
- **Analog handling**:  
  - `ANALOG_REVERSED` (invert pedal sense).  
  - `ANALOG_DEADBAND_COUNTS` (stop zone size).  
  - `ANALOG_FILTER_SAMPLES` (running average window size).  
- **Control loop**: `CONTROL_HZ` (pedal check frequency).  
- **Input curve**:  
  - `INPUT_CURVE = 0` (linear), `1` (quadratic), `2` (logarithmic default).  
  - `LOG_CURVE_K` tunes the log curve strength.  
- **Safety timing**:  
  - `ARM_DELAY_MS` (must hold pedal at stop on startup; min 250 ms enforced).  
  - `DISABLE_DELAY_MS` (delay before disabling at stop during use).  

---

## Tuning Guide

Use these steps to adjust machine feel:

1. **Top speed (`MAX_SPEED_SPS`)**  
   - Start ~4000 (≈1200 RPM motor, ≈69 RPM flywheel with 16T pulley + 7″ wheel).  
   - Increase if too slow, decrease if motor stalls at high end.  

2. **Acceleration (`MAX_ACCEL_SPS2`)**  
   - Start ~800.  
   - Increase for snappier response, decrease for smoother operation with heavy flywheel.  

3. **Pedal deadband (`ANALOG_DEADBAND_COUNTS`)**  
   - Default 20 (≈100 mV).  
   - Increase if machine creeps when foot is off pedal.  
   - Decrease if you want quicker response near stop.  

4. **Pedal filtering (`ANALOG_FILTER_SAMPLES`)**  
   - Default 8.  
   - Increase for smoother input (slower to react).  
   - Decrease for faster input response (may jitter).  

5. **Input curve (`INPUT_CURVE` and `LOG_CURVE_K`)**  
   - `INPUT_CURVE = 2` (logarithmic) gives best control at low speeds.  
   - Adjust `LOG_CURVE_K` higher (e.g. 30) for gentler response near stop, or lower (e.g. 10) for quicker response.  

6. **Disable delay (`DISABLE_DELAY_MS`)**  
   - Default 100 ms.  
   - Increase if you feel frequent unwanted torque cut-offs when hovering near stop.  
   - Decrease for faster disengage when pedal released.  

7. **Arming delay (`ARM_DELAY_MS`)**  
   - Default 500 ms.  
   - Longer delay = more deliberate sta
