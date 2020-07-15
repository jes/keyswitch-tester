#include <TimerOne.h>

#define BUFSZ 128
#define NSWITCHES 10

volatile unsigned long last_halfstep;
volatile int halfstepcount = 0;
volatile bool stepState;

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
unsigned long micros_per_halfstep;

void setup()
{  
  Serial.begin(115200);

  switchpin[0] = 4;
  switchpin[1] = 10;
  switchpin[2] = 15;
  switchpin[3] = A1;
  switchpin[4] = 5;
  switchpin[5] = 14;
  switchpin[6] = 16;
  switchpin[7] = A2;
  switchpin[8] = A3;
  switchpin[9] = A0;

  for (int i = 0; i < 10; i++) {
    pinMode(switchpin[i], INPUT_PULLUP);
  }

  pinMode(stepPin, OUTPUT);
  pinMode(enablePin, OUTPUT);

  curspeed = 0;
  disableStepper();

  Timer1.initialize(0);
  Timer1.stop();
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
      unsigned long last = last_halfstep;
      int count = halfstepcount;
      double proportion = ((double)(micros() - last) / (double)micros_per_halfstep);

      // XXX: think this can be out of range due to race window between grabbing last_halfstep and halfstepcount?
      // if proportion > 1 then it means halfstepcount will already haev increased betwee grabbing last_halfstep and calculating
      // proportion, so we can set the proportion to 0 and assume we already have the updated halfstepcount.
      if (proportion > 1)
        proportion = 0;

      double halfsteps = count + proportion;
      Serial.print(halfsteps/2); Serial.print(state ? '+' : '-'); Serial.println(i);
    }
  }
}

void accelerate() {
   static unsigned long last_accel;

   if (!enabled)
     return;
   
   if (millis() > last_accel && curspeed != targetspeed) {
    last_accel = millis();
    if (targetspeed > curspeed+accel) {
      curspeed += accel;
    } else if (targetspeed < curspeed-accel) {
      curspeed -= accel;
    } else {
      curspeed = targetspeed;
    }
    micros_per_halfstep = 500000 / curspeed;
    Timer1.setPeriod(micros_per_halfstep);
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
