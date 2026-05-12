/* Touchscreen library for XPT2046 Touch Controller Chip
 * Copyright (c) 2015, Paul Stoffregen, paul@pjrc.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "XPT2046_Bitbang.h"

#define CMD_READ_Z1 0xB1
#define CMD_READ_Z2 0xC1
#define CMD_READ_X 0x91
#define CMD_READ_Y 0xD1

#define DELAY 5
#define Z_THRESHOLD 300
#define Z_THRESHOLD_INT 75
#define MSEC_THRESHOLD 3

static XPT2046_Bitbang *isrPinptr;
void isrPin(void);

void XPT2046_Bitbang::begin() {
  pinMode(clkPin, OUTPUT);
  pinMode(misoPin, INPUT);
  pinMode(mosiPin, OUTPUT);
  pinMode(csPin, OUTPUT);

  digitalWrite(csPin, HIGH);
  digitalWrite(clkPin, LOW);

  if (tirqPin != 255) {
    pinMode(tirqPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(tirqPin), isrPin, FALLING);
    isrPinptr = this;
  }
}

ISR_PREFIX
void isrPin(void) {
  XPT2046_Bitbang *o = isrPinptr;
  o->isrWake = true;
}

TS_Point XPT2046_Bitbang::getPoint() {
  update();
  return TS_Point(x, y, z);
}

bool XPT2046_Bitbang::tirqTouched() { return (isrWake); }

bool XPT2046_Bitbang::touched() {
  update();
  return (z >= Z_THRESHOLD);
}

bool XPT2046_Bitbang::bufferEmpty() {
  return ((millis() - lastMs) < MSEC_THRESHOLD);
}

static int16_t besttwoavg(int16_t x, int16_t y, int16_t z) {
  int16_t da, db, dc;
  int16_t reta = 0;

  if (x > y)
    da = x - y;
  else
    da = y - x;
  if (x > z)
    db = x - z;
  else
    db = z - x;
  if (z > y)
    dc = z - y;
  else
    dc = y - z;

  if (da <= db && da <= dc)
    reta = (x + y) >> 1;
  else if (db <= da && db <= dc)
    reta = (x + z) >> 1;
  else
    reta = (y + z) >>
           1; //    else if ( dc <= da && dc <= db ) reta = (x + y) >> 1;

  return (reta);
}

uint16_t XPT2046_Bitbang::readSPI(byte command) {
  uint16_t result = 0;

  for (int i = 7; i >= 0; i--) {
    digitalWrite(mosiPin, command & (1 << i));
    digitalWrite(clkPin, LOW);
    delayMicroseconds(DELAY);
    digitalWrite(clkPin, HIGH);
    delayMicroseconds(DELAY);
  }

  digitalWrite(mosiPin, LOW);
  digitalWrite(clkPin, LOW);

  for (int i = 15; i >= 0; i--) {
    digitalWrite(clkPin, HIGH);
    delayMicroseconds(DELAY);
    digitalWrite(clkPin, LOW);
    delayMicroseconds(DELAY);
    result |= (digitalRead(misoPin) << i);
  }

  return result >> 4;
}

// TODO: perhaps a future version should offer an option for more oversampling,
//       with the RANSAC algorithm https://en.wikipedia.org/wiki/RANSAC

void XPT2046_Bitbang::update() {
  int16_t data[6];
  int zRaw;

  if (!isrWake)
    return;

  uint32_t now = millis();

  if (now - lastMs < MSEC_THRESHOLD)
    return;

  digitalWrite(csPin, LOW);

  int16_t z1 = readSPI(CMD_READ_Z1);
  int16_t z2 = readSPI(CMD_READ_Z2);

  zRaw = (z1 + 4095) - z2;

  if (zRaw >= Z_THRESHOLD) {
    data[0] = readSPI(CMD_READ_X);
    data[1] = readSPI(CMD_READ_Y); // make 3 x-y measurements
    data[2] = readSPI(CMD_READ_X);
    data[3] = readSPI(CMD_READ_Y);
  }

  data[4] = readSPI(CMD_READ_X); // Last Y touch power down
  data[5] = readSPI(CMD_READ_Y);

  digitalWrite(csPin, HIGH);

  // Serial.printf("z=%d  ::  z1=%d,  z2=%d  ", z, z1, z2);
  if (zRaw < 0)
    zRaw = 0;

  if (zRaw < Z_THRESHOLD) { //	if ( !touched ) {
    // Serial.println();
    z = 0;
    x = -1;
    y = -1;

    if (zRaw < Z_THRESHOLD_INT) { //	if ( !touched ) {
      if (tirqPin != 255)
        isrWake = false;
    }

    return;
  }

  z = zRaw;

  // Average pair with least distance between each measured x then y
  // Serial.printf("    z1=%d,z2=%d  ", z1, z2);
  // Serial.printf("p=%d,  %d,%d  %d,%d  %d,%d", zraw,
  // data[0], data[1], data[2], data[3], data[4], data[5]);
  int16_t xRaw = besttwoavg(data[0], data[2], data[4]);
  int16_t yRaw = besttwoavg(data[1], data[3], data[5]);
  int16_t xBound = (rotation & 1) ? screenWidth : screenHeight;
  int16_t yBound = (rotation & 1) ? screenHeight : screenWidth;

  // Serial.printf("    %d,%d", x, y);
  // Serial.println();
  if (zRaw < Z_THRESHOLD)
    return;

  int16_t newX, newY;
  uint32_t delta = now - lastMs;

  switch (rotation) {
  case 0:
    newX = 4095 - yRaw;
    newY = xRaw;
    break;
  case 1:
    newX = xRaw;
    newY = yRaw;
    break;
  case 2:
    newX = yRaw;
    newY = 4095 - xRaw;
    break;
  default: // 3
    newX = 4095 - xRaw;
    newY = 4095 - yRaw;
  }

  newX = map(newX, xMin, xMax, 0, xBound);
  newY = map(newY, yMin, yMax, 0, yBound);

  int16_t dx = abs(newX - x);
  int16_t dy = abs(newY - y);
  uint16_t distance = dx + dy;

  // Fixes jitter when drawing lines
  if (x != -1 && distance > (MAX_MS_TRAVEL * delta)) {
    z = 0;
    return;
  }

  x = newX;
  y = newY;
  lastMs = now; // good read completed, set wait
}
