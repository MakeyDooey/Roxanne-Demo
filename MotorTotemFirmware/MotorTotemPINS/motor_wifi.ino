#include <WiFi.h>
#include <WebSocketsServer.h>

// ── WiFi AP credentials ──────────────────────────────────────────
const char* AP_SSID = "MotorConsole";
const char* AP_PASS = "12345678";

// ── Pin definitions ──────────────────────────────────────────────
const int enPin       = 38;
const int stepPins[]  = {21, 48, 36};
const int dirPins[]   = {47, 35, 37};

// ── Stepper config ───────────────────────────────────────────────
// motorDelay[i] = microseconds between steps (lower = faster)
unsigned long motorDelay[] = {2000, 2000, 2000};

// ── State ────────────────────────────────────────────────────────
int           activeMotor = -1;
unsigned long lastStep    = 0;

WebSocketsServer ws(81);

// ─────────────────────────────────────────────────────────────────
void handleCommand(String cmd, uint8_t clientNum) {
  cmd.trim();
  Serial.print("GOT: "); Serial.println(cmd);

  // SPEED:idx:value  (value = delay in microseconds, 200–5000)
  if (cmd.startsWith("SPEED:")) {
    int fc  = cmd.indexOf(':');
    int sc  = cmd.lastIndexOf(':');
    int idx = cmd.substring(fc + 1, sc).toInt();
    unsigned long val = cmd.substring(sc + 1).toInt();
    if (idx >= 0 && idx < 3 && val >= 200 && val <= 5000) {
      motorDelay[idx] = val;
      String reply = "OK speed=" + String(val) + " motor=" + String(idx);
      ws.sendTXT(clientNum, reply);
      Serial.println(reply);
    }
    return;
  }

  if (cmd.startsWith("START:")) {
    int fc  = cmd.indexOf(':');
    int sc  = cmd.lastIndexOf(':');
    int idx = cmd.substring(fc + 1, sc).toInt();
    String dirStr = cmd.substring(sc + 1);
    dirStr.trim();

    if (idx >= 0 && idx < 3) {
      activeMotor = idx;
      if (dirStr == "CW") {
        digitalWrite(dirPins[idx], HIGH);
        ws.sendTXT(clientNum, "DIR=HIGH");
      } else {
        digitalWrite(dirPins[idx], LOW);
        ws.sendTXT(clientNum, "DIR=LOW");
      }
      delay(5);
      String reply = "OK motor=" + String(idx) + " dir=" + dirStr;
      ws.sendTXT(clientNum, reply);
      Serial.println(reply);
    }
    return;
  }

  if (cmd == "STOP") {
    activeMotor = -1;
    ws.sendTXT(clientNum, "OK:STOP");
    Serial.println("OK:STOP");
    return;
  }

  ws.sendTXT(clientNum, "ERR:UNKNOWN");
}

// ─────────────────────────────────────────────────────────────────
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("WS client #%u connected\n", num);
      ws.sendTXT(num, "READY");
      break;
    case WStype_DISCONNECTED:
      Serial.printf("WS client #%u disconnected\n", num);
      activeMotor = -1;
      break;
    case WStype_TEXT: {
      String cmd = String((char*)payload);
      handleCommand(cmd, num);
      break;
    }
    default: break;
  }
}

// ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(enPin, OUTPUT);
  digitalWrite(enPin, LOW);
  for (int i = 0; i < 3; i++) {
    pinMode(stepPins[i], OUTPUT);
    pinMode(dirPins[i],  OUTPUT);
    digitalWrite(stepPins[i], LOW);
    digitalWrite(dirPins[i],  LOW);
  }

  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  ws.begin();
  ws.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

// ─────────────────────────────────────────────────────────────────
void loop() {
  ws.loop();

  if (activeMotor != -1) {
    unsigned long now = micros();
    if (now - lastStep >= motorDelay[activeMotor]) {
      digitalWrite(stepPins[activeMotor], HIGH);
      delayMicroseconds(5);
      digitalWrite(stepPins[activeMotor], LOW);
      lastStep = now;
    }
  }
}
