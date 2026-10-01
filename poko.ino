#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// POKO — Step 1: Animated Eyes
// ============================================================

// OLED
#define OLED_SDA 8
#define OLED_SCL 9
#define OLED_ADDRESS 0x3C

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

// ------------------------------------------------------------
// Screen
// ------------------------------------------------------------

const int SCREEN_W = 128;
const int SCREEN_H = 64;

// ------------------------------------------------------------
// Eyes
// ------------------------------------------------------------

const int LEFT_EYE_X  = 8;
const int RIGHT_EYE_X = 68;

const int EYE_Y = 18;
const int EYE_W = 52;
const int EYE_H = 38;

const int PUPIL_R = 9;

const int MAX_PUPIL_X = 12;
const int MAX_PUPIL_Y = 7;

// ------------------------------------------------------------
// Eye state
// ------------------------------------------------------------

float pupilX = 0;
float pupilY = 0;

float targetPupilX = 0;
float targetPupilY = 0;

// ------------------------------------------------------------
// Blinking
// ------------------------------------------------------------

bool blinking = false;

float blinkAmount = 0.0;

unsigned long blinkStartTime = 0;
unsigned long nextBlinkTime = 0;

const unsigned long BLINK_DURATION = 180;

// ------------------------------------------------------------
// Looking
// ------------------------------------------------------------

unsigned long nextLookTime = 0;

// ------------------------------------------------------------
// Frame timing
// ------------------------------------------------------------

unsigned long lastFrameTime = 0;

const unsigned long FRAME_INTERVAL = 16;

// ============================================================
// Smooth movement
// ============================================================

float smoothApproach(
  float current,
  float target,
  float speed
)
{
  return current + (target - current) * speed;
}

// ============================================================
// Choose where Poko looks
// ============================================================

void chooseLookDirection()
{
  int direction = random(0, 5);

  switch (direction)
  {
    // Center
    case 0:
      targetPupilX = 0;
      targetPupilY = 0;
      break;

    // Left
    case 1:
      targetPupilX = -MAX_PUPIL_X;
      targetPupilY = random(-3, 4);
      break;

    // Right
    case 2:
      targetPupilX = MAX_PUPIL_X;
      targetPupilY = random(-3, 4);
      break;

    // Up
    case 3:
      targetPupilX = random(-5, 6);
      targetPupilY = -MAX_PUPIL_Y;
      break;

    // Down
    case 4:
      targetPupilX = random(-5, 6);
      targetPupilY = MAX_PUPIL_Y;
      break;
  }

  nextLookTime = millis() + random(1200, 3000);
}

// ============================================================
// Start blink
// ============================================================

void startBlink()
{
  if (!blinking)
  {
    blinking = true;
    blinkStartTime = millis();
  }
}

// ============================================================
// Update blink animation
// ============================================================

void updateBlink()
{
  if (!blinking)
  {
    if (millis() >= nextBlinkTime)
    {
      blinking = true;
      blinkStartTime = millis();
    }

    return;
  }

  unsigned long elapsed =
    millis() - blinkStartTime;

  const unsigned long CLOSE_TIME  = 100;
  const unsigned long CLOSED_TIME = 70;
  const unsigned long OPEN_TIME   = 140;

  // ----------------------------------------------------------
  // Closing
  // ----------------------------------------------------------

  if (elapsed < CLOSE_TIME)
  {
    blinkAmount =
      (float)elapsed / CLOSE_TIME;
  }

  // ----------------------------------------------------------
  // Completely closed
  // ----------------------------------------------------------

  else if (elapsed < CLOSE_TIME + CLOSED_TIME)
  {
    blinkAmount = 1.0;
  }

  // ----------------------------------------------------------
  // Opening
  // ----------------------------------------------------------

  else if (
    elapsed <
    CLOSE_TIME +
    CLOSED_TIME +
    OPEN_TIME
  )
  {
    unsigned long openElapsed =
      elapsed -
      CLOSE_TIME -
      CLOSED_TIME;

    blinkAmount =
      1.0 -
      (
        (float)openElapsed /
        OPEN_TIME
      );
  }

  // ----------------------------------------------------------
  // Finished
  // ----------------------------------------------------------

  else
  {
    blinkAmount = 0.0;

    blinking = false;

    nextBlinkTime =
      millis() + random(1800, 5000);
  }

  blinkAmount =
    constrain(
      blinkAmount,
      0.0,
      1.0
    );
}

// ============================================================
// Draw one eye
// ============================================================

void drawEye(
  int x,
  int y,
  float pupilOffsetX,
  float pupilOffsetY
)
{
  // ----------------------------------------------------------
  // 1. Draw the normal eye
  // ----------------------------------------------------------

  display.setDrawColor(1);

  display.drawRBox(
    x,
    y,
    EYE_W,
    EYE_H,
    8
  );

  // ----------------------------------------------------------
  // 2. Draw pupil
  // ----------------------------------------------------------

  if (blinkAmount < 0.5)
  {
    int pupilCenterX =
      x + EYE_W / 2 + pupilOffsetX;

    int pupilCenterY =
      y + EYE_H / 2 + pupilOffsetY;

    display.setDrawColor(0);

    display.drawDisc(
      pupilCenterX,
      pupilCenterY,
      PUPIL_R
    );
  }

  // ----------------------------------------------------------
  // 3. Close the eye using eyelids
  // ----------------------------------------------------------

  if (blinkAmount > 0.0)
  {
    display.setDrawColor(0);

    int coverHeight =
      (EYE_H / 2) * blinkAmount;

    // Top eyelid
    if (coverHeight > 0)
    {
      display.drawBox(
        x,
        y,
        EYE_W,
        coverHeight
      );
    }

    // Bottom eyelid
    if (coverHeight > 0)
    {
      display.drawBox(
        x,
        y + EYE_H - coverHeight,
        EYE_W,
        coverHeight
      );
    }
  }

  // Restore white drawing color
  display.setDrawColor(1);
}

// ============================================================
// Draw Poko's face
// ============================================================

void drawFace()
{
  display.clearBuffer();

  drawEye(
    LEFT_EYE_X,
    EYE_Y,
    pupilX,
    pupilY
  );

  drawEye(
    RIGHT_EYE_X,
    EYE_Y,
    pupilX,
    pupilY
  );

  display.sendBuffer();
}

// ============================================================
// Setup
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(300);

  Serial.println();
  Serial.println("==============================");
  Serial.println("          POKO");
  Serial.println("     A tiny life on your desk");
  Serial.println("==============================");

  // I2C
  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  // OLED address
  display.setI2CAddress(
    OLED_ADDRESS << 1
  );

  // Start OLED
  display.begin();

  // Random animation timing
  randomSeed(micros());

  pupilX = 0;
  pupilY = 0;

  targetPupilX = 0;
  targetPupilY = 0;

  nextBlinkTime =
    millis() + random(1500, 3500);

  nextLookTime =
    millis() + random(1000, 2000);

  Serial.println("OLED initialized.");
  Serial.println("POKO eyes starting...");
}

// ============================================================
// Main loop
// ============================================================

void loop()
{
  unsigned long now = millis();

  // Choose a new direction
  if (now >= nextLookTime)
  {
    chooseLookDirection();
  }

  // Smooth eye movement
  pupilX =
    smoothApproach(
      pupilX,
      targetPupilX,
      0.08
    );

  pupilY =
    smoothApproach(
      pupilY,
      targetPupilY,
      0.08
    );

  // Update blinking
  updateBlink();

  // ~60 FPS
  if (now - lastFrameTime >= FRAME_INTERVAL)
  {
    lastFrameTime = now;

    drawFace();
  }
}