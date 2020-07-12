#include <TimerOne.h>

#define BUFSZ 128
#define NSWITCHES 10

unsigned long last_halfstep;
int halfstepcount = 0;
bool stepState;

int enablePin = 9;
int stepPin = 3;
bool enabled = false;

bool switchstate[NSWITCHES];
int switchpin[NSWITCHES];

int curspeed = 0; // steps/sec
int targetspeed = 2000; // steps/sec
int accel = 10; // steps/sec per millisecond
int steps_per_rev = 200;
int halfsteps_per_rev = steps_per_rev * 2;

void setup()
{  
  Serial.begin(115200);

  switchpin[0] = 4;
  switchpin[1] = 10;
  switchpin[2] = 5;
  switchpin[3] = 16;
  switchpin[4] = 14;
  switchpin[5] = 15;
  switchpin[6] = A0;
  switchpin[7] = A1;
  switchpin[8] = A2;
  switchpin[9] = A3;

  for (int i = 0; i < 10; i++) {
    pinMode(switchpin[i], INPUT_PULLUP);
  }

  pinMode(stepPin, OUTPUT);
  pinMode(enablePin, OUTPUT);

  curspeed = 0;
  disableStepper();

  Timer1.initialize(0);
  Timer1.attachInterrupt(timerIsr);
}

// called by timer every time we want to do half a step (i.e. toggle step pin)
void timerIsr() {
  last_halfstep = micros();
  stepState = !stepState;
  digitalWrite(stepPin, stepState);
  halfstepcount = (halfstepcount + 1) % halfsteps_per_rev;
}

void loop()
{  
   accelerate();
   communicate();
   scanSwitches();
}

void scanSwitches() {
  for (int i = 0; i < NSWITCHES; i++) {
    int state = !digitalRead(switchpin[i]);
    if (state != switchstate[i]) {
      switchstate[i] = state;
      Serial.print(halfstepcount/2); Serial.print(state ? '+' : '-'); Serial.println(i);
      // TODO: we could interpolate between step counts by looking at micros() since last_halfstep
    }
  }
}

void accelerate() {
   static unsigned long last_accel;

   if (!enabled)
     return;
   
   if (millis() > last_accel && curspeed != targetspeed) {
    last_accel = millis();
    if (targetspeed > curspeed+10) {
      curspeed += accel;
    } else if (targetspeed < curspeed-10) {
      curspeed -= accel;
    } else {
      curspeed = targetspeed;
    }
    Timer1.setPeriod(500000 / curspeed);
   }
}

void enableStepper() {
  enabled = true;
  digitalWrite(enablePin, LOW);

  halfstepcount = 0;
  Timer1.start();
}

void disableStepper() {
  enabled = false;
  digitalWrite(enablePin, HIGH);
  
  Timer1.stop();

  stepState = false;
  digitalWrite(stepPin, stepState);
}

void communicate() {
  static char buf[BUFSZ];
  static int p;
  static bool seen_start;

  if (seen_start == false && halfstepcount == 0) {
    Serial.println(".");
    seen_start = true;
  }
  if (halfstepcount != 0)
    seen_start = false;
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r' || c== '\n' || p == BUFSZ-1) {
      buf[p++] = 0;
      serialCommand(buf);
      p = 0;
    } else {
      buf[p++] = c;
    }
  }
}

void serialCommand(char *buf) {
  char **params = split(buf);

  if (strcmp(params[0], "help") == 0) {
    Serial.print(
      "commands:\r\n"
      "   help           - show help\r\n"
      "   speed RPM      - set speed\r\n"
      "   start          - start motor\r\n"
      "   stop           - stop motor\r\n"
    );
      
  } else if (strcmp(params[0], "speed") == 0) {
    if (!params[1]) {
      Serial.println("error: usage: speed RPM");
      return;
    }
  
    unsigned long rpm = atoi(params[1]);
    if (rpm < 1 || rpm > 2000) {
      Serial.println("error: rpm must be between 1 and 2000");
      return;
    }

    targetspeed = rpm * steps_per_rev / 60;
    
  } else if (strcmp(params[0], "start") == 0) {
    enableStepper();
    
  } else if (strcmp(params[0], "stop") == 0) {
    curspeed = 0;
    disableStepper();

  }
}

// replace each space in buf with a \0, and return a (static!) nul-terminated array of pointers to the string parts
char **split(char *buf) {
  static char *parts[16];
  int n = 0;
  
  char *p = buf;
  while (*p && n < 15) {
    parts[n++] = p;
    while (*p && *p != ' ')
      p++;
    if (*p == ' ')
      *(p++) = 0;
  }
  parts[n++] = 0;
  
  return parts;
}
