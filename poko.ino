#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// POKO v0.4.0
// A tiny life on your desk.
//
// Features:
// 👀 Animated eyes
// 😴 Sleep / Wake
// 😊 Happy expression
// 😠 Angry expression
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
// SCREEN
// ============================================================

const int SCREEN_W = 128;
const int SCREEN_H = 64;


// ============================================================
// EYES
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
// EYE MOVEMENT
// ============================================================

float pupilX = 0.0;
float pupilY = 0.0;

float targetPupilX = 0.0;
float targetPupilY = 0.0;

unsigned long nextLookTime = 0;


// ============================================================
// EYELID ANIMATION
// ============================================================

float blinkCloseAmount = 0.0;
float sleepCloseAmount = 0.0;
float eyeCloseAmount   = 0.0;


// ============================================================
// NORMAL BLINK
// ============================================================

bool blinking = false;

unsigned long blinkStartTime = 0;
unsigned long nextBlinkTime = 0;


// ============================================================
// SLEEP / WAKE STATE
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
// ============================================================

const unsigned long AWAKE_TIME  = 15000;
const unsigned long SLEEPY_TIME = 2500;
const unsigned long SLEEP_TIME  = 10000;
const unsigned long WAKING_TIME = 2000;


// ============================================================
// FRAME TIMING
// ============================================================

unsigned long lastFrameTime = 0;

const unsigned long FRAME_INTERVAL = 16;


// ============================================================
// DEBUG
// ============================================================

unsigned long lastDebugPrint = 0;

const unsigned long DEBUG_INTERVAL = 250;


// ============================================================
// EXPRESSIONS
//
// IMPORTANT:
// These declarations are BEFORE updatePokoState().
// ============================================================

bool happyExpression = false;
bool angryExpression = false;

unsigned long happyStartTime = 0;
unsigned long nextHappyTime = 0;

unsigned long angryStartTime = 0;
unsigned long nextAngryTime = 0;

const unsigned long HAPPY_DURATION = 5000;
const unsigned long ANGRY_DURATION = 5000;

// ============================================================
// FACIAL EXPRESSION TRANSITION
//
// 0.0 = normal face
// 1.0 = full expression
// ============================================================

float happyTransition = 0.0f;
float angryTransition = 0.0f;

bool happyLeaving = false;
bool angryLeaving = false;

const float EXPRESSION_TRANSITION_SPEED = 0.015f;


// ============================================================
// SMOOTH MOVEMENT
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
// SMOOTH STEP
// ============================================================

float smoothStep(float value)
{
  value = constrain(value, 0.0f, 1.0f);

  return value * value * (3.0f - 2.0f * value);
}


// ============================================================
// CHOOSE LOOK DIRECTION
// ============================================================

void chooseLookDirection()
{
  int direction = random(0, 5);

  switch (direction)
  {
    case 0:

      targetPupilX = 0;
      targetPupilY = 0;

      break;


    case 1:

      targetPupilX = -MAX_PUPIL_X;
      targetPupilY = random(-3, 4);

      break;


    case 2:

      targetPupilX = MAX_PUPIL_X;
      targetPupilY = random(-3, 4);

      break;


    case 3:

      targetPupilX = random(-5, 6);
      targetPupilY = -MAX_PUPIL_Y;

      break;


    case 4:

      targetPupilX = random(-5, 6);
      targetPupilY = MAX_PUPIL_Y;

      break;
  }

  nextLookTime =
    millis() + random(1200, 3000);
}


// ============================================================
// START BLINK
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
// UPDATE BLINK
// ============================================================

// ============================================================
// UPDATE BLINK
//
// POKO blinks ONLY during the normal AWAKE state.
// Happy and Angry expressions never blink.
// Sleep/Wake uses the existing sleep animation.
// ============================================================

void updateBlink()
{
  // ----------------------------------------------------------
  // Blink is allowed ONLY in normal AWAKE state.
  // ----------------------------------------------------------

  if (
    pokoState != POKO_AWAKE ||
    happyExpression ||
    angryExpression ||
    happyLeaving ||
    angryLeaving
  )
  {
    blinkCloseAmount = 0.0;
    blinking = false;

    return;
  }


  if (!blinking)
  {
    blinkCloseAmount = 0.0;

    if (millis() >= nextBlinkTime)
    {
      blinking = true;
      blinkStartTime = millis();
    }

    return;
  }


  const unsigned long CLOSE_TIME  = 100;
  const unsigned long CLOSED_TIME = 70;
  const unsigned long OPEN_TIME   = 140;

  unsigned long elapsed =
    millis() - blinkStartTime;


  if (elapsed < CLOSE_TIME)
  {
    blinkCloseAmount =
      (float)elapsed /
      (float)CLOSE_TIME;
  }


  else if (
    elapsed <
    CLOSE_TIME + CLOSED_TIME
  )
  {
    blinkCloseAmount = 1.0;
  }


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

    blinkCloseAmount =
      1.0 -
      (
        (float)openElapsed /
        (float)OPEN_TIME
      );
  }


  else
  {
    blinkCloseAmount = 0.0;

    blinking = false;

    nextBlinkTime =
      millis() + random(1800, 5000);
  }


  blinkCloseAmount =
    constrain(
      blinkCloseAmount,
      0.0f,
      1.0f
    );
}


// ============================================================
// GET SLEEP AMOUNT
// ============================================================

float getSleepAmount()
{
  unsigned long elapsed =
    millis() - stateStartTime;


  if (pokoState == POKO_SLEEPY)
  {
    float progress =
      (float)elapsed /
      (float)SLEEPY_TIME;

    progress =
      constrain(
        progress,
        0.0f,
        1.0f
      );

    return smoothStep(progress);
  }


  if (pokoState == POKO_SLEEPING)
  {
    return 1.0;
  }


  if (pokoState == POKO_WAKING)
  {
    float progress =
      (float)elapsed /
      (float)WAKING_TIME;

    progress =
      constrain(
        progress,
        0.0f,
        1.0f
      );

    return 1.0f - smoothStep(progress);
  }


  return 0.0;
}


// ============================================================
// UPDATE SLEEP ANIMATION
// ============================================================

void updateSleepAnimation()
{
  if (pokoState == POKO_AWAKE)
  {
    sleepCloseAmount = 0.0;

    return;
  }

  sleepCloseAmount =
    getSleepAmount();

  sleepCloseAmount =
    constrain(
      sleepCloseAmount,
      0.0f,
      1.0f
    );
}


// ============================================================
// COMBINE EYELID ANIMATIONS
// ============================================================

void updateEyeCloseAmount()
{
  if (pokoState == POKO_AWAKE)
  {
    eyeCloseAmount = blinkCloseAmount;
  }
  else
  {
    eyeCloseAmount = sleepCloseAmount;
  }

  eyeCloseAmount =
    constrain(
      eyeCloseAmount,
      0.0f,
      1.0f
    );
}


// ============================================================
// STATE NAME
// ============================================================

const char* getStateName(PokoState state)
{
  switch (state)
  {
    case POKO_AWAKE:
      return "AWAKE";

    case POKO_SLEEPY:
      return "SLEEPY";

    case POKO_SLEEPING:
      return "SLEEPING";

    case POKO_WAKING:
      return "WAKING";

    default:
      return "UNKNOWN";
  }
}


// ============================================================
// DEBUG STATUS
// ============================================================

void printDebugStatus()
{
  unsigned long now = millis();

  if (now - lastDebugPrint >= DEBUG_INTERVAL)
  {
    lastDebugPrint = now;

    Serial.print("STATE: ");
    Serial.print(getStateName(pokoState));

    Serial.print(" | blinkCloseAmount: ");
    Serial.print(blinkCloseAmount, 3);

    Serial.print(" | sleepCloseAmount: ");
    Serial.print(sleepCloseAmount, 3);

    Serial.print(" | eyeCloseAmount: ");
    Serial.println(eyeCloseAmount, 3);
  }
}


// ============================================================
// UPDATE POKO STATE
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

        blinking = false;
        blinkCloseAmount = 0.0;

        // Expressions are awake-only.
        happyExpression = false;
        angryExpression = false;

        sleepCloseAmount = 0.0;
        eyeCloseAmount = 0.0;

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

        sleepCloseAmount = 1.0;
        eyeCloseAmount = 1.0;

        Serial.println(
          "POKO is sleeping..."
        );
      }

      break;


    // ========================================================
    // SLEEPING
    // ========================================================

    case POKO_SLEEPING:

      sleepCloseAmount = 1.0;
      eyeCloseAmount = 1.0;

      if (
        now - stateStartTime >=
        SLEEP_TIME
      )
      {
        pokoState = POKO_WAKING;

        stateStartTime = now;

        sleepCloseAmount = 1.0;
        eyeCloseAmount = 1.0;

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

        sleepCloseAmount = 0.0;
        blinkCloseAmount = 0.0;
        eyeCloseAmount = 0.0;

        blinking = false;

        nextBlinkTime =
          millis() + random(1800, 5000);

        chooseLookDirection();

        // Start fresh expression timers.
        nextHappyTime =
          millis() + random(7000, 14000);

        nextAngryTime =
          millis() + random(7000, 14000);

        Serial.println(
          "POKO is awake!"
        );
      }

      break;
  }
}


// ============================================================
// START HAPPY
// ============================================================

void startHappyExpression()
{
  if (pokoState != POKO_AWAKE)
  {
    return;
  }

  // Happy and angry are mutually exclusive.
  angryExpression = false;
  angryLeaving = false;
  angryTransition = 0.0f;

  happyExpression = true;
  happyLeaving = false;

  happyStartTime = millis();

  Serial.println(
    "POKO is happy!"
  );
}


// ============================================================
// UPDATE HAPPY
// ============================================================

void updateHappyExpression()
{
  unsigned long now = millis();

  if (pokoState != POKO_AWAKE)
  {
    happyExpression = false;
    happyLeaving = false;

    happyTransition =
      smoothApproach(
        happyTransition,
        0.0f,
        EXPRESSION_TRANSITION_SPEED
      );

    return;
  }


  // ----------------------------------------------------------
  // HAPPY ENTERING / HOLDING
  // ----------------------------------------------------------

  if (
    happyExpression &&
    !happyLeaving
  )
  {
    // Slowly change from normal -> existing Happy face.

    happyTransition =
      smoothApproach(
        happyTransition,
        1.0f,
        EXPRESSION_TRANSITION_SPEED
      );


    if (
      now - happyStartTime >=
      HAPPY_DURATION
    )
    {
      happyLeaving = true;

      nextHappyTime =
        now + random(7000, 14000);
    }

    return;
  }


  // ----------------------------------------------------------
  // HAPPY -> NORMAL
  // ----------------------------------------------------------

  if (happyLeaving)
  {
    happyTransition =
      smoothApproach(
        happyTransition,
        0.0f,
        EXPRESSION_TRANSITION_SPEED
      );


    if (
      happyTransition <= 0.001f
    )
    {
      happyTransition = 0.0f;

      happyExpression = false;
      happyLeaving = false;
    }

    return;
  }


  // ----------------------------------------------------------
  // DON'T START HAPPY WHILE ANGRY IS ACTIVE/LEAVING
  // ----------------------------------------------------------

  if (
    angryExpression ||
    angryLeaving
  )
  {
    return;
  }


  // ----------------------------------------------------------
  // START HAPPY
  // ----------------------------------------------------------

  if (now >= nextHappyTime)
  {
    startHappyExpression();
  }
}


// ============================================================
// START ANGRY
// ============================================================

void startAngryExpression()
{
  if (pokoState != POKO_AWAKE)
  {
    return;
  }

  // Happy and angry are mutually exclusive.
  happyExpression = false;
  happyLeaving = false;
  happyTransition = 0.0f;

  angryExpression = true;
  angryLeaving = false;

  angryStartTime = millis();

  Serial.println(
    "POKO is angry!"
  );
}


// ============================================================
// UPDATE ANGRY
// ============================================================

void updateAngryExpression()
{
  unsigned long now = millis();

  if (pokoState != POKO_AWAKE)
  {
    angryExpression = false;
    angryLeaving = false;

    angryTransition =
      smoothApproach(
        angryTransition,
        0.0f,
        EXPRESSION_TRANSITION_SPEED
      );

    return;
  }


  // ----------------------------------------------------------
  // ANGRY ENTERING / HOLDING
  // ----------------------------------------------------------

  if (
    angryExpression &&
    !angryLeaving
  )
  {
    // Slowly change from normal -> existing Angry face.

    angryTransition =
      smoothApproach(
        angryTransition,
        1.0f,
        EXPRESSION_TRANSITION_SPEED
      );


    if (
      now - angryStartTime >=
      ANGRY_DURATION
    )
    {
      angryLeaving = true;

      nextAngryTime =
        now + random(7000, 14000);
    }

    return;
  }


  // ----------------------------------------------------------
  // ANGRY -> NORMAL
  // ----------------------------------------------------------

  if (angryLeaving)
  {
    angryTransition =
      smoothApproach(
        angryTransition,
        0.0f,
        EXPRESSION_TRANSITION_SPEED
      );


    if (
      angryTransition <= 0.001f
    )
    {
      angryTransition = 0.0f;

      angryExpression = false;
      angryLeaving = false;
    }

    return;
  }


  // ----------------------------------------------------------
  // DON'T START ANGRY WHILE HAPPY IS ACTIVE/LEAVING
  // ----------------------------------------------------------

  if (
    happyExpression ||
    happyLeaving
  )
  {
    return;
  }


  // ----------------------------------------------------------
  // START ANGRY
  // ----------------------------------------------------------

  if (now >= nextAngryTime)
  {
    startAngryExpression();
  }
}


// ============================================================
// DRAW NORMAL EYE
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
      0.0f,
      1.0f
    );


  // White eye
  display.setDrawColor(1);

  display.drawRBox(
    x,
    y,
    EYE_W,
    EYE_H,
    8
  );


  // Pupil position
  int pupilCenterX =
    x +
    EYE_W / 2 +
    (int)pupilOffsetX;

  int pupilCenterY =
    y +
    EYE_H / 2 +
    (int)pupilOffsetY;


  // Pupil
  display.setDrawColor(0);

  display.drawDisc(
    pupilCenterX,
    pupilCenterY,
    PUPIL_R
  );


  // Eyelids
  if (closeAmount > 0.0)
  {
    display.setDrawColor(0);

    int coverHeight =
      (int)(
        (EYE_H / 2.0f) *
        closeAmount
      );


    if (coverHeight > 0)
    {
      // Top eyelid
      display.drawBox(
        x,
        y,
        EYE_W,
        coverHeight
      );

      // Bottom eyelid
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
// DRAW HAPPY EYE
// ============================================================

void drawHappyEye(
  int x,
  int y
)
{
  display.setDrawColor(1);

  // Same large POKO eye.
  display.drawRBox(
    x,
    y,
    EYE_W,
    EYE_H,
    8
  );

  display.setDrawColor(0);

  // Curved closed happy eye.
  display.drawLine(
    x + 13, y + 20,
    x + 17, y + 16
  );

  display.drawLine(
    x + 17, y + 16,
    x + 22, y + 13
  );

  display.drawLine(
    x + 22, y + 13,
    x + 26, y + 12
  );

  display.drawLine(
    x + 26, y + 12,
    x + 30, y + 13
  );

  display.drawLine(
    x + 30, y + 13,
    x + 35, y + 16
  );

  display.drawLine(
    x + 35, y + 16,
    x + 39, y + 20
  );

  display.setDrawColor(1);
}


// ============================================================
// DRAW HAPPY SMILE
// ============================================================

void drawHappySmile()
{
  if (!happyExpression)
  {
    return;
  }

  display.setDrawColor(1);

  display.drawLine(53, 52, 56, 55);
  display.drawLine(56, 55, 60, 57);
  display.drawLine(60, 57, 64, 58);
  display.drawLine(64, 58, 68, 57);
  display.drawLine(68, 57, 72, 55);
  display.drawLine(72, 55, 75, 52);

  display.drawLine(56, 55, 59, 59);
  display.drawLine(59, 59, 62, 61);
  display.drawLine(62, 61, 66, 61);
  display.drawLine(66, 61, 69, 59);
  display.drawLine(69, 59, 72, 55);

  display.setDrawColor(1);
}


// ============================================================
// DRAW STRONGLY ANGRY EYE
//
// Strongly slanted eye shape.
// ============================================================

void drawAngryEye(
  int x,
  int y,
  float pupilOffsetX,
  float pupilOffsetY,
  bool leftEye
)
{
  display.setDrawColor(1);

  // Base POKO eye.
  display.drawRBox(
    x,
    y,
    EYE_W,
    EYE_H,
    8
  );


  // ----------------------------------------------------------
  // Strong angry slant
  // ----------------------------------------------------------

  display.setDrawColor(0);

  if (leftEye)
  {
    // Left eye:
    // outer side high
    // inner side low

    display.drawTriangle(
      x + 18,
      y,

      x + EYE_W,
      y,

      x + EYE_W,
      y + 13
    );
  }
  else
  {
    // Right eye:
    // inner side low
    // outer side high

    display.drawTriangle(
      x,
      y,

      x + EYE_W - 18,
      y,

      x,
      y + 13
    );
  }


  // ----------------------------------------------------------
  // Pupil
  // ----------------------------------------------------------

  int pupilCenterX =
    x +
    EYE_W / 2 +
    (int)pupilOffsetX;

  int pupilCenterY =
    y +
    EYE_H / 2 +
    (int)pupilOffsetY;

  display.setDrawColor(0);

  display.drawDisc(
    pupilCenterX,
    pupilCenterY,
    PUPIL_R
  );

  display.setDrawColor(1);
}


// ============================================================
// DRAW COMPLETE FACE
// ============================================================

void drawFace()
{
  display.clearBuffer();


  // ----------------------------------------------------------
  // Pupil behavior
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
  // HAPPY
  //
  // Original Happy drawing functions are unchanged.
  // ----------------------------------------------------------

  if (
    pokoState == POKO_AWAKE &&
    eyeCloseAmount <= 0.01f &&
    happyExpression &&
    happyTransition >= 0.5f
  )
  {
    drawHappyEye(
      LEFT_EYE_X,
      EYE_Y
    );

    drawHappyEye(
      RIGHT_EYE_X,
      EYE_Y
    );

    drawHappySmile();
  }


  // ----------------------------------------------------------
  // ANGRY
  //
  // Original Angry drawing function is unchanged.
  // ----------------------------------------------------------

  else if (
    pokoState == POKO_AWAKE &&
    eyeCloseAmount <= 0.01f &&
    angryExpression &&
    angryTransition >= 0.5f
  )
  {
    drawAngryEye(
      LEFT_EYE_X,
      EYE_Y,
      drawPupilX,
      drawPupilY,
      true
    );

    drawAngryEye(
      RIGHT_EYE_X,
      EYE_Y,
      drawPupilX,
      drawPupilY,
      false
    );
  }


  // ----------------------------------------------------------
  // NORMAL
  //
  // Existing normal face remains unchanged.
  // ----------------------------------------------------------

  else
  {
    drawEye(
      LEFT_EYE_X,
      EYE_Y,
      drawPupilX,
      drawPupilY
    );

    drawEye(
      RIGHT_EYE_X,
      EYE_Y,
      drawPupilX,
      drawPupilY
    );
  }


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


  // Random seed
  randomSeed(micros());


  // Initial eye position
  pupilX = 0;
  pupilY = 0;

  targetPupilX = 0;
  targetPupilY = 0;


  // Initial timers
  nextBlinkTime =
    millis() + random(1500, 3500);

  nextLookTime =
    millis() + random(1000, 2000);


  // Initial expression timers
  nextHappyTime =
    millis() + random(7000, 14000);

  nextAngryTime =
    millis() + random(7000, 14000);


  // Initial state
  pokoState = POKO_AWAKE;

  stateStartTime = millis();


  // Initial eyelids
  blinkCloseAmount = 0.0;
  sleepCloseAmount = 0.0;
  eyeCloseAmount   = 0.0;


  // Initial expressions
  happyExpression = false;
  angryExpression = false;


  // Debug
  lastDebugPrint = millis();


  Serial.println(
    "OLED initialized."
  );

  Serial.println(
    "POKO eyes starting..."
  );

  Serial.println(
    "Sleep/wake system enabled."
  );

  Serial.println(
    "Happy expression enabled."
  );

  Serial.println(
    "Angry expression enabled."
  );

  Serial.println(
    "Temporary debug logging enabled."
  );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long now = millis();


  // Update Poko state
  updatePokoState();


  // ----------------------------------------------------------
  // Normal eye movement
  // ----------------------------------------------------------

  if (pokoState == POKO_AWAKE)
  {
    if (now >= nextLookTime)
    {
      chooseLookDirection();
    }

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
  }


  // Sleep / wake
  updateSleepAnimation();


  // Normal blink
  updateBlink();


  // Final eyelid amount
  updateEyeCloseAmount();


  // Happy
  updateHappyExpression();


  // Angry
  updateAngryExpression();


  // Debug
  printDebugStatus();


  // Render approximately 60 FPS
  if (
    now - lastFrameTime >=
    FRAME_INTERVAL
  )
  {
    lastFrameTime = now;

    drawFace();
  }
}