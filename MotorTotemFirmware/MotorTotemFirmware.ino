#include <TMCStepper.h>
#include <SoftwareSerial.h>

// Pin definitions
#define EN_PIN 38
#define STEP1_PIN 18
#define DIR1_PIN 17
#define STEP2_PIN 16
#define DIR2_PIN 13
#define STEP3_PIN 12
#define DIR3_PIN 11

#define R_SENSE 0.11f  // TMC2209 has 0.11 ohm sense resistor

// SoftwareSerial for single-wire UART on GPIO3 (ESP_RX)
SoftwareSerial TMC_SERIAL(3, 3);  // RX=3, TX=3

// TMC2209 drivers with addresses set by MS1/MS2
TMC2209Stepper driver1(&TMC_SERIAL, R_SENSE, 0);  // Address 0 (MS1=0, MS2=0)
TMC2209Stepper driver2(&TMC_SERIAL, R_SENSE, 1);  // Address 1 (MS1=1, MS2=0)
TMC2209Stepper driver3(&TMC_SERIAL, R_SENSE, 2);  // Address 2 (MS1=0, MS2=1)

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  while (!Serial);

  // Set pin modes
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP1_PIN, OUTPUT);
  pinMode(DIR1_PIN, OUTPUT);
  pinMode(STEP2_PIN, OUTPUT);
  pinMode(DIR2_PIN, OUTPUT);
  pinMode(STEP3_PIN, OUTPUT);
  pinMode(DIR3_PIN, OUTPUT);

  // Enable all drivers (EN low)
  digitalWrite(EN_PIN, LOW);

  // Initialize TMC serial
  TMC_SERIAL.begin(115200);

  // Initialize drivers
  driver1.begin();
  driver2.begin();
  driver3.begin();

  // Configure drivers (example settings, adjust as needed)
  for (auto& driver : {driver1, driver2, driver3}) {
    driver.toff(5);              // Turn off time
    driver.rms_current(600);     // RMS current in mA (adjust based on motor)
    driver.microsteps(16);       // 16 microsteps
    driver.TCOOLTHRS(0xFFFFF);   // CoolStep threshold
    driver.SGTHRS(0);            // StallGuard threshold
  }

  Serial.println("TMC2209 drivers initialized");
}

void loop() {
  // Example: Rotate motors forward and backward
  moveMotor(1, 200, true);   // Motor 1, 200 steps, forward
  delay(1000);
  moveMotor(1, 200, false);  // Motor 1, 200 steps, backward
  delay(1000);

  moveMotor(2, 200, true);
  delay(1000);
  moveMotor(2, 200, false);
  delay(1000);

  moveMotor(3, 200, true);
  delay(1000);
  moveMotor(3, 200, false);
  delay(1000);
}

// Function to move a motor
void moveMotor(int motor, int steps, bool direction) {
  int stepPin, dirPin;

  switch (motor) {
    case 1:
      stepPin = STEP1_PIN;
      dirPin = DIR1_PIN;
      break;
    case 2:
      stepPin = STEP2_PIN;
      dirPin = DIR2_PIN;
      break;
    case 3:
      stepPin = STEP3_PIN;
      dirPin = DIR3_PIN;
      break;
    default:
      return;
  }

  digitalWrite(dirPin, direction ? HIGH : LOW);

  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500);  // Adjust delay for speed
    digitalWrite(stepPin, LOW);
    delayMicroseconds(500);
  }
}