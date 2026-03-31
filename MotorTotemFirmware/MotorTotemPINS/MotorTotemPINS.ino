const int enPin = 38;

const int stepPins[] = {21, 48, 36}; 
const int dirPins[]  = {47, 35, 37};

const int baseDelay = 500;
const int stepMult[] = {1, 1, 1};

int activeMotor = -1;
unsigned long lastStep = 0;
String cmdBuffer = "";

void handleCommand(String cmd);

void setup() {
  Serial0.begin(115200);
  delay(500);

  pinMode(enPin, OUTPUT);
  digitalWrite(enPin, LOW);

  for (int i = 0; i < 3; i++) {
    pinMode(stepPins[i], OUTPUT);
    pinMode(dirPins[i],  OUTPUT);
    digitalWrite(stepPins[i], LOW);
    digitalWrite(dirPins[i],  LOW);
  }

  Serial0.println("READY");
}

void loop() {
  while (Serial0.available() > 0) {
    char c = Serial0.read();
    if (c == '\n' || c == '\r') {
      if (cmdBuffer.length() > 0) {
        handleCommand(cmdBuffer);
        cmdBuffer = "";
      }
    } else {
      cmdBuffer += c;
    }
  }

  if (activeMotor != -1) {
    unsigned long now = micros();
    unsigned long motorDelay = (unsigned long)(baseDelay * stepMult[activeMotor]);
    if (now - lastStep >= motorDelay) {
      digitalWrite(stepPins[activeMotor], HIGH);
      delayMicroseconds(5);
      digitalWrite(stepPins[activeMotor], LOW);
      lastStep = now;
    }
  }
}

void handleCommand(String cmd) {
  cmd.trim();
  Serial0.print("GOT: "); Serial0.println(cmd);

  if (cmd.startsWith("START:")) {
    int firstColon  = cmd.indexOf(':');
    int secondColon = cmd.lastIndexOf(':');
    int idx = cmd.substring(firstColon + 1, secondColon).toInt();
    String dirStr = cmd.substring(secondColon + 1);
    dirStr.trim();

    if (idx >= 0 && idx < 3) {
      activeMotor = idx;
      if (dirStr == "CW") {
        digitalWrite(dirPins[idx], HIGH);
        Serial0.println("DIR=HIGH");
      } else {
        digitalWrite(dirPins[idx], LOW);
        Serial0.println("DIR=LOW");
      }
      delay(5);
      Serial0.print("OK motor="); Serial0.print(idx);
      Serial0.print(" dir="); Serial0.println(dirStr);
    }
  }
  else if (cmd == "STOP") {
    activeMotor = -1;
    Serial0.println("OK:STOP");
  }
}