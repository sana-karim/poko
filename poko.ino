#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// POKO
// A tiny life on your desk.
//
// Current features:
// 👀 Animated eyes
// 😴 Sleep / Wake
// ============================================================


// ============================================================
// OLED
// ============================================================

#define OLED_SDA 8
#define OLED_SCL 9
#define OLED_ADDRESS 0x3C

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);


// ============================================================
// Screen
// ============================================================

const int SCREEN_W = 128;
const int SCREEN_H = 64;


// ============================================================
// Eyes
// ============================================================

const int LEFT_EYE_X  = 8;
const int RIGHT_EYE_X = 68;

const int EYE_Y = 18;
const int EYE_W = 52;
const int EYE_H = 38;

const int PUPIL_R = 9;

const int MAX_PUPIL_X = 12;
const int MAX_PUPIL_Y = 7;


// ============================================================
// Eye movement
// ============================================================

float pupilX = 0;
float pupilY = 0;

float targetPupilX = 0;
float targetPupilY = 0;

unsigned long nextLookTime = 0;


// ============================================================
// Eye closing amount
//
// 0.0 = completely open
// 1.0 = completely closed
//
// This is the SINGLE source of truth for eyelid position.
// ============================================================

float eyeCloseAmount = 0.0;


// ============================================================
// Normal blink
// ============================================================

bool blinking = false;

unsigned long blinkStartTime = 0;
unsigned long nextBlinkTime = 0;


// ============================================================
// Sleep / Wake state
// ============================================================

enum PokoState
{
  POKO_AWAKE,
  POKO_SLEEPY,
  POKO_SLEEPING,
  POKO_WAKING
};

PokoState pokoState = POKO_AWAKE;

unsigned long stateStartTime = 0;


// ============================================================
// TEST TIMINGS
//
// Awake      = 15 seconds
// Sleepy     = 2.5 seconds
// Sleeping   = 10 seconds
// Waking     = 2 seconds
//
// These are intentionally short for testing.
// We can increase them later.
// ============================================================

const unsigned long AWAKE_TIME   = 15000;
const unsigned long SLEEPY_TIME  = 2500;
const unsigned long SLEEP_TIME   = 10000;
const unsigned long WAKING_TIME  = 2000;


// ============================================================
// Frame timing
// ============================================================

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
    // --------------------------------------------------------
    // Center
    // --------------------------------------------------------

    case 0:

      targetPupilX = 0;
      targetPupilY = 0;

      break;


    // --------------------------------------------------------
    // Left
    // --------------------------------------------------------

    case 1:

      targetPupilX = -MAX_PUPIL_X;
      targetPupilY = random(-3, 4);

      break;


    // --------------------------------------------------------
    // Right
    // --------------------------------------------------------

    case 2:

      targetPupilX = MAX_PUPIL_X;
      targetPupilY = random(-3, 4);

      break;


    // --------------------------------------------------------
    // Up
    // --------------------------------------------------------

    case 3:

      targetPupilX = random(-5, 6);
      targetPupilY = -MAX_PUPIL_Y;

      break;


    // --------------------------------------------------------
    // Down
    // --------------------------------------------------------

    case 4:

      targetPupilX = random(-5, 6);
      targetPupilY = MAX_PUPIL_Y;

      break;
  }


  // Choose another direction after 1.2–3 seconds

  nextLookTime =
    millis() + random(1200, 3000);
}


// ============================================================
// Start normal blink
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
// Update normal blink
//
// Close → pause → open
//
// Only active while Poko is awake.
// ============================================================

void updateBlink()
{
  // Normal blinking is ONLY allowed while awake.

  if (pokoState != POKO_AWAKE)
  {
    return;
  }


  // ----------------------------------------------------------
  // Waiting for next blink
  // ----------------------------------------------------------

  if (!blinking)
  {
    eyeCloseAmount = 0.0;


    if (millis() >= nextBlinkTime)
    {
      blinking = true;
      blinkStartTime = millis();
    }


    return;
  }


  // ----------------------------------------------------------
  // Blink timing
  // ----------------------------------------------------------

  const unsigned long CLOSE_TIME  = 100;
  const unsigned long CLOSED_TIME = 70;
  const unsigned long OPEN_TIME   = 140;


  unsigned long elapsed =
    millis() - blinkStartTime;


  // ----------------------------------------------------------
  // Close
  // ----------------------------------------------------------

  if (elapsed < CLOSE_TIME)
  {
    eyeCloseAmount =
      (float)elapsed /
      (float)CLOSE_TIME;
  }


  // ----------------------------------------------------------
  // Fully closed
  // ----------------------------------------------------------

  else if (
    elapsed <
    CLOSE_TIME + CLOSED_TIME
  )
  {
    eyeCloseAmount = 1.0;
  }


  // ----------------------------------------------------------
  // Open
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


    eyeCloseAmount =
      1.0 -
      (
        (float)openElapsed /
        (float)OPEN_TIME
      );
  }


  // ----------------------------------------------------------
  // Blink finished
  // ----------------------------------------------------------

  else
  {
    eyeCloseAmount = 0.0;

    blinking = false;

    nextBlinkTime =
      millis() + random(1800, 5000);
  }


  eyeCloseAmount =
    constrain(
      eyeCloseAmount,
      0.0,
      1.0
    );
}


// ============================================================
// Sleep animation amount
//
// Returns:
//
// 0.0 = open
// 1.0 = closed
//
// Uses SmoothStep so the transition starts and ends gently.
// ============================================================

float getSleepAmount()
{
  unsigned long elapsed =
    millis() - stateStartTime;


  // ----------------------------------------------------------
  // Sleepy
  //
// Smoothly close eyes.
// ----------------------------------------------------------

  if (pokoState == POKO_SLEEPY)
  {
    float progress =
      (float)elapsed /
      (float)SLEEPY_TIME;


    progress =
      constrain(
        progress,
        0.0,
        1.0
      );


    // SmoothStep

    float smoothProgress =
      progress *
      progress *
      (3.0 - 2.0 * progress);


    return smoothProgress;
  }


  // ----------------------------------------------------------
  // Sleeping
  // ----------------------------------------------------------

  if (pokoState == POKO_SLEEPING)
  {
    return 1.0;
  }


  // ----------------------------------------------------------
  // Waking
  //
// Smoothly open eyes.
// ----------------------------------------------------------

  if (pokoState == POKO_WAKING)
  {
    float progress =
      (float)elapsed /
      (float)WAKING_TIME;


    progress =
      constrain(
        progress,
        0.0,
        1.0
      );


    // SmoothStep

    float smoothProgress =
      progress *
      progress *
      (3.0 - 2.0 * progress);


    return 1.0 - smoothProgress;
  }


  return 0.0;
}


// ============================================================
// Update sleep / wake animation
//
// This function owns eyeCloseAmount whenever Poko is NOT awake.
// ============================================================

void updateSleepAnimation()
{
  // Normal blinking owns eyeCloseAmount while awake.

  if (pokoState == POKO_AWAKE)
  {
    return;
  }


  eyeCloseAmount =
    getSleepAmount();


  eyeCloseAmount =
    constrain(
      eyeCloseAmount,
      0.0,
      1.0
    );
}


// ============================================================
// Update Poko state
// ============================================================

void updatePokoState()
{
  unsigned long now = millis();


  switch (pokoState)
  {
    // ========================================================
    // AWAKE
    // ========================================================

    case POKO_AWAKE:

      if (
        now - stateStartTime >=
        AWAKE_TIME
      )
      {
        pokoState = POKO_SLEEPY;

        stateStartTime = now;


        // Stop normal blinking

        blinking = false;


        // Start sleep animation fully open

        eyeCloseAmount = 0.0;


        // Center eyes

        targetPupilX = 0;
        targetPupilY = 0;

        pupilX = 0;
        pupilY = 0;


        Serial.println(
          "POKO is getting sleepy..."
        );
      }

      break;


    // ========================================================
    // SLEEPY
    // ========================================================

    case POKO_SLEEPY:

      if (
        now - stateStartTime >=
        SLEEPY_TIME
      )
      {
        pokoState = POKO_SLEEPING;

        stateStartTime = now;


        Serial.println(
          "POKO is sleeping..."
        );
      }

      break;


    // ========================================================
    // SLEEPING
    // ========================================================

    case POKO_SLEEPING:

      if (
        now - stateStartTime >=
        SLEEP_TIME
      )
      {
        pokoState = POKO_WAKING;

        stateStartTime = now;


        Serial.println(
          "POKO is waking up..."
        );
      }

      break;


    // ========================================================
    // WAKING
    // ========================================================

    case POKO_WAKING:

      if (
        now - stateStartTime >=
        WAKING_TIME
      )
      {
        pokoState = POKO_AWAKE;

        stateStartTime = now;


        // Fully open

        eyeCloseAmount = 0.0;


        // Reset blink system

        blinking = false;

        nextBlinkTime =
          millis() + random(1800, 5000);


        // Start looking around

        chooseLookDirection();


        Serial.println(
          "POKO is awake!"
        );
      }

      break;
  }
}


// ============================================================
// Draw one eye
//
// IMPORTANT:
//
// This function ONLY renders.
//
// It does NOT modify animation state.
// ============================================================

void drawEye(
  int x,
  int y,
  float pupilOffsetX,
  float pupilOffsetY
)
{
  float closeAmount =
    constrain(
      eyeCloseAmount,
      0.0,
      1.0
    );


  // ----------------------------------------------------------
  // Draw white eye
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
  // Calculate pupil position
  // ----------------------------------------------------------

  int pupilCenterX =
    x +
    EYE_W / 2 +
    (int)pupilOffsetX;


  int pupilCenterY =
    y +
    EYE_H / 2 +
    (int)pupilOffsetY;


  // ----------------------------------------------------------
  // Draw pupil
  //
  // Pupil is drawn BEFORE eyelids.
  // ----------------------------------------------------------

  display.setDrawColor(0);

  display.drawDisc(
    pupilCenterX,
    pupilCenterY,
    PUPIL_R
  );


  // ----------------------------------------------------------
  // Draw eyelids
  //
  // The eyelids cover the pupil naturally.
  // ----------------------------------------------------------

  if (closeAmount > 0.0)
  {
    display.setDrawColor(0);


    int coverHeight =
      (int)(
        (EYE_H / 2.0) *
        closeAmount
      );


    if (coverHeight > 0)
    {
      // ------------------------------------------------------
      // Top eyelid
      // ------------------------------------------------------

      display.drawBox(
        x,
        y,
        EYE_W,
        coverHeight
      );


      // ------------------------------------------------------
      // Bottom eyelid
      // ------------------------------------------------------

      display.drawBox(
        x,
        y + EYE_H - coverHeight,
        EYE_W,
        coverHeight
      );
    }
  }


  display.setDrawColor(1);
}


// ============================================================
// Draw Poko's complete face
// ============================================================

void drawFace()
{
  display.clearBuffer();


  // ----------------------------------------------------------
  // Pupil behavior
  //
  // During sleep/wake:
  // keep pupils centered.
  //
  // During awake:
  // use normal eye movement.
  // ----------------------------------------------------------

  float drawPupilX = pupilX;
  float drawPupilY = pupilY;


  if (
    pokoState == POKO_SLEEPY ||
    pokoState == POKO_SLEEPING ||
    pokoState == POKO_WAKING
  )
  {
    drawPupilX = 0;
    drawPupilY = 0;
  }


  // ----------------------------------------------------------
  // Left eye
  // ----------------------------------------------------------

  drawEye(
    LEFT_EYE_X,
    EYE_Y,
    drawPupilX,
    drawPupilY
  );


  // ----------------------------------------------------------
  // Right eye
  // ----------------------------------------------------------

  drawEye(
    RIGHT_EYE_X,
    EYE_Y,
    drawPupilX,
    drawPupilY
  );


  // ----------------------------------------------------------
  // Send frame to OLED
  // ----------------------------------------------------------

  display.sendBuffer();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(300);


  Serial.println();
  Serial.println(
    "=============================="
  );
  Serial.println(
    "          POKO"
  );
  Serial.println(
    "   A tiny life on your desk"
  );
  Serial.println(
    "=============================="
  );


  // ==========================================================
  // I2C
  // ==========================================================

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  // ==========================================================
  // OLED address
  // ==========================================================

  display.setI2CAddress(
    OLED_ADDRESS << 1
  );


  // ==========================================================
  // Start OLED
  // ==========================================================

  display.begin();


  // ==========================================================
  // Random animation seed
  // ==========================================================

  randomSeed(micros());


  // ==========================================================
  // Initial eye position
  // ==========================================================

  pupilX = 0;
  pupilY = 0;

  targetPupilX = 0;
  targetPupilY = 0;


  // ==========================================================
  // Initial timers
  // ==========================================================

  nextBlinkTime =
    millis() + random(1500, 3500);

  nextLookTime =
    millis() + random(1000, 2000);


  // ==========================================================
  // Initial Poko state
  // ==========================================================

  pokoState = POKO_AWAKE;

  stateStartTime = millis();

  eyeCloseAmount = 0.0;


  // ==========================================================
  // Serial information
  // ==========================================================

  Serial.println(
    "OLED initialized."
  );

  Serial.println(
    "POKO eyes starting..."
  );

  Serial.println(
    "Sleep/wake system enabled."
  );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long now = millis();


  // ==========================================================
  // Update Poko state
  // ==========================================================

  updatePokoState();


  // ==========================================================
  // Normal eye movement
  //
  // Only active while awake.
  // ==========================================================

  if (pokoState == POKO_AWAKE)
  {
    // --------------------------------------------------------
    // Choose new direction
    // --------------------------------------------------------

    if (now >= nextLookTime)
    {
      chooseLookDirection();
    }


    // --------------------------------------------------------
    // Smooth X movement
    // --------------------------------------------------------

    pupilX =
      smoothApproach(
        pupilX,
        targetPupilX,
        0.08
      );


    // --------------------------------------------------------
    // Smooth Y movement
    // --------------------------------------------------------

    pupilY =
      smoothApproach(
        pupilY,
        targetPupilY,
        0.08
      );
  }


  // ==========================================================
  // Sleep / wake animation
  //
  // Owns eyeCloseAmount outside awake state.
  // ==========================================================

  updateSleepAnimation();


  // ==========================================================
  // Normal blinking
  //
  // Owns eyeCloseAmount while awake.
  // ==========================================================

  updateBlink();


  // ==========================================================
  // Render at approximately 60 FPS
  // ==========================================================

  if (
    now - lastFrameTime >=
    FRAME_INTERVAL
  )
  {
    lastFrameTime = now;

    drawFace();
  }
}