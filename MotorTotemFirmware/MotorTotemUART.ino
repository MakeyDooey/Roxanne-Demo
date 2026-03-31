#include <TMCStepper.h> //Must Install this Arduino Library

// --- UART SETUP ---
#define UART_RX 16 
#define UART_TX 17 
#define SERIAL_PORT Serial1
#define R_SENSE 0.11f // Standard sense resistor for BigTreeTech TMC2209

// Initialize the 3 drivers with their MS1/MS2 hardware addresses
TMC2209Stepper driver0(&SERIAL_PORT, R_SENSE, 0b00); 
TMC2209Stepper driver1(&SERIAL_PORT, R_SENSE, 0b01); 
TMC2209Stepper driver2(&SERIAL_PORT, R_SENSE, 0b10); 
TMC2209Stepper* drivers[] = {&driver0, &driver1, &driver2};

// --- PIN DEFINITIONS ---
const int EN_PINS[]   = {25, 32, 15};
const int STEP_PINS[] = {26, 33,  2}; 
const int DIR_PINS[]  = {27, 12,  4};
const int DIAG_PINS[] = {14, 13,  5};

// --- MOVEMENT VARIABLES ---
int activeMotor = -1;
unsigned long lastStep = 0;
const unsigned long stepDelay = 800; // Microseconds between steps (Speed)
String cmdBuffer = "";

void setup() {
  // 1. Start Serial Communications
  Serial.begin(115200);   // To PC / HTML Frontend
  SERIAL_PORT.begin(115200, SERIAL_8N1, UART_RX, UART_TX); // To TMC Drivers
  delay(1000); // Give drivers a second to boot

  Serial.println("INITIALIZING SYSTEM...");

  // 2. Setup Pins and Configure Drivers via UART
  for (int i = 0; i < 3; i++) {
    pinMode(EN_PINS[i], OUTPUT);
    pinMode(STEP_PINS[i], OUTPUT);
    pinMode(DIR_PINS[i], OUTPUT);
    pinMode(DIAG_PINS[i], INPUT_PULLDOWN); // DIAG pins go HIGH when stalled

    digitalWrite(EN_PINS[i], LOW); // LOW enables the driver

    // UART Configuration
    drivers[i]->begin();
    drivers[i]->toff(5);                 // Enable software power
    drivers[i]->rms_current(800);        // Set motor current (mA)
    drivers[i]->en_SpreadCycle(false);   // Enable StealthChop (Silent mode)
    drivers[i]->pwm_autoscale(true);     // Required for StealthChop
    
    // StallGuard4 Setup (Sensorless Homing)
    drivers[i]->TCOOLTHRS(0xFFFFF);      // Must be high to enable StallGuard
    drivers[i]->SGTHRS(100);             // Sensitivity! (0 to 255). ***YOU MUST TUNE THIS***
  }

  // Set Microsteps to match your friend's HTML frontend
  driver0.microsteps(8);  
  driver1.microsteps(32); 
  driver2.microsteps(32); 

  // 3. Run Sensorless Calibration
  calibrateMotors();

  Serial.println("READY");
}

void loop() {
  // 1. Listen for HTML Frontend Commands
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdBuffer.length() > 0) {
        handleCommand(cmdBuffer);
        cmdBuffer = "";
      }
    } else {
      cmdBuffer += c;
    }
  }

  // 2. Execute Motor Movement (if a motor is active)
  if (activeMotor != -1) {
    unsigned long now = micros();
    if (now - lastStep >= stepDelay) {
      digitalWrite(STEP_PINS[activeMotor], HIGH);
      delayMicroseconds(5); // Brief pulse
      digitalWrite(STEP_PINS[activeMotor], LOW);
      lastStep = now;
    }
  }
}

// --- COMMAND PARSER ---
void handleCommand(String cmd) {
  cmd.trim();
  
  if (cmd.startsWith("START:")) {
    // Expected format: START:1:CW
    int firstColon = cmd.indexOf(':');
    int secondColon = cmd.lastIndexOf(':');
    
    int id = cmd.substring(firstColon + 1, secondColon).toInt();
    String dirStr = cmd.substring(secondColon + 1);
    
    if (id >= 0 && id < 3) {
      activeMotor = id;
      digitalWrite(DIR_PINS[id], (dirStr == "CW") ? HIGH : LOW);
      
      Serial.print("OK motor="); Serial.print(id);
      Serial.print(" dir="); Serial.println(dirStr);
    }
  } 
  else if (cmd == "STOP") {
    activeMotor = -1;
    Serial.println("OK:STOP");
  }
}

// --- SENSORLESS HOMING ROUTINE ---
void calibrateMotors() {
  Serial.println("STARTING SENSORLESS CALIBRATION...");

  for (int i = 0; i < 3; i++) {
    Serial.print("Homing Motor "); Serial.print(i); Serial.print("... ");
    
    // Set direction toward the mechanical block (change to HIGH if it goes the wrong way)
    digitalWrite(DIR_PINS[i], LOW); 

    // Move motor until the DIAG pin goes HIGH (Stall detected)
    while (digitalRead(DIAG_PINS[i]) == LOW) {
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000); // Move slower during calibration
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }
    
    Serial.println(" HOMED!");
    
    // Optional: Back away from the block slightly after homing
    digitalWrite(DIR_PINS[i], HIGH);
    for(int b = 0; b < 200; b++){
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000);
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }
  }
  Serial.println("CALIBRATION COMPLETE.");
}