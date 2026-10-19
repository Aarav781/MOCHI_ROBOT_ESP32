#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TOUCH_PIN 4
#define BUZZ_PIN 25
#define MPU_ADDR 0x68
#define SLEEP_AFTER 30000
#define SHAKE_LIMIT 0.6
#define TILT_LIMIT 0.35
#define TILT_SIGN 1       // change to -1 if eyes look the wrong way
#define CRY_TAPS 4        // taps needed to make Mochi cry
#define CRY_WINDOW 4000   // ...within this many milliseconds

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum State { AWAKE, SLEEPING };
State state = AWAKE;

unsigned long lastActivity = 0, lastBlink = 0, nextBlink = 3000;
unsigned long lastLook = 0, nextLook = 2000, touchStart = 0;
unsigned long tapWindowStart = 0;
int lookX = 0, tapCount = 0;
bool angryBeeped = false, wasTilted = false, prevTouched = false;
float lastMag = 1.0;
float ax = 0, ay = 0, az = 0;

void beep(int freq, int ms) {
  long half = 500000L / freq;
  long cycles = (long)freq * ms / 1000;
  for (long i = 0; i < cycles; i++) {
    digitalWrite(BUZZ_PIN, HIGH);
    delayMicroseconds(half);
    digitalWrite(BUZZ_PIN, LOW);
    delayMicroseconds(half);
  }
}

int16_t read16() {
  int16_t hi = Wire.read();
  int16_t lo = Wire.read();
  return (hi << 8) | lo;
}

bool shaken() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() < 6) return false;
  ax = read16() / 16384.0;
  ay = read16() / 16384.0;
  az = read16() / 16384.0;
  float mag = sqrt(ax * ax + ay * ay + az * az);
  bool s = fabs(mag - lastMag) > SHAKE_LIMIT;
  lastMag = mag;
  return s;
}

void drawNormal(int h) {
  display.clearDisplay();
  display.fillRoundRect(25 + lookX, 15 + (30 - h) / 2, 26, h, 6, WHITE);
  display.fillRoundRect(77 + lookX, 15 + (30 - h) / 2, 26, h, 6, WHITE);
  display.drawLine(54, 54, 60, 58, WHITE);
  display.drawLine(60, 58, 68, 58, WHITE);
  display.drawLine(68, 58, 74, 54, WHITE);
  display.display();
}

void drawHappy() {
  display.clearDisplay();
  for (int i = 0; i < 3; i++) {
    display.drawCircleHelper(38, 38 + i, 14, 1, WHITE);
    display.drawCircleHelper(38, 38 + i, 14, 2, WHITE);
    display.drawCircleHelper(90, 38 + i, 14, 1, WHITE);
    display.drawCircleHelper(90, 38 + i, 14, 2, WHITE);
  }
  display.fillCircle(64, 52, 8, WHITE);
  display.fillRect(54, 40, 20, 12, BLACK);
  display.display();
}

void drawAngry() {
  display.clearDisplay();
  display.fillRoundRect(25, 18, 26, 26, 5, WHITE);
  display.fillRoundRect(77, 18, 26, 26, 5, WHITE);
  display.fillTriangle(24, 16, 53, 16, 53, 31, BLACK);
  display.fillTriangle(104, 16, 75, 16, 75, 31, BLACK);
  display.drawLine(54, 58, 60, 54, WHITE);
  display.drawLine(60, 54, 68, 54, WHITE);
  display.drawLine(68, 54, 74, 58, WHITE);
  display.display();
}

void drawDizzy() {
  display.clearDisplay();
  float a = millis() / 120.0;
  for (int e = 0; e < 2; e++) {
    int cx = (e == 0) ? 38 : 90;
    display.drawCircle(cx, 28, 13, WHITE);
    display.drawLine(cx, 28, cx + 11 * cos(a), 28 + 11 * sin(a), WHITE);
    display.drawLine(cx, 28, cx + 6 * cos(a + 3.14), 28 + 6 * sin(a + 3.14), WHITE);
  }
  display.drawLine(52, 54, 58, 58, WHITE);
  display.drawLine(58, 58, 64, 54, WHITE);
  display.drawLine(64, 54, 70, 58, WHITE);
  display.drawLine(70, 58, 76, 54, WHITE);
  display.display();
}

void drawCry() {
  display.clearDisplay();
  // sad eyes (outer top corners cut off)
  display.fillRoundRect(25, 14, 26, 26, 5, WHITE);
  display.fillRoundRect(77, 14, 26, 26, 5, WHITE);
  display.fillTriangle(24, 12, 24, 27, 53, 12, BLACK);
  display.fillTriangle(104, 12, 104, 27, 75, 12, BLACK);
  // falling tears
  int t1 = (millis() / 25) % 22;
  int t2 = ((millis() / 25) + 11) % 22;
  display.fillCircle(32, 44 + t1, 2, WHITE);
  display.fillCircle(96, 44 + t2, 2, WHITE);
  display.fillCircle(44, 44 + t2, 2, WHITE);
  display.fillCircle(84, 44 + t1, 2, WHITE);
  // wobbly frown
  int w = (millis() / 150) % 2;
  display.drawLine(54, 60, 60, 56 + w, WHITE);
  display.drawLine(60, 56 + w, 68, 56 + w, WHITE);
  display.drawLine(68, 56 + w, 74, 60, WHITE);
  display.display();
}

void dizzy() {
  beep(900, 80);
  beep(700, 80);
  beep(500, 120);
  unsigned long start = millis();
  while (millis() - start < 2000) {
    drawDizzy();
    delay(30);
  }
  shaken();
}

void cry() {
  unsigned long start = millis();
  while (millis() - start < 4000) {
    drawCry();
    beep(800, 100);
    drawCry();
    beep(600, 150);
    unsigned long t = millis();
    while (millis() - t < 250) {
      drawCry();
      delay(30);
    }
  }
}

void drawSleep() {
  display.clearDisplay();
  display.fillRoundRect(25, 36, 26, 3, 1, WHITE);
  display.fillRoundRect(77, 36, 26, 3, 1, WHITE);
  display.drawLine(58, 54, 70, 54, WHITE);
  int p = (millis() / 700) % 3;
  display.setTextColor(WHITE);
  display.setTextSize(p + 1);
  display.setCursor(105, 22 - p * 7);
  display.print("z");
  display.display();
}

void wakeUp() {
  beep(1000, 60);
  beep(1500, 60);
  beep(2000, 100);
  for (int h = 4; h <= 30; h += 6) { drawNormal(h); delay(30); }
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  shaken();
  lastActivity = millis();
  drawNormal(30);
}

void loop() {
  bool touched = digitalRead(TOUCH_PIN) == HIGH;
  bool shake = shaken();

  if (state == SLEEPING) {
    drawSleep();
    if (touched || shake) {
      state = AWAKE;
      lastActivity = millis();
      wakeUp();
    }
    prevTouched = touched;
    delay(50);
    return;
  }

  if (shake) {
    lastActivity = millis();
    dizzy();
    return;
  }

  // count quick taps -> cry
  if (touched && !prevTouched) {
    if (millis() - tapWindowStart > CRY_WINDOW) {
      tapCount = 0;
      tapWindowStart = millis();
    }
    tapCount++;
  }
  prevTouched = touched;
  if (tapCount >= CRY_TAPS) {
    tapCount = 0;
    lastActivity = millis();
    cry();
    return;
  }

  if (touched) {
    lastActivity = millis();
    if (touchStart == 0) {
      touchStart = millis();
      beep(1800, 60);
      beep(2400, 90);
    }
    if (millis() - touchStart > 2500) {
      drawAngry();
      if (!angryBeeped) { beep(400, 200); angryBeeped = true; }
    } else {
      drawHappy();
    }
    delay(50);
    return;
  }
  touchStart = 0;
  angryBeeped = false;

  if (millis() - lastActivity > SLEEP_AFTER) {
    beep(600, 200);
    beep(400, 300);
    state = SLEEPING;
    return;
  }

  if (fabs(ay) > TILT_LIMIT) {
    lookX = ((ay > 0) ? 1 : -1) * TILT_SIGN * 8;
    wasTilted = true;
    lastActivity = millis();
  } else {
    if (wasTilted) { lookX = 0; wasTilted = false; }
    if (millis() - lastLook > nextLook) {
      int opts[3] = { -8, 0, 8 };
      lookX = opts[random(3)];
      lastLook = millis();
      nextLook = random(1500, 4000);
    }
  }

  if (millis() - lastBlink > nextBlink) {
    for (int h = 30; h >= 4; h -= 6) { drawNormal(h); delay(15); }
    for (int h = 4; h <= 30; h += 6) { drawNormal(h); delay(15); }
    lastBlink = millis();
    nextBlink = random(2000, 6000);
  } else {
    drawNormal(30);
    delay(30);
  }
}