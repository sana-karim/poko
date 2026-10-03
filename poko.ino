#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// POKO
// A tiny life on your desk.
// ============================================================
//
// Features:
// 👀 Animated eyes
// 😴 Sleep / Wake
// 😊 Happy expression
// 😠 Angry expression
// 👆 Touch reactions
//
// ============================================================

// ============================================================
// OLED
// ============================================================

#define OLED_SDA 8
#define OLED_SCL 9
#define OLED_ADDRESS 0x3C

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,
    U8X8_PIN_NONE);

// ============================================================
// SCREEN
// ============================================================

const int SCREEN_W = 128;
const int SCREEN_H = 64;

// ============================================================
// EYES
// ============================================================

const int LEFT_EYE_X = 8;
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
float eyeCloseAmount = 0.0;

// ============================================================
// NORMAL BLINK
// ============================================================

// POKO blinks ONLY during normal AWAKE.
// Happy and Angry never blink.

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
// INTERACTION / AWAKE TIMER
// ============================================================

// Any interaction with POKO resets the awake timer.
// While the user is continuously interacting,
// POKO will not automatically fall asleep.

unsigned long lastInteractionTime = 0;

// ============================================================
// POKO TIMINGS
// ============================================================

const unsigned long AWAKE_TIME = 50000; // 50 sec
const unsigned long SLEEPY_TIME = 4000; // 4 sec
const unsigned long SLEEP_TIME = 50000; // 50 sec
const unsigned long WAKING_TIME = 2800; // 2.8 sec

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
// TOUCH SENSOR
// ============================================================
//
// TTP223:
//
// VCC -> ESP32-S3 3V3
// GND -> ESP32-S3 GND
// I/O -> ESP32-S3 GPIO 1
//
// ============================================================

#define TOUCH_PIN 1

bool touchState = false;
bool lastTouchState = false;

unsigned long lastTouchChangeTime = 0;

const unsigned long TOUCH_DEBOUNCE = 50;

// ============================================================
// TOUCH REACTION
// ============================================================

unsigned long touchStartTime = 0;
unsigned long touchReleaseTime = 0;

bool longTouchDetected = false;

bool touchHoldActive = false;

bool touchWakeReaction = false;

// ============================================================
// REPEATED TOUCH DETECTION
// ============================================================

int touchCount = 0;

unsigned long firstTouchTime = 0;

const unsigned long REPEAT_WINDOW = 6000;

// 3 touches = stronger happiness
const int EXCITED_TOUCH_COUNT = 3;

// 5 touches = angry
const int ANGRY_TOUCH_COUNT = 5;

// ============================================================
// LONG TOUCH
// ============================================================

const unsigned long LONG_TOUCH_TIME = 1000;

// ============================================================
// EXPRESSIONS
// ============================================================

bool happyExpression = false;
bool angryExpression = false;

unsigned long happyStartTime = 0;
unsigned long angryStartTime = 0;

unsigned long currentHappyDuration = 5000;

const unsigned long NORMAL_HAPPY_DURATION = 5000;
const unsigned long EXCITED_HAPPY_DURATION = 7000;

const unsigned long ANGRY_DURATION = 3500;

// ============================================================
// FACIAL EXPRESSION TRANSITION
// ============================================================
//
// 0.0 = normal
// 1.0 = full expression
//
// ============================================================

float happyTransition = 0.0f;
float angryTransition = 0.0f;

bool happyLeaving = false;
bool angryLeaving = false;

const float EXPRESSION_TRANSITION_SPEED = 0.08f;

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void resetAwakeTimer();
void wakePokoFromTouch();

// ============================================================
// SMOOTH MOVEMENT
// ============================================================

float smoothApproach(
    float current,
    float target,
    float speed)
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

  nextLookTime = millis() + random(1200, 3000);
}

// ============================================================
// UPDATE BLINK
// ============================================================
//
// POKO blinks ONLY during normal AWAKE.
//
// Happy and Angry never blink.
//
// ============================================================

void updateBlink()
{
  if (
      pokoState != POKO_AWAKE ||
      happyExpression ||
      angryExpression ||
      happyLeaving ||
      angryLeaving)
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

  const unsigned long CLOSE_TIME = 100;
  const unsigned long CLOSED_TIME = 70;
  const unsigned long OPEN_TIME = 140;

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
      CLOSE_TIME + CLOSED_TIME)
  {
    blinkCloseAmount = 1.0;
  }
  else if (
      elapsed <
      CLOSE_TIME +
          CLOSED_TIME +
          OPEN_TIME)
  {
    unsigned long openElapsed =
        elapsed -
        CLOSE_TIME -
        CLOSED_TIME;

    blinkCloseAmount =
        1.0 -
        ((float)openElapsed /
         (float)OPEN_TIME);
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
          1.0f);
}

// ============================================================
// START HAPPY
// ============================================================

void startHappyExpression(
    unsigned long duration = NORMAL_HAPPY_DURATION)
{
  if (pokoState != POKO_AWAKE)
  {
    return;
  }

  // Cancel Angry only when explicitly starting Happy.
  angryExpression = false;
  angryLeaving = false;
  angryTransition = 0.0f;

  happyExpression = true;
  happyLeaving = false;

  happyStartTime = millis();

  currentHappyDuration = duration;

  Serial.println("POKO is happy!");
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

  // Cancel Happy.
  happyExpression = false;
  happyLeaving = false;
  happyTransition = 0.0f;

  // Start Angry.
  angryExpression = true;
  angryLeaving = false;

  angryStartTime = millis();

  Serial.println("POKO is angry!");
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
            EXPRESSION_TRANSITION_SPEED);

    return;
  }

  // ----------------------------------------------------------
  // HAPPY ENTERING / HOLDING
  // ----------------------------------------------------------

  if (
      happyExpression &&
      !happyLeaving)
  {
    happyTransition =
        smoothApproach(
            happyTransition,
            1.0f,
            EXPRESSION_TRANSITION_SPEED);

    // If touch is being held,
    // keep Happy alive.

    if (touchState)
    {
      happyStartTime = now;
      return;
    }

    // Normal timeout.

    if (
        now - happyStartTime >=
        currentHappyDuration)
    {
      happyLeaving = true;
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
            EXPRESSION_TRANSITION_SPEED);

    if (happyTransition <= 0.001f)
    {
      happyTransition = 0.0f;

      happyExpression = false;
      happyLeaving = false;
    }

    return;
  }
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
            EXPRESSION_TRANSITION_SPEED);

    return;
  }

  // ----------------------------------------------------------
  // ANGRY ENTERING / HOLDING
  // ----------------------------------------------------------

  if (
      angryExpression &&
      !angryLeaving)
  {
    angryTransition =
        smoothApproach(
            angryTransition,
            1.0f,
            EXPRESSION_TRANSITION_SPEED);

    if (
        now - angryStartTime >=
        ANGRY_DURATION)
    {
      angryLeaving = true;
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
            EXPRESSION_TRANSITION_SPEED);

    if (angryTransition <= 0.001f)
    {
      angryTransition = 0.0f;

      angryExpression = false;
      angryLeaving = false;
    }

    return;
  }
}

// ============================================================
// WAKE POKO FROM TOUCH
// ============================================================

void wakePokoFromTouch()
{
  if (
      pokoState != POKO_SLEEPY &&
      pokoState != POKO_SLEEPING)
  {
    return;
  }

  unsigned long now = millis();

  // Start waking animation.
  pokoState = POKO_WAKING;
  stateStartTime = now;

  // Start fully closed.
  sleepCloseAmount = 1.0f;
  eyeCloseAmount = 1.0f;

  // Make sure normal blink does not interfere.
  blinking = false;
  blinkCloseAmount = 0.0f;

  // Clear expressions.
  happyExpression = false;
  happyLeaving = false;
  happyTransition = 0.0f;

  angryExpression = false;
  angryLeaving = false;
  angryTransition = 0.0f;

  // Remember that the user woke POKO by touch.
  touchWakeReaction = true;

  Serial.println("POKO woke up because of touch!");
}

// ============================================================
// UPDATE TOUCH
// ============================================================
//
// Touch behavior:
//
// 1 tap       -> Happy
// 2 taps      -> Happy
// 3-4 taps   -> Extended Happy
// 5+ taps     -> Angry
//
// Holding     -> Happy while held
//
// Any interaction resets the awake timer.
//
// ============================================================

void updateTouch()
{
  unsigned long now = millis();

  bool currentTouch =
      digitalRead(TOUCH_PIN) == HIGH;

  // ----------------------------------------------------------
  // RAW STATE CHANGED
  // ----------------------------------------------------------

  if (currentTouch != lastTouchState)
  {
    lastTouchChangeTime = now;
    lastTouchState = currentTouch;
  }

  // ----------------------------------------------------------
  // DEBOUNCE
  // ----------------------------------------------------------

  if (
      now - lastTouchChangeTime <
      TOUCH_DEBOUNCE)
  {
    return;
  }

  // ==========================================================
  // TOUCH STATE CHANGED
  // ==========================================================

  if (currentTouch != touchState)
  {
    touchState = currentTouch;

    // ========================================================
    // TOUCH START
    // ========================================================

    if (touchState)
    {
      touchStartTime = now;

      longTouchDetected = false;
      touchHoldActive = false;

      // ------------------------------------------------------
      // RESET AWAKE TIMER
      // ------------------------------------------------------

      if (pokoState == POKO_AWAKE)
      {
        resetAwakeTimer();
      }

      // ------------------------------------------------------
      // COUNT RAPID TOUCHES
      // ------------------------------------------------------

      if (
          touchCount == 0 ||
          now - firstTouchTime > REPEAT_WINDOW)
      {
        touchCount = 1;
        firstTouchTime = now;
      }
      else
      {
        touchCount++;
      }

      Serial.print("TOUCH START | Count: ");
      Serial.println(touchCount);

      // ------------------------------------------------------
      // SLEEPY / SLEEPING
      // ------------------------------------------------------

      if (
          pokoState == POKO_SLEEPY ||
          pokoState == POKO_SLEEPING)
      {
        wakePokoFromTouch();

        return;
      }

      // ------------------------------------------------------
      // WAKING
      // ------------------------------------------------------

      if (pokoState == POKO_WAKING)
      {
        touchWakeReaction = true;

        return;
      }

      // ------------------------------------------------------
      // ANGRY HAS PRIORITY
      // ------------------------------------------------------
      //
      // Once POKO becomes angry, do not start Happy from
      // subsequent releases/taps during this interaction.
      //

      if (
          pokoState == POKO_AWAKE &&
          touchCount >= ANGRY_TOUCH_COUNT)
      {
        startAngryExpression();

        Serial.println(
            "POKO: Too much poking! 😠");

        // Reset sequence.
        touchCount = 0;
        firstTouchTime = 0;

        return;
      }

      // ------------------------------------------------------
      // 3-4 RAPID TOUCHES = EXCITED HAPPY
      // ------------------------------------------------------

      if (
          pokoState == POKO_AWAKE &&
          touchCount >= EXCITED_TOUCH_COUNT &&
          touchCount < ANGRY_TOUCH_COUNT)
      {
        startHappyExpression(
            EXCITED_HAPPY_DURATION);

        Serial.println(
            "POKO is excited!");

        return;
      }
    }

    // ========================================================
    // TOUCH RELEASE
    // ========================================================

    else
    {
      touchReleaseTime = now;

      unsigned long touchDuration =
          now - touchStartTime;

      // ------------------------------------------------------
      // ANGRY HAS ABSOLUTE PRIORITY
      // ------------------------------------------------------

      if (angryExpression || angryLeaving)
      {
        Serial.println(
            "POKO is still angry.");

        // IMPORTANT:
        // Do not trigger Happy here.

        return;
      }

      // ------------------------------------------------------
      // LONG HOLD RELEASE
      // ------------------------------------------------------

      if (longTouchDetected)
      {
        Serial.print(
            "POKO was petted for ");

        Serial.print(
            touchDuration);

        Serial.println(
            " ms");

        if (
            pokoState == POKO_AWAKE)
        {
          // Give POKO another full awake period
          // after interaction ends.

          resetAwakeTimer();

          // A completed hold ends the heart reaction directly.
          // Do NOT show Happy on release.
          happyExpression = false;
          happyLeaving = false;
          happyTransition = 0.0f;

          Serial.println(
              "POKO is calming down...");
        }

        touchHoldActive = false;

        return;
      }

      // ------------------------------------------------------
      // RAPID TOUCH RELEASE
      // ------------------------------------------------------

      bool repeatedTouch =
          touchCount >= EXCITED_TOUCH_COUNT &&
          now - firstTouchTime <= REPEAT_WINDOW;

      if (repeatedTouch)
      {
        Serial.println(
            "POKO detected repeated interaction.");

        // IMPORTANT:
        //
        // DO NOT reset touchCount here.
        //
        // We need to continue counting:
        //
        // 3 taps -> Happy
        // 4 taps -> Happy
        // 5 taps -> Angry
        //
        // The counter is reset only when:
        // - 5 taps trigger Angry
        // - the repeat window expires

        if (
            pokoState == POKO_AWAKE &&
            !angryExpression &&
            !angryLeaving)
        {
          startHappyExpression(
              EXCITED_HAPPY_DURATION);
        }

        return;
      }

      // ------------------------------------------------------
      // SINGLE TAP
      // ------------------------------------------------------

      if (
          pokoState == POKO_AWAKE &&
          !angryExpression &&
          !angryLeaving)
      {
        resetAwakeTimer();

        startHappyExpression(
            NORMAL_HAPPY_DURATION);
      }
    }
  }

  // ==========================================================
  // LONG HOLD / PETTING
  // ==========================================================

  if (
      touchState &&
      pokoState == POKO_AWAKE &&
      !angryExpression &&
      !angryLeaving)
  {
    // --------------------------------------------------------
    // Continuous interaction keeps POKO awake.
    // --------------------------------------------------------

    stateStartTime = now;
    lastInteractionTime = now;

    // --------------------------------------------------------
    // Detect long hold.
    // --------------------------------------------------------

    if (
        !longTouchDetected &&
        now - touchStartTime >=
            LONG_TOUCH_TIME)
    {
      longTouchDetected = true;
      touchHoldActive = true;

      Serial.println(
          "POKO is being petted...");

      startHappyExpression(
          NORMAL_HAPPY_DURATION);
    }

    // --------------------------------------------------------
    // Keep Happy alive while finger remains on POKO.
    // --------------------------------------------------------

    if (
        touchHoldActive &&
        happyExpression)
    {
      happyStartTime = now;
      happyLeaving = false;
    }
  }
}

// ============================================================
// RESET AWAKE TIMER
// ============================================================

void resetAwakeTimer()
{
  if (pokoState == POKO_AWAKE)
  {
    stateStartTime = millis();

    lastInteractionTime = stateStartTime;

    Serial.println(
        "POKO awake timer reset.");
  }
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
            1.0f);

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
            1.0f);

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
          1.0f);
}

// ============================================================
// COMBINE EYELID ANIMATIONS
// ============================================================

void updateEyeCloseAmount()
{
  if (pokoState == POKO_AWAKE)
  {
    eyeCloseAmount =
        blinkCloseAmount;
  }
  else
  {
    eyeCloseAmount =
        sleepCloseAmount;
  }

  eyeCloseAmount =
      constrain(
          eyeCloseAmount,
          0.0f,
          1.0f);
}

// ============================================================
// STATE NAME
// ============================================================

const char *getStateName(PokoState state)
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

  if (
      now - lastDebugPrint >=
      DEBUG_INTERVAL)
  {
    lastDebugPrint = now;

    Serial.print("STATE: ");
    Serial.print(getStateName(pokoState));

    Serial.print(" | TOUCH: ");
    Serial.print(touchState ? "YES" : "NO");

    Serial.print(" | HAPPY: ");
    Serial.print(happyExpression ? "YES" : "NO");

    Serial.print(" | ANGRY: ");
    Serial.print(angryExpression ? "YES" : "NO");

    Serial.print(" | touchCount: ");
    Serial.print(touchCount);

    Serial.print(" | eyeClose: ");
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
        AWAKE_TIME)
    {
      pokoState = POKO_SLEEPY;

      stateStartTime = now;

      blinking = false;
      blinkCloseAmount = 0.0;

      // Expressions end before sleeping.

      happyExpression = false;
      happyLeaving = false;
      happyTransition = 0.0f;

      angryExpression = false;
      angryLeaving = false;
      angryTransition = 0.0f;

      sleepCloseAmount = 0.0;
      eyeCloseAmount = 0.0;

      targetPupilX = 0;
      targetPupilY = 0;

      pupilX = 0;
      pupilY = 0;

      Serial.println(
          "POKO is getting sleepy...");
    }

    break;

    // ========================================================
    // SLEEPY
    // ========================================================

  case POKO_SLEEPY:

    if (
        now - stateStartTime >=
        SLEEPY_TIME)
    {
      pokoState = POKO_SLEEPING;

      stateStartTime = now;

      sleepCloseAmount = 1.0;
      eyeCloseAmount = 1.0;

      Serial.println(
          "POKO is sleeping...");
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
        SLEEP_TIME)
    {
      pokoState = POKO_WAKING;

      stateStartTime = now;

      sleepCloseAmount = 1.0;
      eyeCloseAmount = 1.0;

      touchWakeReaction = false;

      Serial.println(
          "POKO is waking up...");
    }

    break;

    // ========================================================
    // WAKING
    // ========================================================

  case POKO_WAKING:

    if (
        now - stateStartTime >=
        WAKING_TIME)
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

      // ----------------------------------------------------
      // If user touched POKO while sleeping and is STILL
      // touching it after the wake animation:
      //
      // Waking -> Happy
      // ----------------------------------------------------

      if (touchWakeReaction)
      {
        touchWakeReaction = false;

        if (touchState)
        {
          startHappyExpression(
              NORMAL_HAPPY_DURATION);

          happyStartTime = millis();
          happyLeaving = false;

          Serial.println(
              "POKO is happy to see you!");
        }
      }

      Serial.println(
          "POKO is awake!");
    }

    break;
  }
}

// ============================================================
// DRAW NORMAL EYE
// ============================================================

void drawEye(
    int x,
    int y,
    float pupilOffsetX,
    float pupilOffsetY)
{
  float closeAmount =
      constrain(
          eyeCloseAmount,
          0.0f,
          1.0f);

  // White eye

  display.setDrawColor(1);

  display.drawRBox(
      x,
      y,
      EYE_W,
      EYE_H,
      8);

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
      PUPIL_R);

  // Eyelids

  if (closeAmount > 0.0)
  {
    display.setDrawColor(0);

    int coverHeight =
        (int)((EYE_H / 2.0f) *
              closeAmount);

    if (coverHeight > 0)
    {
      // Top eyelid

      display.drawBox(
          x,
          y,
          EYE_W,
          coverHeight);

      // Bottom eyelid

      display.drawBox(
          x,
          y + EYE_H - coverHeight,
          EYE_W,
          coverHeight);
    }
  }

  display.setDrawColor(1);
}

// ============================================================
// DRAW HAPPY EYE
// ============================================================

void drawHappyEye(
    int x,
    int y)
{
  display.setDrawColor(1);

  display.drawRBox(
      x,
      y,
      EYE_W,
      EYE_H,
      8);

  display.setDrawColor(0);

  display.drawLine(
      x + 13, y + 20,
      x + 17, y + 16);

  display.drawLine(
      x + 17, y + 16,
      x + 22, y + 13);

  display.drawLine(
      x + 22, y + 13,
      x + 26, y + 12);

  display.drawLine(
      x + 26, y + 12,
      x + 30, y + 13);

  display.drawLine(
      x + 30, y + 13,
      x + 35, y + 16);

  display.drawLine(
      x + 35, y + 16,
      x + 39, y + 20);

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
// DRAW ANGRY EYE
// ============================================================

void drawAngryEye(
    int x,
    int y,
    float pupilOffsetX,
    float pupilOffsetY,
    bool leftEye)
{
  display.setDrawColor(1);

  display.drawRBox(
      x,
      y,
      EYE_W,
      EYE_H,
      8);

  display.setDrawColor(0);

  if (leftEye)
  {
    display.drawTriangle(
        x + 18,
        y,
        x + EYE_W,
        y,
        x + EYE_W,
        y + 13);
  }
  else
  {
    display.drawTriangle(
        x,
        y,
        x + EYE_W - 18,
        y,
        x,
        y + 13);
  }

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
      PUPIL_R);

  display.setDrawColor(1);
}

// ============================================================
// GEOMETRIC EXPRESSION MORPH
// ============================================================
// Uses the existing happyTransition / angryTransition values.
// No touch, duration, state, blink, or expression timing logic is changed.

void drawMorphPolyline(
    const int* points,
    int pointCount,
    float amount)
{
  amount = constrain(amount, 0.0f, 1.0f);

  if (pointCount < 2 || amount <= 0.0f)
  {
    return;
  }

  float scaled = amount * (pointCount - 1);
  int completedSegments = (int)scaled;
  float segmentAmount = scaled - completedSegments;

  if (completedSegments >= pointCount - 1)
  {
    completedSegments = pointCount - 1;
    segmentAmount = 0.0f;
  }

  for (int i = 0; i < completedSegments; i++)
  {
    display.drawLine(
        points[i * 2],
        points[i * 2 + 1],
        points[(i + 1) * 2],
        points[(i + 1) * 2 + 1]);
  }

  if (completedSegments < pointCount - 1)
  {
    int x1 = points[completedSegments * 2];
    int y1 = points[completedSegments * 2 + 1];
    int x2 = points[(completedSegments + 1) * 2];
    int y2 = points[(completedSegments + 1) * 2 + 1];

    int x = (int)round(
        x1 + (x2 - x1) * segmentAmount);

    int y = (int)round(
        y1 + (y2 - y1) * segmentAmount);

    display.drawLine(x1, y1, x, y);
  }
}

void drawHappyEyeMorph(
    int x,
    int y,
    float amount,
    float pupilOffsetX,
    float pupilOffsetY)
{
  amount = constrain(amount, 0.0f, 1.0f);

  if (amount >= 0.95f)
  {
    // Use the exact preserved Happy artwork for the final part
    // of the transition.
    drawHappyEye(x, y);
    return;
  }

  display.setDrawColor(1);
  display.drawRBox(x, y, EYE_W, EYE_H, 8);

  display.setDrawColor(0);

  int pupilRadius =
      (int)round(PUPIL_R * (1.0f - amount));

  if (pupilRadius > 0)
  {
    int pupilCenterX =
        x + EYE_W / 2 + (int)pupilOffsetX;

    int pupilCenterY =
        y + EYE_H / 2 + (int)pupilOffsetY;

    display.drawDisc(
        pupilCenterX,
        pupilCenterY,
        pupilRadius);
  }

  const int curve[] =
  {
    x + 13, y + 20,
    x + 17, y + 16,
    x + 22, y + 13,
    x + 26, y + 12,
    x + 30, y + 13,
    x + 35, y + 16,
    x + 39, y + 20
  };

  drawMorphPolyline(curve, 7, amount);

  display.setDrawColor(1);
}

void drawHappySmileMorph(float amount)
{
  amount = constrain(amount, 0.0f, 1.0f);

  if (!happyExpression && amount <= 0.0f)
  {
    return;
  }

  if (amount >= 0.95f)
  {
    // Use the exact preserved Happy smile for the final part
    // of the transition.
    drawHappySmile();
    return;
  }

  display.setDrawColor(1);

  const int smileOuter[] =
  {
    53, 52,
    56, 55,
    60, 57,
    64, 58,
    68, 57,
    72, 55,
    75, 52
  };

  const int smileInner[] =
  {
    56, 55,
    59, 59,
    62, 61,
    66, 61,
    69, 59,
    72, 55
  };

  drawMorphPolyline(smileOuter, 7, amount);
  drawMorphPolyline(smileInner, 6, amount);

  display.setDrawColor(1);
}

void drawAngryEyeMorph(
    int x,
    int y,
    float pupilOffsetX,
    float pupilOffsetY,
    bool leftEye,
    float amount)
{
  amount = constrain(amount, 0.0f, 1.0f);

  if (amount >= 0.999f)
  {
    drawAngryEye(
        x,
        y,
        pupilOffsetX,
        pupilOffsetY,
        leftEye);
    return;
  }

  display.setDrawColor(1);
  display.drawRBox(x, y, EYE_W, EYE_H, 8);

  int pupilCenterX =
      x + EYE_W / 2 + (int)pupilOffsetX;

  int pupilCenterY =
      y + EYE_H / 2 + (int)pupilOffsetY;

  display.setDrawColor(0);

  display.drawDisc(
      pupilCenterX,
      pupilCenterY,
      PUPIL_R);

  int slantHeight =
      (int)round(13.0f * amount);

  if (slantHeight > 0)
  {
    if (leftEye)
    {
      display.drawTriangle(
          x + 18, y,
          x + EYE_W, y,
          x + EYE_W, y + slantHeight);
    }
    else
    {
      display.drawTriangle(
          x, y,
          x + EYE_W - 18, y,
          x, y + slantHeight);
    }
  }

  display.setDrawColor(1);
}

// ============================================================
// DRAW HEART
// ============================================================

void drawHeart(
  int cx,
  int cy,
  int size
)
{
  display.setDrawColor(1);

  display.drawDisc(
    cx - size / 3,
    cy - size / 4,
    size / 3
  );

  display.drawDisc(
    cx + size / 3,
    cy - size / 4,
    size / 3
  );

  display.drawTriangle(
    cx - size / 2,
    cy - size / 6,
    cx + size / 2,
    cy - size / 6,
    cx,
    cy + size / 2
  );

  display.setDrawColor(1);
}

// ============================================================
// DRAW TOO EXCITED HEART EYES
// ============================================================

void drawTooExcitedEyes()
{
  // Gentle breathing pulse: 18 -> 21 -> 18
  unsigned long elapsed =
    millis() - touchStartTime;

  float phase =
    (float)(elapsed % 1600) / 1600.0f;

  float pulse;

  if (phase < 0.5f)
  {
    pulse = phase * 2.0f;
  }
  else
  {
    pulse = 1.0f - ((phase - 0.5f) * 2.0f);
  }

  pulse =
    pulse * pulse *
    (3.0f - 2.0f * pulse);

  int heartSize =
    18 + (int)(3.0f * pulse);

  drawHeart(
    LEFT_EYE_X + EYE_W / 2,
    EYE_Y + EYE_H / 2,
    heartSize
  );

  drawHeart(
    RIGHT_EYE_X + EYE_W / 2,
    EYE_Y + EYE_H / 2,
    heartSize
  );
}

// ============================================================
// DRAW COMPLETE FACE
// ============================================================

void drawFace()
{
  display.clearBuffer();

  // ==========================================================
  // TOO EXCITED / HEART EYES
  // ==========================================================

  if (
    pokoState == POKO_AWAKE &&
    touchHoldActive
  )
  {
    drawTooExcitedEyes();

    display.sendBuffer();

    return;
  }

  float drawPupilX = pupilX;
  float drawPupilY = pupilY;

  if (
      pokoState == POKO_SLEEPY ||
      pokoState == POKO_SLEEPING ||
      pokoState == POKO_WAKING)
  {
    drawPupilX = 0;
    drawPupilY = 0;
  }

  // ==========================================================
  // HAPPY MORPH
  // ==========================================================

  if (
      pokoState == POKO_AWAKE &&
      happyExpression)
  {
    drawHappyEyeMorph(
        LEFT_EYE_X,
        EYE_Y,
        happyTransition,
        drawPupilX,
        drawPupilY);

    drawHappyEyeMorph(
        RIGHT_EYE_X,
        EYE_Y,
        happyTransition,
        drawPupilX,
        drawPupilY);

    drawHappySmileMorph(happyTransition);
  }

  // ==========================================================
  // ANGRY MORPH
  // ==========================================================

  else if (
      pokoState == POKO_AWAKE &&
      angryExpression)
  {
    drawAngryEyeMorph(
        LEFT_EYE_X,
        EYE_Y,
        drawPupilX,
        drawPupilY,
        true,
        angryTransition);

    drawAngryEyeMorph(
        RIGHT_EYE_X,
        EYE_Y,
        drawPupilX,
        drawPupilY,
        false,
        angryTransition);
  }

  // ==========================================================
  // NORMAL
  // ==========================================================

  else
  {
    drawEye(
        LEFT_EYE_X,
        EYE_Y,
        drawPupilX,
        drawPupilY);

    drawEye(
        RIGHT_EYE_X,
        EYE_Y,
        drawPupilX,
        drawPupilY);
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
  Serial.println("==============================");
  Serial.println("          POKO");
  Serial.println("   A tiny life on your desk");
  Serial.println("==============================");
  Serial.println();

  // ----------------------------------------------------------
  // Touch sensor
  // ----------------------------------------------------------

  pinMode(
      TOUCH_PIN,
      INPUT);

  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(
      OLED_SDA,
      OLED_SCL);

  // ----------------------------------------------------------
  // OLED address
  // ----------------------------------------------------------

  display.setI2CAddress(
      OLED_ADDRESS << 1);

  // ----------------------------------------------------------
  // Start OLED
  // ----------------------------------------------------------

  display.begin();

  // ----------------------------------------------------------
  // Random seed
  // ----------------------------------------------------------

  randomSeed(micros());

  // ----------------------------------------------------------
  // Initial eye position
  // ----------------------------------------------------------

  pupilX = 0;
  pupilY = 0;

  targetPupilX = 0;
  targetPupilY = 0;

  // ----------------------------------------------------------
  // Initial timers
  // ----------------------------------------------------------

  nextBlinkTime =
      millis() + random(1500, 3500);

  nextLookTime =
      millis() + random(1000, 2000);

  // ----------------------------------------------------------
  // Initial state
  // ----------------------------------------------------------

  pokoState = POKO_AWAKE;

  stateStartTime = millis();

  // ----------------------------------------------------------
  // Initial eyelids
  // ----------------------------------------------------------

  blinkCloseAmount = 0.0;
  sleepCloseAmount = 0.0;
  eyeCloseAmount = 0.0;

  // ----------------------------------------------------------
  // Initial expressions
  // ----------------------------------------------------------

  happyExpression = false;
  angryExpression = false;

  happyTransition = 0.0f;
  angryTransition = 0.0f;

  happyLeaving = false;
  angryLeaving = false;

  // ----------------------------------------------------------
  // Touch
  // ----------------------------------------------------------

  touchState = false;
  lastTouchState = false;

  touchCount = 0;
  firstTouchTime = 0;

  touchStartTime = 0;
  touchReleaseTime = 0;

  longTouchDetected = false;
  touchHoldActive = false;

  touchWakeReaction = false;

  // ----------------------------------------------------------
  // Debug
  // ----------------------------------------------------------

  lastDebugPrint = millis();

  Serial.println("OLED initialized.");
  Serial.println("POKO eyes starting...");
  Serial.println("Sleep/wake system enabled.");
  Serial.println("Touch reactions enabled.");
  Serial.println("Angry reaction enabled.");
  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long now = millis();

  // ----------------------------------------------------------
  // State machine
  // ----------------------------------------------------------

  updatePokoState();

  // ----------------------------------------------------------
  // Touch
  // ----------------------------------------------------------

  updateTouch();

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
            0.08);

    pupilY =
        smoothApproach(
            pupilY,
            targetPupilY,
            0.08);
  }

  // ----------------------------------------------------------
  // Sleep / wake
  // ----------------------------------------------------------

  updateSleepAnimation();

  // ----------------------------------------------------------
  // Blink
  // ----------------------------------------------------------

  updateBlink();

  // ----------------------------------------------------------
  // Final eyelid amount
  // ----------------------------------------------------------

  updateEyeCloseAmount();

  // ----------------------------------------------------------
  // Happy
  // ----------------------------------------------------------

  updateHappyExpression();

  // ----------------------------------------------------------
  // Angry
  // ----------------------------------------------------------

  updateAngryExpression();

  // ----------------------------------------------------------
  // Debug
  // ----------------------------------------------------------

  printDebugStatus();

  // ----------------------------------------------------------
  // Render ~60 FPS
  // ----------------------------------------------------------

  if (
      now - lastFrameTime >=
      FRAME_INTERVAL)
  {
    lastFrameTime = now;

    drawFace();
  }
}