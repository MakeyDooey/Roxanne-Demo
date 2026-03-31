#include <TMCStepper.h> //Must Install this Arduino Library

// --- PIN DEFINITIONS ---
const int EN_PINS[]   = {38, 38, 38};
const int STEP_PINS[] = {21, 48, 36}; 
const int DIR_PINS[]  = {47, 35, 37};
const int DIAG_PINS[] = {1, 2,  7};

// --- MOTOR CONFIGS --- 
const int SENSE = 30; //Stall Guard Sensitivity
const int MICRO_STEPS[] = {8, 32, 32};
const unsigned long stepDelay = 10000; // Microseconds between steps (100 steps/sec) (SPEED CONTROLLED VIA UART COMMANDS, THIS IS JUST A DEFAULT)

// --- UART SETUP ---
#define UART_RX 15 
#define UART_TX 16
#define SERIAL_PORT Serial1
#define R_SENSE 0.11f // Standard sense resistor for BigTreeTech TMC2209

// Initialize the 3 drivers with their MS1/MS2 hardware addresses
TMC2209Stepper driver0(&SERIAL_PORT, R_SENSE, 0b00); 
TMC2209Stepper driver1(&SERIAL_PORT, R_SENSE, 0b01); 
TMC2209Stepper driver2(&SERIAL_PORT, R_SENSE, 0b10); 
TMC2209Stepper* drivers[] = {&driver0, &driver1, &driver2};

// --- MOVEMENT VARIABLES ---
bool activeMotors[3] = {false, false, false};
unsigned long lastStep[3] = {0, 0, 0};
String cmdBuffer = "";

void setup() {
  // 1. Start Serial Communications
  Serial.begin(115200);   // Debug output to USB
  SERIAL_PORT.begin(115200, SERIAL_8N1, UART_RX, UART_TX); // To TMC Drivers
  Serial1.begin(115200); // Commands from UART
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
    drivers[i]->en_spreadCycle(false);   // Enable StealthChop (Silent mode)
    drivers[i]->pwm_autoscale(true);     // Required for StealthChop
    
    // StallGuard4 Setup (Sensorless Homing)
    drivers[i]->TCOOLTHRS(0xFFFFF);      // Must be high to enable StallGuard
    drivers[i]->SGTHRS(SENSE);             // Sensitivity! (0 to 255). ***YOU MUST TUNE THIS***

    // Microstepping Setup
    drivers[i]->microsteps(MICRO_STEPS[i]);  
  }

  // 3. Run Sensorless Calibration
  calibrateMotors();

  Serial.println("READY");
}

void loop() {
  // 1. Listen for Commands from Serial1
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    if (c == '\n' || c == '\r') {
      if (cmdBuffer.length() > 0) {
        handleCommand(cmdBuffer);
        cmdBuffer = "";
      }
    } else {
      cmdBuffer += c;
    }
  }

  // 2. Execute Motor Movement (if motors are active)
  unsigned long now = micros();
  for (int i = 0; i < 3; i++) {
    if (activeMotors[i]) {
      if (now - lastStep[i] >= stepDelay) {
        digitalWrite(STEP_PINS[i], HIGH);
        delayMicroseconds(5); // Brief pulse
        digitalWrite(STEP_PINS[i], LOW);
        lastStep[i] = now;
        Serial.print("Stepped motor "); Serial.println(i); // Debug
      }
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
      activeMotors[id] = true;
      digitalWrite(DIR_PINS[id], (dirStr == "CW") ? HIGH : LOW);
      
      Serial1.print("OK motor="); Serial1.print(id);
      Serial1.print(" dir="); Serial1.println(dirStr);
    }
  } 
  else if (cmd == "STOP") {
    for (int i = 0; i < 3; i++) activeMotors[i] = false;
    Serial1.println("OK:STOP");
  }
}

// --- SENSORLESS HOMING ROUTINE ---
void calibrateMotors() {
  Serial.println("STARTING SENSORLESS CALIBRATION...");

  // Create an array to remember the original microstep settings
  uint16_t originalMicrosteps[3];

  // 1. SAVE & OVERRIDE
  for (int i = 0; i < 3; i++) {
    // Read the current setting and save it
    originalMicrosteps[i] = drivers[i]->microsteps(); 
    
    // Force all to 1/8 for reliable back-EMF generation
    drivers[i]->microsteps(8); 
  }

  // --- CALIBRATION SEQUENCE ---
  for (int i = 0; i < 3; i++) {
    Serial.print("Homing Motor "); Serial.print(i); Serial.print("... ");
    
    digitalWrite(DIR_PINS[i], LOW); 

    // Flush any stuck flags
    drivers[i]->SG_RESULT(); 

    // Grace period: Take 64 blind steps to get up to speed
    for (int startup = 0; startup < 64; startup++) {
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000); 
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }

    // Now listen for the crash
    while (digitalRead(DIAG_PINS[i]) == LOW) {
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000); 
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }
    
    Serial.println(" HOMED!");
    
    // Back away from the block
    digitalWrite(DIR_PINS[i], HIGH);
    for(int b = 0; b < 200; b++){
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000);
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }

    delay(500); // Anti-crosstalk delay
  }

  // 2. RESTORE ORIGINAL SETTINGS
  Serial.println("Restoring original microstep settings...");
  for (int i = 0; i < 3; i++) {
    drivers[i]->microsteps(originalMicrosteps[i]); 
  }

  Serial.println("CALIBRATION COMPLETE.");
}