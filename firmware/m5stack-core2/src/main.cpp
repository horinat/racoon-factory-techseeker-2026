
#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Replace these values before uploading to the M5Stack Core2.
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_KEY = "YOUR_API_KEY";

const char* API_URL =
    "https://racoonfactory-techseeker-2026-wvbquo-34408a-220-158-16-196.sslip.io/api/scores";
const char* DEVICE_ID = "core2-demo-01";
const char* PLAYER_NAME = "CORE2 PLAYER";

constexpr uint32_t GAME_DURATION_SECONDS = 60;
constexpr uint32_t GAME_DURATION_MS = GAME_DURATION_SECONDS * 1000;
constexpr uint32_t HIT_COOLDOWN_MS = 300;
constexpr uint8_t WIFI_CONNECT_ATTEMPTS = 2;
constexpr uint8_t SCORE_SEND_ATTEMPTS = 2;

// Temporary pin assignment for TechSeeker prototype.
// Core2 official pin map:
// PORT-A: G32/G33, PORT-B: G26/G36, PORT-C: G14/G13.
constexpr int START_PIN = 32;    // PORT-A yellow. Digital input, active LOW.
constexpr int RESTART_PIN = 14;  // PORT-C yellow. Digital input, active LOW.
constexpr int TARGET_PIN = 26;   // PORT-B yellow. Digital input, active LOW.
constexpr int SERVO_PIN = 13;    // PORT-C white. PWM output signal.

// Servo settings. Change these after Saturday's angle test.
constexpr int SERVO_IDLE_ANGLE = 0;
constexpr int SERVO_SEQUENCE_ANGLES[] = {120, 140, 90, 0};
constexpr uint32_t SERVO_SEQUENCE_INTERVALS_MS[] = {100, 100, 100, 100};
constexpr size_t SERVO_SEQUENCE_COUNT =
    sizeof(SERVO_SEQUENCE_ANGLES) / sizeof(SERVO_SEQUENCE_ANGLES[0]);
constexpr int SERVO_PWM_HZ = 50;
constexpr int SERVO_PWM_BITS = 16;
constexpr int SERVO_PWM_CHANNEL = 0;
constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2400;

bool gameRunning = false;
bool gamePaused = false;
bool scoreSent = false;
int score = 0;
uint32_t gameStartedAt = 0;
uint32_t pausedRemainingMs = GAME_DURATION_MS;
uint32_t gameNumber = 0;
uint32_t lastHitAt = 0;
uint32_t servoNextStepAt = 0;
size_t servoSequenceIndex = 0;
bool servoSequenceRunning = false;
String lastSendMessage = "";

bool previousStartState = false;
bool previousRestartState = false;
bool previousTargetState = false;

void setServoAngle(int angle);

void drawButtonLabels() {
  M5.Display.fillRect(0, 200, 320, 40, TFT_DARKGREY);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString(gameRunning ? "-" : "START", 53, 220, 2);
  M5.Display.drawString(gameRunning ? "+1 HIT" : "-", 160, 220, 2);
  M5.Display.drawString(gameRunning ? "+5 HIT" : "-", 267, 220, 2);
}

void drawGame() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("RacoonFactory", 160, 26, 2);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  const char* stateText = "READY";
  if (gameRunning && gamePaused) stateText = "PAUSED";
  if (gameRunning && !gamePaused) stateText = "GAME RUNNING";
  M5.Display.drawString(stateText, 160, 62, 4);

  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.drawNumber(score, 160, 125, 7);
  drawButtonLabels();
}

void drawTimeLeft() {
  uint32_t elapsed = millis() - gameStartedAt;
  uint32_t remainingMs = gamePaused
                             ? pausedRemainingMs
                             : (elapsed >= GAME_DURATION_MS ? 0 : GAME_DURATION_MS - elapsed);
  uint32_t secondsLeft = (remainingMs + 999) / 1000;
  M5.Display.fillRect(100, 168, 120, 24, TFT_BLACK);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.drawString(String(secondsLeft) + " sec", 160, 180, 2);
}

bool connectWifi(bool forceReconnect = false) {
  if (!forceReconnect && WiFi.status() == WL_CONNECTED) return true;

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("Connecting Wi-Fi...", 160, 100, 2);

  for (uint8_t attempt = 1; attempt <= WIFI_CONNECT_ATTEMPTS; attempt++) {
    if (forceReconnect || attempt > 1) {
      WiFi.disconnect(false);
      delay(500);
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) {
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Wi-Fi connected IP=%s RSSI=%d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      return true;
    }
  }

  Serial.printf("Wi-Fi failed status=%d\n", WiFi.status());
  return false;
}

bool sendScore() {
  if (!connectWifi()) {
    lastSendMessage = "Wi-Fi failed";
    Serial.println(lastSendMessage);
    return false;
  }

  String gameId = String(DEVICE_ID) + "-" + String(gameNumber) + "-" + String(millis());
  String json = "{";
  json += "\"game_id\":\"" + gameId + "\",";
  json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  json += "\"player_name\":\"" + String(PLAYER_NAME) + "\",";
  json += "\"score\":" + String(score) + ",";
  json += "\"duration_seconds\":" + String(GAME_DURATION_SECONDS) + ",";
  json += "\"started_at\":\"2026-06-14T00:00:00+09:00\"";
  json += "}";

  for (uint8_t attempt = 1; attempt <= SCORE_SEND_ATTEMPTS; attempt++) {
    WiFiClientSecure client;
    // Demo setting for the generated sslip.io domain. Use a CA certificate for production.
    client.setInsecure();
    client.setHandshakeTimeout(30);

    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(15000);
    if (!http.begin(client, API_URL)) {
      lastSendMessage = "HTTP begin failed";
      Serial.println(lastSendMessage);
      return false;
    }

    http.addHeader("Authorization", "Bearer " + String(API_KEY));
    http.addHeader("Content-Type", "application/json");
    int status = http.POST(json);
    String response = http.getString();
    Serial.printf("POST attempt=%d status=%d response=%s\n", attempt, status,
                  response.c_str());
    http.end();

    lastSendMessage = "HTTP " + String(status);
    if (status < 0) {
      lastSendMessage += " " + http.errorToString(status);
    }
    if (response.length() > 0) {
      lastSendMessage += " " + response.substring(0, 40);
    }

    if (status == 201 || status == 409) return true;
    if (status >= 400) return false;

    connectWifi(true);
  }

  return false;
}

void startGame() {
  gameNumber++;
  score = 0;
  scoreSent = false;
  gameRunning = true;
  gamePaused = false;
  pausedRemainingMs = GAME_DURATION_MS;
  gameStartedAt = millis();
  lastHitAt = 0;
  drawGame();
}

void pauseGame() {
  if (!gameRunning || gamePaused) return;
  uint32_t elapsed = millis() - gameStartedAt;
  pausedRemainingMs = elapsed >= GAME_DURATION_MS ? 0 : GAME_DURATION_MS - elapsed;
  gamePaused = true;
  drawGame();
}

void resumeGame() {
  if (!gameRunning || !gamePaused) return;
  gamePaused = false;
  gameStartedAt = millis() - (GAME_DURATION_MS - pausedRemainingMs);
  drawGame();
}

void finishGame() {
  gameRunning = false;
  drawGame();

  M5.Display.fillRect(0, 155, 320, 40, TFT_BLACK);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("Sending score...", 160, 176, 2);

  scoreSent = sendScore();
  M5.Display.fillRect(0, 155, 320, 40, TFT_BLACK);
  M5.Display.setTextColor(scoreSent ? TFT_GREEN : TFT_RED, TFT_BLACK);
  M5.Display.drawString(scoreSent ? "SENT!" : "SEND FAILED", 160, 176, 2);
  if (!scoreSent) {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.drawString(lastSendMessage, 160, 192, 1);
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  pinMode(START_PIN, INPUT_PULLUP);
  pinMode(RESTART_PIN, INPUT_PULLUP);
  pinMode(TARGET_PIN, INPUT_PULLUP);
  ledcSetup(SERVO_PWM_CHANNEL, SERVO_PWM_HZ, SERVO_PWM_BITS);
  ledcAttachPin(SERVO_PIN, SERVO_PWM_CHANNEL);
  setServoAngle(SERVO_IDLE_ANGLE);

  drawGame();
}

uint32_t servoAngleToDuty(int angle) {
  angle = constrain(angle, 0, 180);
  int pulseUs = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  return ((uint64_t)pulseUs * ((1 << SERVO_PWM_BITS) - 1)) / 20000;
}

void setServoAngle(int angle) {
  ledcWrite(SERVO_PWM_CHANNEL, servoAngleToDuty(angle));
}

void startServoSequence() {
  servoSequenceRunning = true;
  servoSequenceIndex = 0;
  int angle = SERVO_SEQUENCE_ANGLES[servoSequenceIndex];
  setServoAngle(angle);
  servoNextStepAt = millis() + SERVO_SEQUENCE_INTERVALS_MS[servoSequenceIndex];
}

bool risingEdge(bool currentState, bool& previousState) {
  bool triggered = currentState && !previousState;
  previousState = currentState;
  return triggered;
}

void registerHit() {
  uint32_t now = millis();
  if (!gameRunning || gamePaused) return;
  if (now - lastHitAt < HIT_COOLDOWN_MS) return;

  lastHitAt = now;
  score += 1;
  drawGame();
  startServoSequence();
}

void updateServo() {
  if (!servoSequenceRunning || millis() < servoNextStepAt) return;

  servoSequenceIndex++;
  if (servoSequenceIndex >= SERVO_SEQUENCE_COUNT) {
    servoSequenceRunning = false;
    return;
  }

  int angle = SERVO_SEQUENCE_ANGLES[servoSequenceIndex];
  setServoAngle(angle);
  servoNextStepAt = millis() + SERVO_SEQUENCE_INTERVALS_MS[servoSequenceIndex];
}

void updateExternalInputs() {
  bool startPressed = digitalRead(START_PIN) == LOW;
  bool restartPressed = digitalRead(RESTART_PIN) == LOW;
  bool targetDetected = digitalRead(TARGET_PIN) == LOW;

  if (risingEdge(startPressed, previousStartState) && !gameRunning) {
    startGame();
  }
  if (risingEdge(restartPressed, previousRestartState)) {
    if (gamePaused) {
      resumeGame();
    } else {
      startGame();
    }
  }
  if (risingEdge(targetDetected, previousTargetState)) {
    registerHit();
  }
}

void loop() {
  M5.update();
  updateExternalInputs();
  updateServo();

  bool buttonAPressed = M5.BtnA.wasPressed();
  if (!gameRunning && buttonAPressed) {
    startGame();
  } else if (gameRunning && gamePaused && buttonAPressed) {
    resumeGame();
  }

  if (gameRunning && !gamePaused) {
    if (M5.BtnB.wasPressed()) {
      registerHit();
    }
    if (M5.BtnC.wasPressed()) {
      score += 5;
      drawGame();
    }

    drawTimeLeft();
    if (millis() - gameStartedAt >= GAME_DURATION_MS) {
      finishGame();
    }
  }

  delay(20);
}
