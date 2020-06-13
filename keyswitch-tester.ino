/* Keyboard switch tester
 * 
 * James Stanley 2020
 * 
 * Pin assignments:
 *   9 - servo
 *   13 - status LED
 *   2 - switch (normally open, connect to ground to activate)
 * 
 * Output over serial at 9600 baud
 */

#include <Servo.h>

Servo servo;

int servopin = 9;
int ledpin = 10;
int switchpin = 2;

int servomin = 1300;
int servomax = 2000;
float maxspeed = 3; // servo usecs per ms

float servopos;

void setup() {
  servo.attach(servopin);
  pinMode(ledpin, OUTPUT);
  pinMode(switchpin, INPUT_PULLUP);
  servo.writeMicroseconds(servomin);
  Serial.begin(9600);
}

void loop() {
  if (!pressed()) {
    cycle();
  } else {
    busy_delay(1000);
  }
  busy_delay(100); // let switch settle
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
  busy_delay((oldpos - servomin) / maxspeed); // give the servo time to get back
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
    pos = pos + (now - lastms) * maxspeed;
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
    pos = pos - (now - lastms) * maxspeed;
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
