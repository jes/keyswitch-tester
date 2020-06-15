/* Keyboard switch tester
 * 
 * James Stanley 2020
 * 
 * Pin assignments:
 *   9 - servo
 *   10 - status LED
 *   2 - switch (normally open, connect to ground to activate)
 * 
 * Output over serial at 9600 baud
 */

#include <Servo.h>

Servo servo;

int servopin = 9;
int ledpin = 10;
int switchpin = 2;

int servomin = 1500;
int servomax = 2000;
float servospeed = 0.2; // servo usecs per ms
float servomaxspeed = 4; // speed to reset

float servopos;

void setup() {
  servo.attach(servopin);
  pinMode(ledpin, OUTPUT);
  pinMode(switchpin, INPUT_PULLUP);
  writeServo(servomin);
  Serial.begin(9600);
  delay(1000); // wait for serial port to work?
  Serial.print("keyswitch-tester begins\n");
}

void loop() {
  if (!pressed()) {
    cycle();
  } else {
    busy_delay(1000);
  }
}

void busy_delay(unsigned long ms) {
  unsigned long startms = millis();
  unsigned long endms = startms + ms;

  // check that "start <= millis()" so that we don't wait forever when millis() overflows
  while (millis() < endms && startms <= millis()) {
     pressed(); // just keep the led status up to date
  }
}

bool pressed() {
  int state = !digitalRead(switchpin);
  digitalWrite(ledpin, state);
  return state;
}

void writeServo(float val) {
  servopos = val;
  servo.writeMicroseconds(val);
}

void resetServo() {
  float oldpos = servopos;
  writeServo(servomin);
  busy_delay((oldpos - servomin) / servomaxspeed); // give the servo time to get back
}

void cycle() {
  unsigned long lastms = millis();
  float pos = servopos;
  
  while (!pressed() && pos < servomax) {
    unsigned long now = millis();
    if (now < lastms) { // time overflow: bail out and start again
      resetServo();
      return;
    }
    pos = pos + (now - lastms) * servospeed;
    lastms = millis();
    
    if (pos > servomax)
      pos = servomax;
    writeServo(pos);
  }

  if (pos < servomax) {
    Serial.print("OK "); Serial.print((int)pos); Serial.print(" ");
    back_cycle();
  } else {
    Serial.print("FAIL "); Serial.print((int)pos); Serial.print("\n");
    resetServo();
  }
}

void back_cycle() {
  unsigned long lastms = millis();
  float pos = servopos;

  busy_delay(100);
  
  while (pressed() && pos > servomin) {
    unsigned long now = millis();
    if (now < lastms) { // time overflow: bail out and start again
      Serial.println("OK (timewrap)\n");
      resetServo();
      return;
    }
    pos = pos - (now - lastms) * servospeed;
    lastms = millis();
    
    if (pos < servomin)
      pos = servomin;
    writeServo(pos);
  }

  if (pos > servomin) {
    Serial.print("OK "); Serial.print((int)pos); Serial.print("\n");
    resetServo();
  } else {
    Serial.print("FAIL "); Serial.print((int)pos); Serial.print("\n");
    resetServo();
  }
}
