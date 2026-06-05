/*
 * Douflex - arm motor receiver (WiFi AP + UDP)
 * Protocol: J,x1,y1,x2,y2,mask\n  (UDP 4210, AP DouflexRX / douflex1234)
 *
 * Motors:
 *   BASE      – BTS7960 #3 + 24V DC geared motor  (Joy1 X)
 *   BODY 1    – BTS7960 #1 + 24V DC geared motor  (Joy1 Y)
 *   BODY 2    – BTS7960 #2 + 24V DC geared motor  (Joy2 Y)
 *   MON ROT   – TB6600 + NEMA17 stepper            (Joy2 X via mask bits 2-3)
 *
 * BODY limit switches (MIN + MAX per motor):
 *   - MIN pressed: stop travel toward MIN; reverse still allowed.
 *   - MAX pressed: stop travel toward MAX; reverse still allowed.
 *   Wiring: COM->GND, NO->GPIO, pressed = LOW.
 *   Body1: MIN=GPIO13 MAX=GPIO12 (internal pull-up OK)
 *   Body2: MIN=GPIO36 MAX=GPIO39 — add external 10k to 3.3V on each (input-only pins)
 *
 * Upload: flash THIS board first, then remote_joystick_wifi.
 */

#include <WiFi.h>
#include <WiFiUdp.h>

// =============================================================================
// BODY limit switches — set 0 to disable limits for that motor (motor runs free)
// =============================================================================
#define USE_BODY1_LIMIT_SWITCHES  1   // Body1: GPIO 13 MIN, 12 MAX
#define USE_BODY2_LIMIT_SWITCHES  1   // Body2: GPIO 36 MIN, 39 MAX (wiring verified)

#define PIN_LIMIT_BODY1_MIN      13
#define PIN_LIMIT_BODY1_MAX      12
#define PIN_LIMIT_BODY2_MIN      36
#define PIN_LIMIT_BODY2_MAX      39

// If motor runs wrong way vs MIN/MAX labels, flip (0 or 1)
#define BODY1_JOY_UP_IS_TOWARD_MAX     1
#define BODY2_JOY_UP_IS_TOWARD_MAX     1

// =============================================================================
// ANTI-GRAVITY HOLD — small upward PWM when joystick is centered (prevents arm sag)
// Set HOLD_DUTY to 0 to disable. Increase if arm still drops; decrease if arm creeps up.
// HOLD_DIRECTION: 1 = positive PWM holds up, -1 = negative PWM holds up (match your wiring)
// =============================================================================
#define BODY1_HOLD_DUTY          40    // 0–220; start low, raise until arm stays put
#define BODY2_HOLD_DUTY          40
#define BODY1_HOLD_DIRECTION      1    // 1 or -1 (flip if arm moves down instead of holding)
#define BODY2_HOLD_DIRECTION      1

// ----- WiFi AP (match remote) -----
static const char *kApSsid = "DouflexRX";
static const char *kApPass = "douflex1234"; 
static const uint16_t kUdpPort = 4210;

// ----- TB6600 (monitor rotate only) -----
static const int PIN_TB_MON_ROT_STEP = 23;
static const int PIN_TB_MON_ROT_DIR = 25;
static const int PIN_TB_MON_ROT_ENA = 26;

// ----- BTS7960 #3 (Base – 24V DC geared motor) -----
static const int PIN_BTS_BASE_R_PWM = 18;
static const int PIN_BTS_BASE_L_PWM = 19;
static const int PIN_BTS_BASE_R_EN  = 17;
static const int PIN_BTS_BASE_L_EN  = 5;

// ----- BTS7960 -----
static const int PIN_BTS1_R_PWM = 27;
static const int PIN_BTS1_L_PWM = 14;
static const int PIN_BTS1_R_EN = 32;
static const int PIN_BTS1_L_EN = 33;

static const int PIN_BTS2_R_PWM = 4;
static const int PIN_BTS2_L_PWM = 15;
static const int PIN_BTS2_R_EN = 16;
static const int PIN_BTS2_L_EN = 2;

static const int kAdcCenter = 2048;
static const int kAdcDeadband = 240;

static const uint32_t kLinkTimeoutMs = 2000;
static const uint32_t kStepPulseUs = 5;
static const uint32_t kMonStepIntervalUs = 200;
static const int kPwmMaxDuty = 220;
static const int kLimitEscapeMinDuty = 90;  // min PWM when backing away from a pressed limit
static const uint8_t kLimitDebounceHits = 3;  // consecutive LOW samples before "pressed"
static const int kUdpDrainMaxPerLoop = 8;     // avoid UDP flood starving motor/limit logic

static WiFiUDP udp;

// Debounced limit state — updated once per loop (fast, no delayMicroseconds)
struct LimitSwitchState {
  uint8_t lowStreak;
  bool pressed;
};
static LimitSwitchState s_limB1Min = {0, false};
static LimitSwitchState s_limB1Max = {0, false};
static LimitSwitchState s_limB2Min = {0, false};
static LimitSwitchState s_limB2Max = {0, false};

static volatile int g_x1 = kAdcCenter;
static volatile int g_y1 = kAdcCenter;
static volatile int g_x2 = kAdcCenter;
static volatile int g_y2 = kAdcCenter;
static volatile int g_mask = 0;
static volatile uint32_t g_lastPacketMs = 0;
static IPAddress s_lastRemoteIp(0, 0, 0, 0);
static uint16_t s_lastRemotePort = 0;

static uint32_t lastOkLogMs = 0;
static int lastLogX1 = -1, lastLogY1 = -1, lastLogX2 = -1, lastLogY2 = -1; 
static int lastLogMask = -1;
static uint8_t s_limitEventMask = 0;
static uint32_t s_lastLimitFbMs = 0;

static uint32_t s_lastMonRotStepUs = 0;
static bool s_warnedNoLink = false;

enum BodyId { BODY_1 = 0, BODY_2 = 1 };

static bool s_body1LimitsLive = (USE_BODY1_LIMIT_SWITCHES != 0);
static bool s_body2LimitsLive = (USE_BODY2_LIMIT_SWITCHES != 0);

static inline bool joyActive(int v) {
  return v < (kAdcCenter - kAdcDeadband) || v > (kAdcCenter + kAdcDeadband);
}

static bool parseLine(const char *line, int *x1, int *y1, int *x2, int *y2, int *mask) {
  if (line[0] != 'J' || line[1] != ',') return false;
  int m = 0;
  if (sscanf(line + 2, "%d,%d,%d,%d,%d", x1, y1, x2, y2, &m) != 5) return false;
  if (m < 0 || m > 15) return false;
  *mask = m;
  return true;
}

static bool shouldLogOk(uint32_t now, int x1, int y1, int x2, int y2, int mask) {
  const bool joy = joyActive(x1) || joyActive(y1) || joyActive(x2) || joyActive(y2);
  if (mask != 0 || joy) return true;
  if (mask != lastLogMask || x1 != lastLogX1 || y1 != lastLogY1 || x2 != lastLogX2 ||
      y2 != lastLogY2) {
    return true;
  }
  if (now - lastOkLogMs >= 3000) return true;
  return false;
}

static bool bodyLimitsActive(BodyId body) {
  return (body == BODY_1) ? s_body1LimitsLive : s_body2LimitsLive;
}

static const char *bodyName(BodyId body) {
  return (body == BODY_1) ? "body1" : "body2";
}

static void limitDebouncerStep(LimitSwitchState *st, int pin) {
  if (pin < 0) {
    st->lowStreak = 0;
    st->pressed = false;
    return;
  }
  if (digitalRead(pin) == LOW) {
    if (st->lowStreak < kLimitDebounceHits + 2) st->lowStreak++;
  } else if (st->lowStreak > 0) {
    st->lowStreak--;
  }
  st->pressed = (st->lowStreak >= kLimitDebounceHits);
}

static void updateAllLimitStates() {
  if (USE_BODY1_LIMIT_SWITCHES && s_body1LimitsLive) {
    limitDebouncerStep(&s_limB1Min, PIN_LIMIT_BODY1_MIN);
    limitDebouncerStep(&s_limB1Max, PIN_LIMIT_BODY1_MAX);
  } else {
    s_limB1Min.pressed = s_limB1Max.pressed = false;
  }
  if (USE_BODY2_LIMIT_SWITCHES && s_body2LimitsLive) {
    limitDebouncerStep(&s_limB2Min, PIN_LIMIT_BODY2_MIN);
    limitDebouncerStep(&s_limB2Max, PIN_LIMIT_BODY2_MAX);
  } else {
    s_limB2Min.pressed = s_limB2Max.pressed = false;
  }
}

static bool limitMinPressed(BodyId body) {
  if (!bodyLimitsActive(body)) return false;
  return (body == BODY_1) ? s_limB1Min.pressed : s_limB2Min.pressed;
}

static bool limitMaxPressed(BodyId body) {
  if (!bodyLimitsActive(body)) return false;
  return (body == BODY_1) ? s_limB1Max.pressed : s_limB2Max.pressed;
}

// Boot-only: quick triple read (not in main loop)
static bool limitSwitchRawLowBoot(int pin) {
  if (pin < 0) return false;
  int lows = 0;
  for (int i = 0; i < 3; ++i) {
    if (digitalRead(pin) == LOW) lows++;
    delayMicroseconds(200);
  }
  return lows >= 2;
}

static void bodyLimitPinsInit() {
  if (USE_BODY1_LIMIT_SWITCHES) {
    if (PIN_LIMIT_BODY1_MIN >= 0) pinMode(PIN_LIMIT_BODY1_MIN, INPUT_PULLUP);
    if (PIN_LIMIT_BODY1_MAX >= 0) pinMode(PIN_LIMIT_BODY1_MAX, INPUT_PULLUP);
  }
  if (USE_BODY2_LIMIT_SWITCHES) {
    if (PIN_LIMIT_BODY2_MIN >= 0) pinMode(PIN_LIMIT_BODY2_MIN, INPUT_PULLUP);
    if (PIN_LIMIT_BODY2_MAX >= 0) pinMode(PIN_LIMIT_BODY2_MAX, INPUT_PULLUP);
  }
}

static void bodyLimitBootCheck(BodyId body, int pinMin, int pinMax, bool compileEnable,
                              bool *limitsLive) {
  if (!compileEnable) {
    *limitsLive = false;
    Serial.printf("[LIM] %s: limits OFF (#define disabled)\n", bodyName(body));
    return;
  }

  *limitsLive = true;
  delay(15);

  const bool minPressed = limitSwitchRawLowBoot(pinMin);
  const bool maxPressed = limitSwitchRawLowBoot(pinMax);

  if (minPressed && maxPressed) {
    *limitsLive = false;
    Serial.printf("[LIM] %s: BOTH switches LOW at boot — limits DISABLED (short/wiring)\n",
                  bodyName(body));
  } else {
    if (minPressed) {
      Serial.printf("[LIM] %s: MIN active at boot — toward-MIN blocked until switch opens\n",
                    bodyName(body));
    }
    if (maxPressed) {
      Serial.printf("[LIM] %s: MAX active at boot — toward-MAX blocked until switch opens\n",
                    bodyName(body));
    }
  }

  Serial.printf("[LIM] %s: gpio MIN=%d MAX=%d  read min=%d max=%d  limits=%s\n", bodyName(body),
                pinMin, pinMax, minPressed ? 1 : 0, maxPressed ? 1 : 0,
                *limitsLive ? "ON" : "OFF");
}

static void bodyLimitSanityCheck() {
  Serial.println("[LIM] Bidirectional travel: MIN stops backward, MAX stops forward, reverse OK");
  bodyLimitBootCheck(BODY_1, PIN_LIMIT_BODY1_MIN, PIN_LIMIT_BODY1_MAX, USE_BODY1_LIMIT_SWITCHES,
                     &s_body1LimitsLive);
  bodyLimitBootCheck(BODY_2, PIN_LIMIT_BODY2_MIN, PIN_LIMIT_BODY2_MAX, USE_BODY2_LIMIT_SWITCHES,
                     &s_body2LimitsLive);
}

static void bodyLimitPins(BodyId body, int *pinMin, int *pinMax) {
  if (body == BODY_1) {
    *pinMin = PIN_LIMIT_BODY1_MIN;
    *pinMax = PIN_LIMIT_BODY1_MAX;
  } else {
    *pinMin = PIN_LIMIT_BODY2_MIN;
    *pinMax = PIN_LIMIT_BODY2_MAX;
  }
}

static bool bodyJoyUpIsTowardMax(BodyId body) {
  return (body == BODY_1) ? (BODY1_JOY_UP_IS_TOWARD_MAX != 0) : (BODY2_JOY_UP_IS_TOWARD_MAX != 0);
}

// True when signed motor cmd (positive = R_PWM) moves toward MAX end.
static bool cmdIsTowardMax(BodyId body, int16_t cmd) {
  if (cmd == 0) return false;
  return (cmd > 0) ? bodyJoyUpIsTowardMax(body) : !bodyJoyUpIsTowardMax(body);
}

static void logBodyLimitBlock(BodyId body, bool atMax) {
  static uint32_t lastLogMs[2] = {0, 0};
  const uint32_t ms = millis();
  const int idx = (int)body;
  if (ms - lastLogMs[idx] < 1500) return;
  lastLogMs[idx] = ms;
  Serial.printf("[LIM] %s blocked at %s switch\n", bodyName(body), atMax ? "MAX" : "MIN");
}

static uint8_t limitEventBit(BodyId body, bool atMax) {
  if (body == BODY_1) return atMax ? 0x02 : 0x01;
  return atMax ? 0x08 : 0x04;
}

static void notifyLimitEvent(BodyId body, bool atMax) {
  s_limitEventMask |= limitEventBit(body, atMax);
}

// Final safety gate: never drive into a pressed limit (uses debounced cache + motor cmd direction).
static int16_t clampMotorCmdByLimits(BodyId body, int16_t cmd) {
  if (cmd == 0 || !bodyLimitsActive(body)) return cmd;

  const bool towardMax = cmdIsTowardMax(body, cmd);
  if (towardMax && limitMaxPressed(body)) {
    notifyLimitEvent(body, true);
    logBodyLimitBlock(body, true);
    return 0;
  }
  if (!towardMax && limitMinPressed(body)) {
    notifyLimitEvent(body, false);
    logBodyLimitBlock(body, false);
    return 0;
  }
  return cmd;
}

// Stop hold PWM from pushing into a pressed limit.
static int16_t applyHoldLimitGating(BodyId body, int16_t hold) {
  return clampMotorCmdByLimits(body, hold);
}

// Stronger PWM when backing away from a pressed limit.
static int16_t boostLimitEscape(BodyId body, int16_t cmd) {
  if (cmd == 0 || !bodyLimitsActive(body)) return cmd;

  const bool towardMax = cmdIsTowardMax(body, cmd);
  const bool leavingMin = towardMax && limitMinPressed(body);
  const bool leavingMax = !towardMax && limitMaxPressed(body);
  if (!leavingMin && !leavingMax) return cmd;

  const int16_t absCmd = cmd > 0 ? cmd : (int16_t)(-cmd);
  if (absCmd >= kLimitEscapeMinDuty) return cmd;
  return cmd > 0 ? (int16_t)kLimitEscapeMinDuty : (int16_t)(-kLimitEscapeMinDuty);
}

static void bodyMotorPwmWrite(BodyId body, int pinR, int pinL, int16_t cmd) {
  cmd = clampMotorCmdByLimits(body, cmd);
  bridgePwmWrite(pinR, pinL, cmd);
}

static void sendLimitFeedbackIfAny() {
  if (s_limitEventMask == 0 || s_lastRemotePort == 0) return;
  const uint32_t now = millis();
  if (now - s_lastLimitFbMs < 50) return;
  s_lastLimitFbMs = now;
  char line[24];
  const uint8_t mask = s_limitEventMask;
  s_limitEventMask = 0;
  snprintf(line, sizeof(line), "L,%u\n", mask);
  udp.beginPacket(s_lastRemoteIp, s_lastRemotePort);
  udp.write((const uint8_t *)line, strlen(line));
  udp.endPacket();
}

static void tb6600PinsInit(int stepPin, int dirPin, int enaPin) {
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enaPin, OUTPUT);
  digitalWrite(stepPin, LOW);
  digitalWrite(dirPin, LOW);
  digitalWrite(enaPin, LOW);
}

static void pulseStep(int stepPin) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(kStepPulseUs);
  digitalWrite(stepPin, LOW);
}

static void bridgePwmWrite(int pinR, int pinL, int16_t cmd) {
  cmd = constrain(cmd, -kPwmMaxDuty, kPwmMaxDuty);
  if (cmd > 0) {
    analogWrite(pinL, 0);
    analogWrite(pinR, cmd);
  } else if (cmd < 0) {
    analogWrite(pinR, 0);
    analogWrite(pinL, -cmd);
  } else {
    analogWrite(pinR, 0);
    analogWrite(pinL, 0);
  }
}

static void serviceBaseDc(int x1, bool linkOk) {
  if (!linkOk) {
    bridgePwmWrite(PIN_BTS_BASE_R_PWM, PIN_BTS_BASE_L_PWM, 0);
    if (!s_warnedNoLink) {
      s_warnedNoLink = true;
      Serial.println("[BASE] no UDP from remote");
    }
    return;
  }
  s_warnedNoLink = false;

  const int d = x1 - kAdcCenter;
  const int ad = d > 0 ? d : -d;
  if (ad < kAdcDeadband) {
    bridgePwmWrite(PIN_BTS_BASE_R_PWM, PIN_BTS_BASE_L_PWM, 0);
    return;
  }

  const int span = 2048 - kAdcDeadband;
  int mag = (int)((int64_t)ad * kPwmMaxDuty / span);
  if (mag > kPwmMaxDuty) mag = kPwmMaxDuty;

  int16_t cmd = (d > 0) ? (int16_t)mag : (int16_t)(-mag);
  bridgePwmWrite(PIN_BTS_BASE_R_PWM, PIN_BTS_BASE_L_PWM, cmd);
}

static void serviceMonRotStepper(uint8_t mask, bool linkOk) {
  if (!linkOk) return;
  const bool port = (mask & 0x04) != 0;
  const bool land = (mask & 0x08) != 0;
  if (port == land) return;

  digitalWrite(PIN_TB_MON_ROT_DIR, port ? HIGH : LOW);
  uint32_t now = micros();
  if ((uint32_t)(now - s_lastMonRotStepUs) >= kMonStepIntervalUs) {
    s_lastMonRotStepUs = now;
    pulseStep(PIN_TB_MON_ROT_STEP);
  }
}

static int16_t bodyHoldDuty(BodyId body) {
  if (body == BODY_1) return (int16_t)(BODY1_HOLD_DUTY * BODY1_HOLD_DIRECTION);
  return (int16_t)(BODY2_HOLD_DUTY * BODY2_HOLD_DIRECTION);
}

static void serviceBts7960(int y, int pinR, int pinL, BodyId body, bool linkOk) {
  const int16_t hold = bodyHoldDuty(body);

  if (!linkOk) {
    bodyMotorPwmWrite(body, pinR, pinL, applyHoldLimitGating(body, hold));
    return;
  }

  const int d = y - kAdcCenter;
  const int ad = d > 0 ? d : -d;
  if (ad < kAdcDeadband) {
    bodyMotorPwmWrite(body, pinR, pinL, applyHoldLimitGating(body, hold));
    return;
  }

  const int span = 2048 - kAdcDeadband;
  int mag = (int)((int64_t)ad * kPwmMaxDuty / span);
  if (mag > kPwmMaxDuty) mag = kPwmMaxDuty;

  int16_t cmd = (d > 0) ? (int16_t)mag : (int16_t)(-mag);
  cmd = boostLimitEscape(body, cmd);
  bodyMotorPwmWrite(body, pinR, pinL, cmd);
}

static void motorOutputsStop() {
  bridgePwmWrite(PIN_BTS_BASE_R_PWM, PIN_BTS_BASE_L_PWM, 0);
  bodyMotorPwmWrite(BODY_1, PIN_BTS1_R_PWM, PIN_BTS1_L_PWM,
                    applyHoldLimitGating(BODY_1, bodyHoldDuty(BODY_1)));
  bodyMotorPwmWrite(BODY_2, PIN_BTS2_R_PWM, PIN_BTS2_L_PWM,
                    applyHoldLimitGating(BODY_2, bodyHoldDuty(BODY_2)));
}

static void drainUdpAndUpdate() {
  int n;
  int drained = 0;
  while ((n = udp.parsePacket()) > 0 && drained < kUdpDrainMaxPerLoop) {
    drained++;
    char buf[128];
    if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;
    int len = udp.read(buf, n);
    if (len <= 0) continue;
    buf[len] = '\0';
    char *lastNl = strrchr(buf, '\n');
    if (lastNl) *lastNl = '\0';

    int x1, y1, x2, y2, mask;
    if (!parseLine(buf, &x1, &y1, &x2, &y2, &mask)) {
      continue;
    }
    g_x1 = x1;
    g_y1 = y1;
    g_x2 = x2;
    g_y2 = y2;
    g_mask = mask;
    g_lastPacketMs = millis();
    s_lastRemoteIp = udp.remoteIP();
    s_lastRemotePort = udp.remotePort();

    const uint32_t now = millis();
    if (shouldLogOk(now, x1, y1, x2, y2, mask)) {
      lastOkLogMs = now;
      lastLogX1 = x1;
      lastLogY1 = y1;
      lastLogX2 = x2;
      lastLogY2 = y2;
      lastLogMask = mask;
      Serial.printf("OK x1=%d y1=%d x2=%d y2=%d mask=%d\n", x1, y1, x2, y2, mask);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  bodyLimitPinsInit();
  bodyLimitSanityCheck();

  // TB6600 — monitor rotate only
  tb6600PinsInit(PIN_TB_MON_ROT_STEP, PIN_TB_MON_ROT_DIR, PIN_TB_MON_ROT_ENA);

  // BTS7960 — Base
  pinMode(PIN_BTS_BASE_R_EN, OUTPUT);
  pinMode(PIN_BTS_BASE_L_EN, OUTPUT);
  digitalWrite(PIN_BTS_BASE_R_EN, HIGH);
  digitalWrite(PIN_BTS_BASE_L_EN, HIGH);
  analogWriteResolution(PIN_BTS_BASE_R_PWM, 8);
  analogWriteResolution(PIN_BTS_BASE_L_PWM, 8);

  // BTS7960 — Body 1 & 2
  pinMode(PIN_BTS1_R_EN, OUTPUT);
  pinMode(PIN_BTS1_L_EN, OUTPUT);
  pinMode(PIN_BTS2_R_EN, OUTPUT);
  pinMode(PIN_BTS2_L_EN, OUTPUT);
  digitalWrite(PIN_BTS1_R_EN, HIGH);
  digitalWrite(PIN_BTS1_L_EN, HIGH);
  digitalWrite(PIN_BTS2_R_EN, HIGH);
  digitalWrite(PIN_BTS2_L_EN, HIGH);
  analogWriteResolution(PIN_BTS1_R_PWM, 8);
  analogWriteResolution(PIN_BTS1_L_PWM, 8);
  analogWriteResolution(PIN_BTS2_R_PWM, 8);
  analogWriteResolution(PIN_BTS2_L_PWM, 8);

  motorOutputsStop();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(kApSsid, kApPass)) {
    Serial.println("softAP failed");
    while (true) delay(1000);
  }
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  WiFi.setSleep(WIFI_PS_NONE);  // lower control latency on arm receiver
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  if (!udp.begin(kUdpPort)) {
    Serial.println("UDP begin failed");
    while (true) delay(1000);
  }

  g_lastPacketMs = 0;
  Serial.printf("Arm UDP %u | body1 limits=%s  body2 limits=%s\n", kUdpPort,
                s_body1LimitsLive ? "ON" : "OFF", s_body2LimitsLive ? "ON" : "OFF");
  Serial.println("Motors: base=Joy1 X (DC)  body1=Joy1 Y  body2=Joy2 Y  monRot=Joy2 X (stepper)");
}

void loop() {
  updateAllLimitStates();
  drainUdpAndUpdate();

  const uint32_t nowMs = millis();
  const bool linkOk = (nowMs - g_lastPacketMs) <= kLinkTimeoutMs;
  if (!linkOk) {
    motorOutputsStop();
  }

  const int x1 = g_x1;
  const int y1 = g_y1;
  const int y2 = g_y2;
  const uint8_t mask = (uint8_t)g_mask;

  serviceBaseDc(x1, linkOk);
  serviceMonRotStepper(mask, linkOk);

  serviceBts7960(y1, PIN_BTS1_R_PWM, PIN_BTS1_L_PWM, BODY_1, linkOk);
  serviceBts7960(y2, PIN_BTS2_R_PWM, PIN_BTS2_L_PWM, BODY_2, linkOk);
  sendLimitFeedbackIfAny();
}
