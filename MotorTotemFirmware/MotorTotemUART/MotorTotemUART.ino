#include <TMCStepper.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// --- PIN DEFINITIONS ---
const int EN_PINS[]   = {38, 38, 38};
const int STEP_PINS[] = {21, 48, 36}; 
const int DIR_PINS[]  = {47, 35, 37};
const int DIAG_PINS[] = {1, 2,  7};

// --- MOTOR CONFIGS --- 
const int SENSE = 30;
const int MICRO_STEPS[] = {8, 32, 32};
const unsigned long stepDelay = 10000;

// --- UART SETUP ---
#define UART_RX 15 
#define UART_TX 16
#define SERIAL_PORT Serial1
#define CMD_PORT    Serial
#define R_SENSE 0.11f

// --- WIFI/WEBSOCKET SETUP ---
const char* AP_SSID = "Roxanne";
const char* AP_PASS = "makeydooey";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Initialize drivers
TMC2209Stepper driver0(&SERIAL_PORT, R_SENSE, 0b00); 
TMC2209Stepper driver1(&SERIAL_PORT, R_SENSE, 0b01); 
TMC2209Stepper driver2(&SERIAL_PORT, R_SENSE, 0b10); 
TMC2209Stepper* drivers[] = {&driver0, &driver1, &driver2};

// --- MOVEMENT VARIABLES ---
bool activeMotors[3] = {false, false, false};
unsigned long lastStep[3] = {0, 0, 0};
String cmdBuffer = "";

// --- WEBSOCKET HANDLER ---
void handleCommand(String cmd);

void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    CMD_PORT.printf("WS client connected: %u\n", client->id());
    client->text("CONNECTED:Roxanne");
  } else if (type == WS_EVT_DISCONNECT) {
    CMD_PORT.printf("WS client disconnected: %u\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      String msg = "";
      for (size_t i = 0; i < len; i++) msg += (char)data[i];
      msg.trim();
      CMD_PORT.print("WS rx: "); CMD_PORT.println(msg);
      handleCommand(msg);
    }
  }
}

void setup() {
  CMD_PORT.begin(115200);
  SERIAL_PORT.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(1000);

  CMD_PORT.println("INITIALIZING SYSTEM...");

  for (int i = 0; i < 3; i++) {
    pinMode(EN_PINS[i], OUTPUT);
    pinMode(STEP_PINS[i], OUTPUT);
    pinMode(DIR_PINS[i], OUTPUT);
    pinMode(DIAG_PINS[i], INPUT_PULLDOWN);
    digitalWrite(EN_PINS[i], LOW);

    drivers[i]->begin();
    drivers[i]->toff(5);
    drivers[i]->rms_current(800);
    drivers[i]->en_spreadCycle(false);
    drivers[i]->pwm_autoscale(true);
    drivers[i]->TCOOLTHRS(0xFFFFF);
    drivers[i]->SGTHRS(SENSE);
    drivers[i]->microsteps(MICRO_STEPS[i]);  
  }

  calibrateMotors();

  // Start WiFi Access Point
  WiFi.softAP(AP_SSID, AP_PASS);
  CMD_PORT.print("AP IP: ");
  CMD_PORT.println(WiFi.softAPIP());

  // Start WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
  CMD_PORT.println("WebSocket server started on ws://192.168.4.1/ws");

  CMD_PORT.println("READY");
}

void loop() {
  // USB serial commands still work for debugging
  while (CMD_PORT.available() > 0) {
    char c = CMD_PORT.read();
    if (c == '\n' || c == '\r') {
      cmdBuffer.trim();
      if (cmdBuffer.length() > 0) {
        handleCommand(cmdBuffer);
        cmdBuffer = "";
      }
    } else {
      cmdBuffer += c;
    }
  }

  // WebSocket cleanup
  ws.cleanupClients();

  // Step active motors
  unsigned long now = micros();
  for (int i = 0; i < 3; i++) {
    if (activeMotors[i]) {
      if (now - lastStep[i] >= stepDelay) {
        digitalWrite(STEP_PINS[i], HIGH);
        delayMicroseconds(5);
        digitalWrite(STEP_PINS[i], LOW);
        lastStep[i] = now;
      }
    }
  }
}

// --- COMMAND PARSER ---
void handleCommand(String cmd) {
  cmd.trim();

  if (cmd.startsWith("START:")) {
    int firstColon = cmd.indexOf(':');
    int secondColon = cmd.indexOf(':', firstColon + 1);
    int id = cmd.substring(firstColon + 1, secondColon).toInt();
    String dirStr = cmd.substring(secondColon + 1);
    dirStr.trim();

    if (id >= 0 && id < 3) {
      activeMotors[id] = true;
      digitalWrite(DIR_PINS[id], (dirStr == "CW") ? HIGH : LOW);
      String resp = "OK motor=" + String(id) + " dir=" + dirStr;
      CMD_PORT.println(resp);
      ws.textAll(resp);
    } else {
      ws.textAll("ERR invalid motor id");
      CMD_PORT.println("ERR invalid motor id");
    }
  }
  else if (cmd == "STOP") {
    for (int i = 0; i < 3; i++) activeMotors[i] = false;
    CMD_PORT.println("OK:STOP");
    ws.textAll("OK:STOP");
  }
  else if (cmd == "STATUS") {
    for (int i = 0; i < 3; i++) {
      String s = "motor " + String(i) + " active=" + String(activeMotors[i]);
      CMD_PORT.println(s);
      ws.textAll(s);
    }
  }
  else {
    String err = "ERR unknown: " + cmd;
    CMD_PORT.println(err);
    ws.textAll(err);
  }
}

// --- SENSORLESS HOMING ROUTINE ---
void calibrateMotors() {
  CMD_PORT.println("STARTING SENSORLESS CALIBRATION...");
  uint16_t originalMicrosteps[3];

  for (int i = 0; i < 3; i++) {
    originalMicrosteps[i] = drivers[i]->microsteps(); 
    drivers[i]->microsteps(8); 
  }

  for (int i = 0; i < 3; i++) {
    CMD_PORT.print("Homing Motor "); CMD_PORT.print(i); CMD_PORT.print("... ");
    digitalWrite(DIR_PINS[i], LOW); 
    drivers[i]->SG_RESULT(); 

    for (int startup = 0; startup < 64; startup++) {
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000); 
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }

    while (digitalRead(DIAG_PINS[i]) == LOW) {
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000); 
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }

    CMD_PORT.println(" HOMED!");

    digitalWrite(DIR_PINS[i], HIGH);
    for (int b = 0; b < 200; b++) {
      digitalWrite(STEP_PINS[i], HIGH);
      delayMicroseconds(1000);
      digitalWrite(STEP_PINS[i], LOW);
      delayMicroseconds(1000);
    }
    delay(500);
  }

  CMD_PORT.println("Restoring original microstep settings...");
  for (int i = 0; i < 3; i++) {
    drivers[i]->microsteps(originalMicrosteps[i]); 
  }
  CMD_PORT.println("CALIBRATION COMPLETE.");
}
