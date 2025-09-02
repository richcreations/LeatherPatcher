/* =======================================================
   LeatherPatcher - Stepper Motor Controller
   Author: Richard Simpson

   License: Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
   https://creativecommons.org/licenses/by-sa/4.0/

   You are free to share and adapt this code, even commercially,
   as long as you give appropriate credit and license your 
   derivative works under the same terms. No warranties are given.
   ======================================================= */

/* =======================================================
   LEATHERPATCHER STEPPER CONTROLLER
   For Arduino Nano + DRV8825 + NEMA 17

   REQUIREMENTS:
   - Install "AccelStepper" library (by Mike McCauley)
   - Install "RunningAverage" library (by Rob Tillaart)
   Use Arduino IDE: Tools → Manage Libraries

   SAFETY:
   - On power-up, motor is always DISABLED.
   - Motor will not “arm” until the pedal is held fully at STOP 
     for at least 0.5 seconds (built-in safety rule, cannot be disabled).
   - When the pedal is released back to STOP, the motor DISABLES 
     after a short delay (default 100 ms).
   - These safety rules cannot be turned off.
   - **Important safety note:** Always test the system with the motor 
     NOT connected to the sewing machine belt first. Confirm that the 
     pedal zones, LED feedback, and motor behavior are correct before 
     attaching the belt to the flywheel. This avoids unexpected motion 
     of the machine during initial setup and tuning.

   HOW TO CHANGE SETTINGS:
   1. Open this file in the Arduino IDE.
   2. Scroll down to the section marked "=== USER CONFIGURATION ===".
   3. Each setting is explained in comments next to it.
   4. To change: replace the number (or true/false) with a new value.
   5. Example: To flip motor direction, change:
        #define REVERSE_DIRECTION false
      into:
        #define REVERSE_DIRECTION true
   6. After making changes, click the ✔ Verify button in the IDE to check.
   7. Then click → Upload to send to the Arduino Nano.

   KEY SETTINGS:
   - Pins: STEP_PIN, DIR_PIN, ENABLE_PIN, ANALOG_PIN
   - Max speed & acceleration: MAX_SPEED_SPS, MAX_ACCEL_SPS2
   - Pedal calibration: ANALOG_MIN_COUNTS, ANALOG_MAX_COUNTS
   - Deadband and hold band: ANALOG_DEADBAND_COUNTS, HOLD_BAND_COUNTS
   - Input curve: INPUT_CURVE and LOG_CURVE_K
   - Safety timing: ARM_DELAY_MS and DISABLE_DELAY_MS

   LED STATUS (on Nano’s built-in LED pin 13):
     - Startup/arming: slow blink (1 Hz).
     - Armed but STOP (pedal up): double blink every second.
     - Hold band (needle locked): fast blink (2 Hz).
     - Running (pedal pressed, sewing): solid ON.
     - Disabled (driver cut): LED OFF.
   ======================================================= */



// === USER CONFIGURATION ===

// --- Pin mapping ---
#define STEP_PIN        5
#define DIR_PIN         2
#define ENABLE_PIN      8
#define ANALOG_PIN      A7

// --- Motor behavior ---
#define REVERSE_DIRECTION   false   // flip if motor runs opposite
#define DISABLE_AT_ZERO     true    // enforced in logic

// --- Motion tuning ---
#define MAX_SPEED_SPS       4000    // max step rate at 5V input (steps per second)
#define MAX_ACCEL_SPS2      800     // max acceleration (steps per second^2)

// --- Pedal calibration ---
#define ANALOG_MIN_COUNTS   10     // ADC value for pedal fully at rest
#define ANALOG_MAX_COUNTS   1010   // ADC value for pedal fully pressed

// --- Analog input handling ---
#define ANALOG_REVERSED        false  // true = 5V = stop, false = 0V = stop
#define ANALOG_DEADBAND_COUNTS 20     // zone below this disables driver
#define HOLD_BAND_COUNTS       60     // zone above deadband but below this = hold needle
#define ANALOG_FILTER_SAMPLES  8      // running average window size (samples)

// --- Control loop ---
#define CONTROL_HZ           100    // update rate for analog-to-speed mapping (~10 ms)

// --- Input curve ---
#define INPUT_CURVE          2      // 0 = linear, 1 = quadratic, 2 = logarithmic (default)
#define LOG_CURVE_K          20.0   // steepness of log curve (larger = more gentle near zero)

// --- Safety timing ---
#define ARM_DELAY_MS         500    // must hold pedal at stop this long on startup (min enforced = 250 ms)
#define DISABLE_DELAY_MS     100    // must hold pedal at stop this long before disabling during use



// =======================================================
//  Includes and Globals
// =======================================================
#include <AccelStepper.h>
#include <RunningAverage.h>

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
RunningAverage filter(ANALOG_FILTER_SAMPLES);

const int LED_PIN = 13;  // built-in Nano LED

// Safety state
bool armed = false;
unsigned long armStart = 0;
unsigned long disableStart = 0;

// Control timing
unsigned long lastControlUpdate = 0;
const unsigned long controlInterval = 1000UL / CONTROL_HZ;

// LED timing
unsigned long lastLED = 0;
bool ledState = false;


// =======================================================
//  Helper Functions
// =======================================================

// Normalize raw ADC to 0.0–1.0 with calibration + reversal
float normalizeADC(int raw) {
  if (raw < ANALOG_MIN_COUNTS) raw = ANALOG_MIN_COUNTS;
  if (raw > ANALOG_MAX_COUNTS) raw = ANALOG_MAX_COUNTS;

  float u = (float)(raw - ANALOG_MIN_COUNTS) / (float)(ANALOG_MAX_COUNTS - ANALOG_MIN_COUNTS);

  if (ANALOG_REVERSED) u = 1.0 - u;
  if (u < 0.0) u = 0.0;
  if (u > 1.0) u = 1.0;
  return u;
}

// Apply input curve
float applyCurve(float u) {
  switch (INPUT_CURVE) {
    case 1: // quadratic
      return u * u;
    case 2: { // logarithmic
      float k = LOG_CURVE_K;
      return log(1.0 + k * u) / log(1.0 + k);
    }
    default: // linear
      return u;
  }
}

// LED status indication
void updateLED(unsigned long now, bool running, bool inHold, bool inDeadband) {
  if (!armed) {
    // Startup/arming: slow blink 1 Hz
    if (now - lastLED >= 500) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastLED = now;
    }
  } else if (running) {
    digitalWrite(LED_PIN, HIGH); // solid on
  } else if (inHold) {
    // Hold band: blink 2 Hz
    if (now - lastLED >= 250) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastLED = now;
    }
  } else if (inDeadband) {
    // Armed but pedal at stop: double blink
    unsigned long t = now % 1000;
    if ((t < 100) || (t >= 200 && t < 300)) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}


// =======================================================
//  Setup
// =======================================================
void setup() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Set direction once at startup
  digitalWrite(DIR_PIN, REVERSE_DIRECTION ? HIGH : LOW);

  // Keep driver disabled initially
  digitalWrite(ENABLE_PIN, HIGH);

  stepper.setMaxSpeed(MAX_SPEED_SPS);
  stepper.setAcceleration(MAX_ACCEL_SPS2);

  // Allow analog reference and DRV8825 to settle
  delay(250);

  // Initialize filter
  filter.clear();
  for (int i = 0; i < ANALOG_FILTER_SAMPLES; i++) {
    filter.addValue(analogRead(ANALOG_PIN));
  }
}


// =======================================================
//  Loop
// =======================================================
void loop() {
  unsigned long now = millis();

  // --- Read analog input (raw for safety, filtered for control) ---
  int rawADC = analogRead(ANALOG_PIN);
  filter.addValue(rawADC);
  int filteredADC = (int)filter.getAverage();

  // --- Safety: arming at startup ---
  if (!armed) {
    if (rawADC <= ANALOG_DEADBAND_COUNTS) {
      if (armStart == 0) {
        armStart = now;
      } else if (now - armStart >= max((int)ARM_DELAY_MS, 250)) {
        armed = true; // armed after deadband hold
      }
    } else {
      armStart = 0; // reset timer if pedal not in deadband
    }
    stepper.stop(); // ensure no movement
    digitalWrite(ENABLE_PIN, HIGH);
    updateLED(now, false, false, false);
    return;
  }

  bool inDeadband = false;
  bool inHold = false;
  bool running = false;

  // --- Control loop at CONTROL_HZ ---
  if (now - lastControlUpdate >= controlInterval) {
    lastControlUpdate = now;

    float u = normalizeADC(filteredADC);
    int scaledADC = (int)(u * 1023);

    if (scaledADC <= ANALOG_DEADBAND_COUNTS) {
      // Zone 1: Zero Deadband → disable driver
      inDeadband = true;
      if (disableStart == 0) {
        disableStart = now;
      } else if (now - disableStart >= DISABLE_DELAY_MS) {
        digitalWrite(ENABLE_PIN, HIGH); // disable driver
        stepper.stop();
      }
    } 
    else if (scaledADC <= HOLD_BAND_COUNTS) {
      // Zone 2: Hold Band → enable driver, hold position
      inHold = true;
      disableStart = 0;
      digitalWrite(ENABLE_PIN, LOW);
      stepper.moveTo(stepper.currentPosition()); // lock position
    } 
    else {
      // Zone 3: Throttle
      disableStart = 0;
      digitalWrite(ENABLE_PIN, LOW); 
      running = true;

      float curved = applyCurve(u);
      float targetSpeed = curved * MAX_SPEED_SPS;

      stepper.setMaxSpeed(MAX_SPEED_SPS);
      stepper.setAcceleration(MAX_ACCEL_SPS2);

      static double virtualPos = 0;
      virtualPos += targetSpeed / CONTROL_HZ;
      stepper.moveTo((long)virtualPos);
    }
  }

  // Always run the stepper for pulse generation
  stepper.run();

  // Update LED status
  updateLED(now, running, inHold, inDeadband);
}
