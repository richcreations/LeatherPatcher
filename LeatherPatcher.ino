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

   CONFIGURATION:
   All tunable settings are in the USER CONFIGURATION section below.
   To change a setting:
     1. Open this sketch in Arduino IDE.
     2. Scroll down to the section marked "=== USER CONFIGURATION ===".
     3. Edit the number or true/false value.
     4. Click the ✔ Verify button to check.
     5. Click → Upload to send it to the Arduino Nano.

   LED STATUS (on Nano’s built-in LED pin 13):
     - Startup/arming: slow blink (1 Hz).
     - Armed but stopped: double blink every second.
     - Running (pedal pressed): LED solid ON.
     - Disabled by pedal in deadband: LED OFF.
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

// --- Analog input handling ---
#define ANALOG_REVERSED        false  // true = 5V = stop, false = 0V = stop
#define ANALOG_DEADBAND_COUNTS 20     // deadband around 0 input (ADC counts, ~100 mV)
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

const int ANALOG_MAX = 1023;
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

// Normalize raw ADC (0-1023) to 0.0-1.0, with reversal if needed
float normalizeADC(int raw) {
  float u = (float)raw / (float)ANALOG_MAX;
  if (ANALOG_REVERSED) u = 1.0 - u;
  if (u < 0.0) u = 0.0;
  if (u > 1.0) u = 1.0;
  return u;
}

// Apply deadband to normalized input
float applyDeadband(float u) {
  if (u * ANALOG_MAX <= ANALOG_DEADBAND_COUNTS) return 0.0;
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
void updateLED(unsigned long now, bool running, bool inDeadband) {
  if (!armed) {
    // Startup/arming: slow blink 1 Hz
    if (now - lastLED >= 500) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastLED = now;
    }
  } else if (running) {
    // Running: solid ON
    digitalWrite(LED_PIN, HIGH);
  } else if (inDeadband) {
    // Armed but pedal at stop: double blink
    unsigned long t = now % 1000;
    if ((t < 100) || (t >= 200 && t < 300)) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  } else {
    // Disabled by pedal in deadband: LED OFF
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

  // --- Read analog input (raw for safety, filtered for speed) ---
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
    updateLED(now, false, false);
    return;
  }

  bool inDeadband = false;
  bool running = false;

  // --- Control loop at CONTROL_HZ ---
  if (now - lastControlUpdate >= controlInterval) {
    lastControlUpdate = now;

    float u = normalizeADC(filteredADC);
    u = applyDeadband(u);

    if (u == 0.0) {
      // Inside deadband
      inDeadband = true;
      if (disableStart == 0) {
        disableStart = now;
      } else if (now - disableStart >= DISABLE_DELAY_MS) {
        digitalWrite(ENABLE_PIN, HIGH); // disable driver
        stepper.stop();
      }
    } else {
      // Outside deadband
      disableStart = 0;
      digitalWrite(ENABLE_PIN, LOW); // enable driver
      running = true;

      float curved = applyCurve(u);
      float targetSpeed = curved * MAX_SPEED_SPS;

      stepper.setMaxSpeed(MAX_SPEED_SPS);
      stepper.setAcceleration(MAX_ACCEL_SPS2);

      // Virtual position trick for velocity control
      static double virtualPos = 0;
      virtualPos += targetSpeed / CONTROL_HZ;
      stepper.moveTo((long)virtualPos);
    }
  }

  // Always run the stepper for pulse generation
  stepper.run();

  // Update LED status
  updateLED(now, running, inDeadband);
}
