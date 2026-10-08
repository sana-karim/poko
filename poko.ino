#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

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
// 📶 Wi-Fi + NTP time + weather
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

const char *POKO_BUILD_VERSION = "v0.6.1-weather-ui-v7";
const char *POKO_BUILD_NAME = "WEATHER UI - NO HEADER LINE";

// ============================================================
// v0.6.0 - WI-FI / TIME / WEATHER
// ============================================================
// Set your Wi-Fi credentials here before compiling.
// Weather location is Patna, Bihar, India.
// Open-Meteo is used without an API key.
// ============================================================

const char *WIFI_SSID = "Airtel_Zerotouch";
const char *WIFI_PASSWORD = "Airtel@123";

const float WEATHER_LATITUDE = 25.5941f;
const float WEATHER_LONGITUDE = 85.1376f;
const char *WEATHER_CITY = "PATNA";

const unsigned long WEATHER_UPDATE_INTERVAL = 30UL * 60UL * 1000UL;
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;
const unsigned long INFO_TRANSITION_TIME = 900;

bool wifiReady = false;
bool timeReady = false;

float weatherTemperature = 0.0f;
int weatherCode = -1;
float weatherHumidity = 0.0f;
float weatherWindSpeed = 0.0f;
float weatherWindDirection = 0.0f;
float weatherPrecipitation = 0.0f;
bool weatherValid = false;
unsigned long lastWeatherUpdate = 0;

// Information display mode.
enum InfoMode
{
  INFO_NONE,
  INFO_CLOCK,
  INFO_WEATHER
};

InfoMode infoMode = INFO_NONE;
unsigned long infoModeStartTime = 0;
bool infoExitArmed = false;

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

// ------------------------------------------------------------
// USB SERIAL SAFETY
//
// Serial is debug-only. Never allow a closed/disconnected USB
// monitor to block the main POKO animation loop.
// ------------------------------------------------------------

bool debugSerialReady(size_t requiredBytes = 256)
{
  return Serial.availableForWrite() >= requiredBytes;
}

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

void enterInfoMode(InfoMode mode);
void exitInfoMode();
void connectWiFi();
void syncTimeFromNTP();
void updateWeather();

const char *weatherDescription(int code);

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

  if (debugSerialReady())
  {
    Serial.println("POKO is happy!");
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

  // Cancel Happy.
  happyExpression = false;
  happyLeaving = false;
  happyTransition = 0.0f;

  // Start Angry.
  angryExpression = true;
  angryLeaving = false;

  angryStartTime = millis();

  if (debugSerialReady())
  {
    Serial.println("POKO is angry!");
  }
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
// v0.6.0 - WIFI / TIME / WEATHER HELPERS
// ============================================================

void connectWiFi()
{
  if (debugSerialReady())
  {
    Serial.println("[WIFI] connectWiFi() called");
    Serial.print("[WIFI] Current status: ");
    Serial.println(WiFi.status());
    wl_status_t status = WiFi.status();

    Serial.printf("[WIFI] Current status: %d\n", status);

    switch (status)
    {
    case WL_CONNECTED:
      Serial.println("[WIFI] Status: CONNECTED");
      break;

    case WL_NO_SSID_AVAIL:
      Serial.println("[WIFI] Status: NO SSID AVAILABLE");
      break;

    case WL_CONNECT_FAILED:
      Serial.println("[WIFI] Status: CONNECT FAILED");
      break;

    case WL_CONNECTION_LOST:
      Serial.println("[WIFI] Status: CONNECTION LOST");
      break;

    case WL_DISCONNECTED:
      Serial.println("[WIFI] Status: DISCONNECTED");
      break;

    default:
      Serial.println("[WIFI] Status: UNKNOWN / ESP32-specific state");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiReady = true;

    if (debugSerialReady())
    {
      Serial.println("[WIFI] Already connected");
      Serial.print("[WIFI] IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[WIFI] RSSI: ");
      Serial.println(WiFi.RSSI());
    }

    return;
  }

  if (WIFI_SSID[0] == '\0')
  {
    wifiReady = false;

    if (debugSerialReady())
    {
      Serial.println("[WIFI] ERROR: SSID is empty/default");
    }

    return;
  }

  Serial.printf("[WIFI TEST] SSID length: %d\n", strlen(WIFI_SSID));
  Serial.printf("[WIFI TEST] Password length: %d\n", strlen(WIFI_PASSWORD));

  Serial.println("[WIFI TEST] About to call WiFi.begin()");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("[WIFI TEST] WiFi.begin() called");

  if (debugSerialReady())
  {
    Serial.print("[WIFI] Connecting to: ");
    Serial.println(WIFI_SSID);
  }

  unsigned long start = millis();
  unsigned long lastLog = 0;

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < WIFI_CONNECT_TIMEOUT)
  {
    delay(20);

    if (millis() - lastLog >= 1000)
    {
      lastLog = millis();

      if (debugSerialReady())
      {
        Serial.print("[WIFI] Waiting... status=");
        Serial.print(WiFi.status());
        Serial.print(" elapsed=");
        Serial.print(millis() - start);
        Serial.println(" ms");
      }
    }
  }

  wifiReady = WiFi.status() == WL_CONNECTED;

  if (debugSerialReady())
  {
    if (wifiReady)
    {
      Serial.println("[WIFI] CONNECTED");
      Serial.print("[WIFI] IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[WIFI] RSSI: ");
      Serial.println(WiFi.RSSI());
    }
    else
    {
      Serial.println("[WIFI] FAILED to connect");
      Serial.print("[WIFI] Final status: ");
      Serial.println(WiFi.status());
    }
  }
}

void syncTimeFromNTP()
{
  if (debugSerialReady())
  {
    Serial.println("[NTP] syncTimeFromNTP() called");
  }

  if (!wifiReady)
  {
    timeReady = false;

    if (debugSerialReady())
    {
      Serial.println("[NTP] SKIPPED: WiFi is not ready");
    }

    return;
  }

  configTzTime("IST-5:30", "pool.ntp.org", "time.nist.gov");

  if (debugSerialReady())
  {
    Serial.println("[NTP] Waiting for time...");
  }

  struct tm timeInfo;
  unsigned long start = millis();

  while (!getLocalTime(&timeInfo, 100) &&
         millis() - start < 5000)
  {
  }

  timeReady = getLocalTime(&timeInfo, 10);

  if (debugSerialReady())
  {
    if (timeReady)
    {
      Serial.println("[NTP] TIME READY");
      Serial.printf("[NTP] %02d:%02d:%02d %02d/%02d/%04d\n",
                    timeInfo.tm_hour,
                    timeInfo.tm_min,
                    timeInfo.tm_sec,
                    timeInfo.tm_mday,
                    timeInfo.tm_mon + 1,
                    timeInfo.tm_year + 1900);
    }
    else
    {
      Serial.println("[NTP] FAILED: time not available");
    }
  }
}

void updateWeather()
{
  if (debugSerialReady())
  {
    Serial.println("[WEATHER] updateWeather() called");
  }

  if (weatherValid &&
      millis() - lastWeatherUpdate < WEATHER_UPDATE_INTERVAL)
  {
    return;
  }

  connectWiFi();

  if (!wifiReady)
  {
    if (debugSerialReady())
    {
      Serial.println("[WEATHER] ABORTED: WiFi not connected");
    }
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url =
      "https://api.open-meteo.com/v1/forecast?latitude=" +
      String(WEATHER_LATITUDE, 4) +
      "&longitude=" +
      String(WEATHER_LONGITUDE, 4) +
      "&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m,wind_direction_10m,pressure_msl&timezone=Asia%2FKolkata";

  if (debugSerialReady())
  {
    Serial.println("[WEATHER] Opening Open-Meteo connection...");
  }

  if (!http.begin(client, url))
  {
    if (debugSerialReady())
    {
      Serial.println("[WEATHER] ERROR: http.begin() failed");
    }
    return;
  }

  http.setTimeout(7000);

  if (debugSerialReady())
  {
    Serial.println("[WEATHER] Sending HTTP GET...");
  }

  int httpCode = http.GET();

  if (debugSerialReady())
  {
    Serial.print("[WEATHER] HTTP code: ");
    Serial.println(httpCode);
  }

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();

    // Open-Meteo includes the same field names in current_units and current.
    // Parse only inside the current object so unit strings cannot be matched.
    int currentKey = payload.indexOf("\"current\"");
    int currentStart = -1;
    int currentEnd = -1;

    if (currentKey >= 0)
    {
      currentStart = payload.indexOf('{', currentKey);
      if (currentStart >= 0)
      {
        currentEnd = payload.indexOf('}', currentStart);
      }
    }

    auto extractCurrentNumber = [&](const char *key, float &value) -> bool
    {
      if (currentStart < 0 || currentEnd <= currentStart)
      {
        return false;
      }

      String token = String("\"") + key + "\"";
      int keyPos = payload.indexOf(token, currentStart);

      if (keyPos < 0 || keyPos >= currentEnd)
      {
        return false;
      }

      int colon = payload.indexOf(':', keyPos + token.length());
      if (colon < 0 || colon >= currentEnd)
      {
        return false;
      }

      int valueStart = colon + 1;
      while (valueStart < currentEnd)
      {
        char c = payload.charAt(valueStart);
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
        {
          valueStart++;
        }
        else
        {
          break;
        }
      }

      int valueEnd = valueStart;
      while (valueEnd < currentEnd)
      {
        char c = payload.charAt(valueEnd);
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' ||
            c == '.' || c == 'e' || c == 'E')
        {
          valueEnd++;
        }
        else
        {
          break;
        }
      }

      if (valueEnd <= valueStart)
      {
        return false;
      }

      String valueText = payload.substring(valueStart, valueEnd);
      value = valueText.toFloat();
      return true;
    };

    float temperature = 0.0f;
    float humidity = 0.0f;
    float precipitation = 0.0f;
    float windSpeed = 0.0f;
    float windDirection = 0.0f;
    float codeFloat = -1.0f;

    bool tempOK = extractCurrentNumber("temperature_2m", temperature);
    bool humidityOK = extractCurrentNumber("relative_humidity_2m", humidity);
    bool precipitationOK = extractCurrentNumber("precipitation", precipitation);
    bool codeOK = extractCurrentNumber("weather_code", codeFloat);
    bool windSpeedOK = extractCurrentNumber("wind_speed_10m", windSpeed);
    bool windDirectionOK = extractCurrentNumber("wind_direction_10m", windDirection);

    if (debugSerialReady())
    {
      Serial.printf(
          "[WEATHER DEBUG] current=%d..%d temp=%d humidity=%d code=%d wind=%d/%d precip=%d\n",
          currentStart,
          currentEnd,
          tempOK,
          humidityOK,
          codeOK,
          windSpeedOK,
          windDirectionOK,
          precipitationOK);
    }

    if (tempOK && codeOK)
    {
      weatherTemperature = temperature;
      weatherCode = (int)codeFloat;

      if (humidityOK)
      {
        weatherHumidity = humidity;
      }

      if (precipitationOK)
      {
        weatherPrecipitation = precipitation;
      }

      if (windSpeedOK)
      {
        weatherWindSpeed = windSpeed;
      }

      if (windDirectionOK)
      {
        weatherWindDirection = windDirection;
      }

      weatherValid = true;
      lastWeatherUpdate = millis();

      if (debugSerialReady())
      {
        Serial.printf(
            "[WEATHER] %.1f C | humidity %.0f%% | wind %.1f km/h @ %.0f deg | precip %.1f mm\n",
            weatherTemperature,
            weatherHumidity,
            weatherWindSpeed,
            weatherWindDirection,
            weatherPrecipitation);
      }
    }
    else if (debugSerialReady())
    {
      Serial.println("[WEATHER] ERROR: required current weather values missing");
    }
  }

  http.end();

  // Wi-Fi is only needed for synchronization/data retrieval.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiReady = false;
}

const char *weatherDescription(int code)
{
  switch (code)
  {
  case 0:
    return "CLEAR";
  case 1:
  case 2:
    return "PARTLY CLOUDY";
  case 3:
    return "CLOUDY";
  case 45:
  case 48:
    return "FOGGY";
  case 51:
  case 53:
  case 55:
  case 56:
  case 57:
    return "DRIZZLE";
  case 61:
  case 63:
  case 65:
  case 66:
  case 67:
    return "RAIN";
  case 71:
  case 73:
  case 75:
  case 77:
    return "SNOW";
  case 80:
  case 81:
  case 82:
    return "SHOWERS";
  case 85:
  case 86:
    return "SNOW SHOWERS";
  case 95:
  case 96:
  case 99:
    return "STORM";
  default:
    return "UNKNOWN";
  }
}

void enterInfoMode(InfoMode mode)
{
  infoMode = mode;
  infoModeStartTime = millis();
  infoExitArmed = false;

  // Information screens must never fall asleep.
  lastInteractionTime = millis();

  // Keep POKO centered/awake while showing information.
  pokoState = POKO_AWAKE;
  stateStartTime = millis();
  blinking = false;
  blinkCloseAmount = 0.0f;
  eyeCloseAmount = 0.0f;
  sleepCloseAmount = 0.0f;

  happyExpression = false;
  happyLeaving = false;
  happyTransition = 0.0f;
  angryExpression = false;
  angryLeaving = false;
  angryTransition = 0.0f;

  targetPupilX = 0;
  targetPupilY = 0;
  pupilX = 0;
  pupilY = 0;

  if (debugSerialReady())
  {
    Serial.print("[INFO] Opening mode: ");
    Serial.println(mode == INFO_CLOCK ? "CLOCK" : "WEATHER");
  }

  if (mode == INFO_CLOCK)
  {
    if (!timeReady)
    {
      connectWiFi();
      syncTimeFromNTP();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      wifiReady = false;
    }
  }
  else if (mode == INFO_WEATHER)
  {
    if (!timeReady)
    {
      connectWiFi();
      syncTimeFromNTP();
    }

    updateWeather();
  }

  // Do not let the information gesture become another POKO tap.
  touchCount = 0;
  firstTouchTime = 0;
  longTouchDetected = true;
  touchHoldActive = false;
}

void exitInfoMode()
{
  infoMode = INFO_NONE;
  infoExitArmed = false;

  // Clear the gesture that opened the information screen.
  touchCount = 0;
  firstTouchTime = 0;
  longTouchDetected = false;
  touchHoldActive = false;

  resetAwakeTimer();
  chooseLookDirection();
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

  if (debugSerialReady())
  {
    Serial.println("POKO woke up because of touch!");
  }
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
  // INFORMATION MODE
  // ==========================================================
  // A tap or a hold exits the information screen. No tap
  // counting, sleep logic, or expression reaction is allowed
  // while Clock/Weather is being displayed.

  if (infoMode != INFO_NONE)
  {
    if (currentTouch != touchState)
    {
      touchState = currentTouch;

      // The release that completes the Clock/Weather opening
      // gesture must NOT close the information screen.
      if (!touchState)
      {
        if (infoExitArmed)
        {
          exitInfoMode();
        }
        else
        {
          infoExitArmed = true;
        }
      }
    }

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

      if (debugSerialReady())
      {
        Serial.print("TOUCH START | Count: ");
        Serial.println(touchCount);
      }

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

        if (debugSerialReady())
        {
          Serial.println(
              "POKO: Too much poking! 😠");
        }

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

        if (debugSerialReady())
        {
          Serial.println(
              "POKO is excited!");
        }

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
        if (debugSerialReady())
        {
          Serial.println(
              "POKO is still angry.");
        }

        // IMPORTANT:
        // Do not trigger Happy here.

        return;
      }

      // ------------------------------------------------------
      // LONG HOLD RELEASE
      // ------------------------------------------------------

      if (longTouchDetected)
      {
        if (debugSerialReady())
        {
          Serial.print(
              "POKO was petted for ");

          Serial.print(
              touchDuration);

          Serial.println(
              " ms");
        }

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

          if (debugSerialReady())
          {
            Serial.println(
                "POKO is calming down...");
          }
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
        if (debugSerialReady())
        {
          Serial.println(
              "POKO detected repeated interaction.");
        }

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

      // ------------------------------------------------------
      // v0.6.0 INFORMATION GESTURES
      //
      // 1 tap + hold  = Clock
      // 2 taps + hold = Weather
      //
      // The current hold itself is counted as the next touch,
      // therefore 1 tap + hold = 2 and 2 taps + hold = 3.
      // ------------------------------------------------------

      if (touchCount == 2)
      {
        enterInfoMode(INFO_CLOCK);

        if (debugSerialReady())
        {
          Serial.println("POKO: Clock mode");
        }

        return;
      }

      if (touchCount == 3)
      {
        enterInfoMode(INFO_WEATHER);

        if (debugSerialReady())
        {
          Serial.println("POKO: Weather mode");
        }

        return;
      }

      // Original v0.5.1 hold behavior remains unchanged.
      touchHoldActive = true;

      if (debugSerialReady())
      {
        Serial.println(
            "POKO is being petted...");
      }

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

    if (debugSerialReady())
    {
      Serial.println(
          "POKO awake timer reset.");
    }
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

    if (!debugSerialReady())
    {
      return;
    }

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
    Serial.print(eyeCloseAmount, 3);

    Serial.print(" | INFO: ");
    if (infoMode == INFO_CLOCK)
    {
      Serial.println("CLOCK");
    }
    else if (infoMode == INFO_WEATHER)
    {
      Serial.println("WEATHER");
    }
    else
    {
      Serial.println("NONE");
    }
  }
}

// ============================================================
// UPDATE POKO STATE
// ============================================================

void updatePokoState()
{
  unsigned long now = millis();

  // Clock/Weather never sleep.
  if (infoMode != INFO_NONE)
  {
    return;
  }

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

      if (debugSerialReady())
      {
        Serial.println(
            "POKO is getting sleepy...");
      }
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

      if (debugSerialReady())
      {
        Serial.println(
            "POKO is sleeping...");
      }
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

      if (debugSerialReady())
      {
        Serial.println(
            "POKO is waking up...");
      }
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

          if (debugSerialReady())
          {
            Serial.println(
                "POKO is happy to see you!");
          }
        }
      }

      if (debugSerialReady())
      {
        Serial.println(
            "POKO is awake!");
      }
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
    const int *points,
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
          x + 39, y + 20};

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
          75, 52};

  const int smileInner[] =
      {
          56, 55,
          59, 59,
          62, 61,
          66, 61,
          69, 59,
          72, 55};

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
    int size)
{
  display.setDrawColor(1);

  display.drawDisc(
      cx - size / 3,
      cy - size / 4,
      size / 3);

  display.drawDisc(
      cx + size / 3,
      cy - size / 4,
      size / 3);

  display.drawTriangle(
      cx - size / 2,
      cy - size / 6,
      cx + size / 2,
      cy - size / 6,
      cx,
      cy + size / 2);

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
      heartSize);

  drawHeart(
      RIGHT_EYE_X + EYE_W / 2,
      EYE_Y + EYE_H / 2,
      heartSize);
}

// ============================================================
// DRAW INFORMATION SCREENS
// ============================================================

void drawCenteredText(const char *text, int y, uint8_t font = 1)
{
  if (font == 2)
  {
    display.setFont(u8g2_font_helvB14_tr);
  }
  else
  {
    display.setFont(u8g2_font_6x12_tr);
  }

  int width = display.getStrWidth(text);
  int x = (SCREEN_W - width) / 2;
  display.drawStr(x, y, text);
}

void drawClockScreen()
{
  struct tm timeInfo;

  if (!getLocalTime(&timeInfo, 5))
  {
    drawCenteredText("TIME NOT READY", 36);
    return;
  }

  char dateText[24];
  char timeText[16];
  char ampm[3];

  // Date: Fri 17 Apr. 2026
  strftime(dateText, sizeof(dateText), "%a %d %b. %Y", &timeInfo);

  int hour12 = timeInfo.tm_hour % 12;
  if (hour12 == 0)
  {
    hour12 = 12;
  }

  strftime(ampm, sizeof(ampm), "%p", &timeInfo);

  snprintf(
      timeText,
      sizeof(timeText),
      "%02d:%02d %s",
      hour12,
      timeInfo.tm_min,
      ampm);

  // Date
  display.setFont(u8g2_font_6x12_tr);
  int width = display.getStrWidth(dateText);
  display.drawStr((SCREEN_W - width) / 2, 10, dateText);

  // Dotted separator between date and time.
  for (int x = 8; x < SCREEN_W - 8; x += 4)
  {
    display.drawPixel(x, 17);
  }

  // Time
  // Keep AM/PM at the existing size, but increase only HH:MM by one size.
  char hourMinText[8];
  snprintf(
      hourMinText,
      sizeof(hourMinText),
      "%02d:%02d",
      hour12,
      timeInfo.tm_min);

  display.setFont(u8g2_font_helvB24_tr);
  int hourMinWidth = display.getStrWidth(hourMinText);

  display.setFont(u8g2_font_helvB14_tr);
  int ampmWidth = display.getStrWidth(ampm);

  const int timeGap = 3;
  const int totalTimeWidth = hourMinWidth + timeGap + ampmWidth;
  const int timeX = (SCREEN_W - totalTimeWidth) / 2;

  // Vertically center the complete time between the dotted date separator
  // and the bottom seconds line. Keep both separator positions unchanged.
  const int topBoundary = 17;
  // const int bottomBoundary = 57;
  const int bottomBoundary = 52;
  const int centerY = (topBoundary + bottomBoundary) / 2;

  display.setFont(u8g2_font_helvB24_tr);
  const int timeAscent = display.getAscent();
  const int timeDescent = display.getDescent();
  const int timeBaseline = centerY + (timeAscent - timeDescent) / 2;

  display.drawStr(timeX, timeBaseline, hourMinText);

  display.setFont(u8g2_font_helvB14_tr);
  display.drawStr(timeX + hourMinWidth + timeGap, timeBaseline, ampm);

  // Seconds progress: 0 -> 60.
  // The line fills one second at a time and resets at the next minute.
  const int progressX = 16;
  const int progressWidth = SCREEN_W - 32;
  const int progressY = 60;

  // Background dotted line.
  for (int x = progressX; x <= progressX + progressWidth; x += 2)
  {
    display.drawPixel(x, progressY);
  }

  // Fill according to the current second.
  int filledWidth = (timeInfo.tm_sec * progressWidth) / 60;
  if (timeInfo.tm_sec > 0)
  {
    display.drawBox(progressX, progressY - 1, filledWidth + 1, 3);
  }
}

const char *windDirectionText(float degrees)
{
  static const char *directions[] = {
      "N", "NNE", "NE", "ENE",
      "E", "ESE", "SE", "SSE",
      "S", "SSW", "SW", "WSW",
      "W", "WNW", "NW", "NNW"};

  int index = (int)((degrees + 11.25f) / 22.5f) % 16;
  return directions[index];
}

void drawCloudShape(int cx, int cy)
{
  display.drawDisc(cx - 5, cy + 2, 7);
  display.drawDisc(cx + 4, cy, 8);
  display.drawDisc(cx + 13, cy + 4, 6);
  display.drawRBox(cx - 12, cy + 3, 31, 10, 4);
}

void drawSunShape(int cx, int cy, int radius, bool hideHorizontalRays = false)
{
  display.drawDisc(cx, cy, radius);
  for (int i = 0; i < 8; i++)
  {
    if (hideHorizontalRays && (i == 0 || i == 4))
      continue;
    float a = i * 0.785398f;
    int x1 = cx + (int)((radius + 4) * cos(a));
    int y1 = cy + (int)((radius + 4) * sin(a));
    int x2 = cx + (int)((radius + 9) * cos(a));
    int y2 = cy + (int)((radius + 9) * sin(a));
    display.drawLine(x1, y1, x2, y2);
  }
}

void drawWeatherIcon(int code)
{
  // Compact 34x34 monochrome weather icon for the 128x64 OLED.
  const int cx = 17;
  const int cy = 25;

  display.setDrawColor(1);

  if (code == 0)
  {
    drawSunShape(cx, cy, 7);
    return;
  }

  if (code == 1)
  {
    // Mainly clear: small sun behind a cloud.
    drawSunShape(cx - 6, cy - 6, 4, true);
    drawCloudShape(cx - 1, cy + 1);
    return;
  }

  if (code == 2)
  {
    // Partly cloudy: larger cloud with visible sun.
    drawSunShape(cx - 7, cy - 7, 4, true);
    drawCloudShape(cx - 1, cy + 1);
    return;
  }

  if (code == 3)
  {
    // Overcast.
    drawCloudShape(cx - 1, cy);
    drawCloudShape(cx - 5, cy - 5);
    return;
  }

  if (code == 45 || code == 48)
  {
    // Fog: three horizontal layers.
    display.drawLine(3, 17, 30, 17);
    display.drawLine(1, 24, 33, 24);
    display.drawLine(4, 31, 30, 31);
    display.drawLine(7, 38, 27, 38);
    return;
  }

  // Drizzle / rain / showers / snow / storm.
  drawCloudShape(cx - 1, cy - 2);

  if (code == 71 || code == 73 || code == 75 || code == 77)
  {
    // Snow: small flakes.
    for (int i = 0; i < 3; i++)
    {
      int x = 8 + i * 9;
      int y = 39;
      display.drawLine(x - 2, y, x + 2, y);
      display.drawLine(x, y - 2, x, y + 2);
    }
    return;
  }

  if (code >= 95)
  {
    // Thunderstorm: rain plus a simple lightning bolt.
    display.drawLine(13, 36, 10, 42);
    display.drawLine(10, 42, 14, 42);
    display.drawLine(14, 42, 12, 47);
    display.drawLine(22, 36, 19, 42);
    display.drawLine(19, 42, 23, 42);
    display.drawLine(23, 42, 21, 47);
    return;
  }

  // Drizzle / rain / showers.
  display.drawLine(10, 37, 8, 44);
  display.drawLine(18, 37, 16, 44);
  display.drawLine(26, 37, 24, 44);
}

void drawWeatherScreen()
{
  if (!weatherValid)
  {
    drawCenteredText("WEATHER", 22);
    drawCenteredText("NO DATA", 42);
    return;
  }

  // Keep the existing clean visual hierarchy, but use the available
  // 128x64 area for useful weather details.
  display.setFont(u8g2_font_6x12_tr);

  // Header.
  int width = display.getStrWidth(WEATHER_CITY);
  display.drawStr((SCREEN_W - width) / 2, 10, WEATHER_CITY);
  // Icon + temperature.
  drawWeatherIcon(weatherCode);

  // Draw the degree symbol separately. The selected U8g2 font may
  // not render the UTF-8 "°" character correctly.
  char tempText[16];
  snprintf(tempText, sizeof(tempText), "%.0f", weatherTemperature);

  display.setFont(u8g2_font_helvB14_tr);

  int tempX = 39;
  int tempWidth = display.getStrWidth(tempText);
  display.drawStr(tempX, 35, tempText);

  // Small degree symbol.
  // One visual space between the temperature and °C.
  int degreeX = tempX + tempWidth + 3;
  display.drawCircle(degreeX + 2, 24, 2);

  // Celsius stays attached to the degree symbol: 25 °C.
  display.drawStr(degreeX + 5, 35, "C");

  // Condition.
  const char *description = weatherDescription(weatherCode);
  display.setFont(u8g2_font_5x8_tr);
  width = display.getStrWidth(description);
  display.drawStr(39, 44, description);

  // Bottom line: wind speed + direction + humidity.
  char detailsText[32];
  snprintf(
      detailsText,
      sizeof(detailsText),
      "W %.0f km/h %s  H %.0f%%",
      weatherWindSpeed,
      windDirectionText(weatherWindDirection),
      weatherHumidity);

  width = display.getStrWidth(detailsText);
  display.drawStr((SCREEN_W - width) / 2, 59, detailsText);
}

void drawInfoScreen()
{
  display.clearBuffer();
  display.setDrawColor(1);

  // Curious/look-at-you transition before information.
  if (millis() - infoModeStartTime < INFO_TRANSITION_TIME)
  {
    drawEye(
        LEFT_EYE_X,
        EYE_Y,
        0,
        0);

    drawEye(
        RIGHT_EYE_X,
        EYE_Y,
        0,
        0);

    display.sendBuffer();
    return;
  }

  if (infoMode == INFO_CLOCK)
  {
    drawClockScreen();
  }
  else if (infoMode == INFO_WEATHER)
  {
    drawWeatherScreen();
  }

  display.sendBuffer();
}

// ============================================================
// DRAW COMPLETE FACE
// ============================================================

void drawFace()
{
  if (infoMode != INFO_NONE)
  {
    drawInfoScreen();
    return;
  }

  display.clearBuffer();

  // ==========================================================
  // TOO EXCITED / HEART EYES
  // ==========================================================

  if (
      pokoState == POKO_AWAKE &&
      touchHoldActive)
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
  Serial.println("========================================");
  Serial.println("POKO BUILD IDENTIFICATION");
  Serial.print("BUILD VERSION: ");
  Serial.println(POKO_BUILD_VERSION);
  Serial.print("BUILD NAME: ");
  Serial.println(POKO_BUILD_NAME);
  Serial.print("COMPILED SOURCE: ");
  Serial.println(__FILE__);
  Serial.println("========================================");

  if (debugSerialReady())
  {
    Serial.println();
    Serial.println("==============================");
    Serial.println("          POKO");
    Serial.println("   A tiny life on your desk");
    Serial.println("==============================");
    Serial.println();
  }

  // ----------------------------------------------------------
  // v0.6.0 networking
  // Wi-Fi starts OFF. It is enabled only when time/weather
  // data is requested, then turned off again.
  // ----------------------------------------------------------

  WiFi.mode(WIFI_OFF);
  wifiReady = false;
  timeReady = false;

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

  if (debugSerialReady())
  {
    Serial.println("OLED initialized.");
    Serial.println("POKO eyes starting...");
    Serial.println("Sleep/wake system enabled.");
    Serial.println("Touch reactions enabled.");
    Serial.println("Angry reaction enabled.");
    Serial.println();
  }
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