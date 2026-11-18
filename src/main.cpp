// StickC Motion Face - 4 animated characters that react to the motion sensor (M5StickC).
// The face is drawn in a 128x128 canvas and scaled to the screen width; the area below shows
// the character name, mood and battery.
//  moods  : idle (blink/glance/follow tilt), excited (moved), dizzy (shaken),
//           scared (free fall), sleeping (still for ~25 s)
//  button : click = next character (pop animation), hold = change colour
#include <M5Unified.h>
#include <Preferences.h>
#include <math.h>

#define FW_VERSION "1.0.0"

#ifndef LOOK_X_SIGN
#define LOOK_X_SIGN 1.0f
#endif
#ifndef LOOK_Y_SIGN
#define LOOK_Y_SIGN -1.0f
#endif

static const float SHAKE_ENERGY = 8.0f;
static const float MOVE_ENERGY = 2.5f;
static const uint32_t SLEEP_AFTER_MS = 25000;
static const uint32_t DROWSY_FROM_MS = 12000;

static M5Canvas cv(&M5.Display);   // 128x128 face
static M5Canvas out(&M5.Display);  // full screen: scaled face + info panel
static const char* MOOD_NAMES[] = {"idle", "happy", "dizzy", "scared", "asleep"};
static Preferences prefs;

enum Mood : uint8_t { IDLE, EXCITED, DIZZY, SCARED, SLEEPING };
enum Character : uint8_t { CAT, ROBOT, GHOST, CYCLOPS, NUM_CHARS };
static const char* CHAR_NAMES[] = {"CAT", "ROBO", "BOO", "CYCLO"};

static const uint8_t THEMES[][3] = {
    {80, 200, 255}, {255, 205, 70}, {255, 120, 170},
    {120, 230, 130}, {255, 150, 80}, {180, 140, 255}};
static const int NUM_THEMES = sizeof(THEMES) / sizeof(THEMES[0]);

static int theme = 0;
static uint8_t character = CAT;
static Mood mood = IDLE;

static float ax = 0, ay = 0, az = 1, pax = 0, pay = 0, paz = 1;
static float baseX = 0, baseY = 0;
static float energy = 0;
static uint32_t lastActive = 0, dizzyUntil = 0, scaredUntil = 0, excitedUntil = 0;

static float lookX = 0, lookY = 0, glanceX = 0, glanceY = 0;
static uint32_t nextGlance = 0, nextBlink = 2000, blinkStart = 0;
static bool blinking = false;
static float lidBase = 0;
static uint32_t lastFrame = 0, lastLog = 0;
static uint32_t fxStart = 0, nameUntil = 0;
static int forcedMood = -1;       // -1 = automatic (from the IMU)
static bool shotRequested = false;

// per-frame drawing state
static uint32_t gNow;
static int X, Y;
static float gLid;
static uint16_t cBody, cNavy, cPink, cBg;

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
static inline uint16_t rgb(int r, int g, int b) { return cv.color565(r, g, b); }

// bend > 0 -> smile, bend < 0 -> ^ shape
static void curve(int cx, int cy, int w, float bend, int th, uint16_t col) {
  for (int x = -w; x <= w; x++) {
    float u = (float)x / w;
    cv.fillCircle(cx + x, cy + (int)(bend * (1 - u * u)), th / 2, col);
  }
}

static void spiral(int cx, int cy, int R, float rot, uint16_t col) {
  cv.fillCircle(cx, cy, R, TFT_WHITE);
  for (float a = 0; a < 12.5f; a += 0.25f) {
    float r = a * (R - 1) / 12.5f;
    cv.fillCircle(cx + (int)(cosf(a + rot) * r), cy + (int)(sinf(a + rot) * r), 1, col);
  }
}

static void star(int x, int y, uint16_t col) {
  cv.drawLine(x - 4, y, x + 4, y, col);
  cv.drawLine(x, y - 4, x, y + 4, col);
  cv.fillCircle(x, y, 2, col);
}

// Round eye used by cat / ghost / cyclops. navyEye: solid dark eye (ghost).
static void eye(int ex, int ey, int rx, int ry, int side, bool navyEye) {
  if (mood == DIZZY) {
    spiral(ex, ey, rx + 1, gNow * 0.015f * -side, cNavy);
  } else if (mood == EXCITED) {
    curve(ex, ey + 4, rx - 2, -9, 4, cNavy);
  } else if (mood == SCARED) {
    cv.fillEllipse(ex, ey, rx + 2, ry + 3, TFT_WHITE);
    cv.fillCircle(ex + (int)(sinf(gNow * 0.09f) * 1.5f), ey, 3, cNavy);
  } else if (gLid >= 0.9f) {
    curve(ex, ey + 2, rx - 2, 5, 4, cNavy);
  } else {
    int pr = rx * 55 / 100 + 1;
    if (navyEye) {
      cv.fillEllipse(ex, ey, rx, ry, cNavy);
      int px = ex + (int)(lookX * 3), py = ey + (int)(lookY * 3);
      cv.fillCircle(px - 2, py - 3, 3, TFT_WHITE);
    } else {
      cv.fillEllipse(ex, ey, rx, ry, TFT_WHITE);
      int px = ex + (int)(lookX * (rx - pr - 1));
      int py = ey + (int)(lookY * (ry - pr - 1));
      cv.fillCircle(px, py, pr, cNavy);
      cv.fillCircle(px - pr / 3, py - pr / 3, pr / 3 + 1, TFT_WHITE);
      cv.fillCircle(px + pr / 3, py + pr / 3, 1, TFT_WHITE);
    }
    if (gLid > 0.01f)
      cv.fillRect(ex - rx - 2, ey - ry - 2, 2 * rx + 4, (int)(gLid * (2 * ry + 2)), cBody);
  }
}

// kind 0 = smile, 1 = small "o"
static void mouth(int cx, int my, int w, int kind) {
  switch (mood) {
    case EXCITED:
      cv.fillEllipse(cx, my, w * 7 / 10, w * 65 / 100, cNavy);
      if (kind == 0) cv.fillEllipse(cx, my + w / 3, w * 4 / 10, w * 28 / 100, rgb(255, 90, 110));
      break;
    case DIZZY:
      for (int x = -w; x <= w; x++)
        cv.fillCircle(cx + x, my + (int)(3 * sinf(x * 0.5f + gNow * 0.02f)), 1, cNavy);
      break;
    case SCARED:
      cv.fillEllipse(cx, my + 2, w * 55 / 100, w * 75 / 100, cNavy);
      break;
    case SLEEPING:
      cv.fillEllipse(cx, my, w * 3 / 10 + 1, 3 + (int)(sinf(gNow * 0.002f) * 1.5f), cNavy);
      break;
    default:
      if (kind == 1) cv.fillEllipse(cx, my, 4, 5, cNavy);
      else curve(cx, my - 2, w, 6, 3, cNavy);
      break;
  }
}

static void drawCat() {
  float earDy = mood == EXCITED ? sinf(gNow * 0.03f) * 2 : 0;
  int ed = (int)earDy;
  cv.fillTriangle(28 + X, 28 + Y, 34 + X, 4 + Y + ed, 56 + X, 16 + Y, cBody);
  cv.fillTriangle(100 + X, 28 + Y, 94 + X, 4 + Y + ed, 72 + X, 16 + Y, cBody);
  cv.fillTriangle(36 + X, 22 + Y, 38 + X, 10 + Y + ed, 48 + X, 17 + Y, cPink);
  cv.fillTriangle(92 + X, 22 + Y, 90 + X, 10 + Y + ed, 80 + X, 17 + Y, cPink);
  cv.fillRoundRect(8 + X, 10 + Y, 112, 104, 42, cBody);
  cv.fillEllipse(24 + X, 80 + Y, 7, 4, cPink);
  cv.fillEllipse(104 + X, 80 + Y, 7, 4, cPink);
  eye(40 + X, 58 + Y, 15, 18, -1, false);
  eye(88 + X, 58 + Y, 15, 18, 1, false);
  mouth(64 + X, 88 + Y, 14, 0);
  // whiskers
  if (mood != SLEEPING) {
    uint16_t w = cNavy;
    cv.drawLine(4 + X, 78 + Y, 14 + X, 80 + Y, w);
    cv.drawLine(4 + X, 86 + Y, 14 + X, 85 + Y, w);
    cv.drawLine(124 + X, 78 + Y, 114 + X, 80 + Y, w);
    cv.drawLine(124 + X, 86 + Y, 114 + X, 85 + Y, w);
  }
}

static void drawRobot() {
  const uint8_t* t = THEMES[theme];
  uint16_t dark = rgb(t[0] * 55 / 100, t[1] * 55 / 100, t[2] * 55 / 100);
  uint16_t screen = rgb(10, 18, 28);
  uint16_t light = cBody;

  // antenna light
  bool on;
  if (mood == EXCITED) on = (gNow / 120) % 2;
  else if (mood == DIZZY) on = (gNow / 70) % 2;
  else if (mood == SLEEPING) on = false;
  else on = (gNow / 700) % 2;
  cv.fillRect(63 + X, 10 + Y, 3, 14, dark);
  cv.fillCircle(64 + X, 8 + Y, 5, on ? rgb(255, 70, 70) : dark);
  // ear bolts + head
  cv.fillRoundRect(4 + X, 50 + Y, 10, 26, 3, dark);
  cv.fillRoundRect(114 + X, 50 + Y, 10, 26, 3, dark);
  cv.fillRoundRect(14 + X, 22 + Y, 100, 86, 14, cBody);
  cv.fillRoundRect(24 + X, 32 + Y, 80, 66, 8, screen);

  int ox = (int)(lookX * 4), oy = (int)(lookY * 3);
  for (int side = -1; side <= 1; side += 2) {
    int ex = 64 + X + side * 17 + ox, ey = 58 + Y + oy;
    if (mood == DIZZY) {
      for (int k = -1; k <= 1; k++) {
        cv.drawLine(ex - 9 + k, ey - 9, ex + 9 + k, ey + 9, light);
        cv.drawLine(ex - 9 + k, ey + 9, ex + 9 + k, ey - 9, light);
      }
    } else if (mood == EXCITED) {
      curve(ex, ey + 5, 11, -9, 4, light);
    } else if (mood == SCARED) {
      cv.fillRoundRect(ex - 13, ey - 15, 26, 30, 4, light);
      cv.fillRect(ex - 3, ey - 3, 6, 6, screen);
    } else if (gLid >= 0.9f) {
      cv.fillRect(ex - 10, ey, 20, 3, light);
    } else {
      cv.fillRoundRect(ex - 11, ey - 12, 22, 24, 4, light);
      cv.fillRect(ex - 8, ey - 9, 4, 4, TFT_WHITE);
      if (gLid > 0.01f) cv.fillRect(ex - 12, ey - 13, 24, (int)(gLid * 26), screen);
    }
  }
  int mx = 64 + X + ox / 2, my = 84 + Y;
  switch (mood) {
    case EXCITED: curve(mx, my - 2, 16, 8, 4, light); break;
    case DIZZY:
      for (int x = -16; x <= 16; x += 2)
        cv.fillRect(mx + x, my + (((x + 16) / 4) % 2) * 4, 2, 3, light);
      break;
    case SCARED:
      cv.fillEllipse(mx, my + 2, 9, 8, light);
      cv.fillEllipse(mx, my + 2, 6, 5, screen);
      break;
    case SLEEPING: cv.fillRect(mx - 8, my + 2, 16, 2, light); break;
    default:
      for (int i = 0; i < 5; i++) cv.fillRect(mx - 18 + i * 8, my, 6, 3, light);
      break;
  }
}

static void drawGhost() {
  int gy = Y + (int)(sinf(gNow * 0.003f) * 4);
  float arm = mood == EXCITED ? -22 : (mood == DIZZY ? sinf(gNow * 0.02f) * 8 : sinf(gNow * 0.004f) * 3);
  cv.fillEllipse(18 + X, 78 + gy + (int)arm, 8, 12, cBody);
  cv.fillEllipse(110 + X, 78 + gy + (int)arm, 8, 12, cBody);
  cv.fillCircle(64 + X, 50 + gy, 40, cBody);
  cv.fillRect(24 + X, 50 + gy, 80, 50, cBody);
  for (int i = 0; i < 4; i++)
    cv.fillCircle(34 + i * 20 + X, 100 + gy + (int)(sinf(gNow * 0.012f + i * 1.5f) * 3), 10, cBody);
  cv.fillEllipse(30 + X, 68 + gy, 7, 4, cPink);
  cv.fillEllipse(98 + X, 68 + gy, 7, 4, cPink);
  int saveY = Y;
  Y = gy;
  eye(46 + X, 48 + gy, 9, 12, -1, true);
  eye(82 + X, 48 + gy, 9, 12, 1, true);
  Y = saveY;
  mouth(64 + X, 74 + gy, 10, 1);
}

static void drawCyclops() {
  const uint8_t* t = THEMES[theme];
  uint16_t dark = rgb(t[0] * 60 / 100, t[1] * 60 / 100, t[2] * 60 / 100);
  float wig = mood == EXCITED ? sinf(gNow * 0.03f) * 3 : 0;
  cv.fillTriangle(28 + X, 34 + Y, 24 + X, 6 + Y, 48 + X, 20 + Y, dark);
  cv.fillTriangle(100 + X, 34 + Y, 104 + X, 6 + Y, 80 + X, 20 + Y, dark);
  cv.fillEllipse(14 + X, 80 + Y + (int)wig, 9, 13, cBody);
  cv.fillEllipse(114 + X, 80 + Y - (int)wig, 9, 13, cBody);
  cv.fillCircle(64 + X, 66 + Y, 52, cBody);
  cv.fillEllipse(30 + X, 84 + Y, 8, 4, cPink);
  cv.fillEllipse(98 + X, 84 + Y, 8, 4, cPink);
  eye(64 + X, 52 + Y, 26, 28, 1, false);
  mouth(64 + X, 92 + Y, 24, 0);
  if (mood == IDLE) {
    cv.fillTriangle(50 + X, 98 + Y, 58 + X, 99 + Y, 54 + X, 107 + Y, TFT_WHITE);
    cv.fillTriangle(78 + X, 98 + Y, 70 + X, 99 + Y, 74 + X, 107 + Y, TFT_WHITE);
  } else if (mood == EXCITED) {
    cv.fillRect(54 + X, 84 + Y, 20, 4, TFT_WHITE);
  }
}

static void readMotion(uint32_t now) {
  if (M5.Imu.update()) {
    pax = ax; pay = ay; paz = az;
    M5.Imu.getAccel(&ax, &ay, &az);
  }
  float dx = ax - pax, dy = ay - pay, dz = az - paz;
  float jerk = sqrtf(dx * dx + dy * dy + dz * dz);
  energy = energy * 0.93f + jerk;
  float mag = sqrtf(ax * ax + ay * ay + az * az);

  // baseline = resting orientation, so only tilt *changes* steer the eyes
  baseX += (ax - baseX) * 0.006f;
  baseY += (ay - baseY) * 0.006f;

  if (energy > 0.8f) lastActive = now;
  if (energy > SHAKE_ENERGY) dizzyUntil = now + 2200;
  if (mag < 0.35f) scaredUntil = now + 900;
  if (energy > MOVE_ENERGY) excitedUntil = now + 700;

  uint32_t still = now - lastActive;
  if (now < dizzyUntil) mood = DIZZY;
  else if (now < scaredUntil) mood = SCARED;
  else if (now < excitedUntil) mood = EXCITED;
  else if (still > SLEEP_AFTER_MS) mood = SLEEPING;
  else mood = IDLE;
  if (forcedMood >= 0) mood = (Mood)forcedMood;  // serial "mood N" test command
}

static void updateEyes(uint32_t now) {
  if (now > nextGlance) {
    glanceX = (random(-60, 61)) / 100.0f;
    glanceY = (random(-30, 41)) / 100.0f;
    nextGlance = now + random(1200, 3500);
  }
  float tx = clampf((ax - baseX) * 2.0f * LOOK_X_SIGN, -1, 1);
  float ty = clampf((ay - baseY) * 2.0f * LOOK_Y_SIGN, -1, 1);
  if (fabsf(tx) < 0.15f && fabsf(ty) < 0.15f) { tx = glanceX; ty = glanceY; }
  lookX += (tx - lookX) * 0.15f;
  lookY += (ty - lookY) * 0.15f;

  if (!blinking && now > nextBlink) { blinking = true; blinkStart = now; }
  uint32_t still = now - lastActive;
  float drowsy = clampf((float)((int32_t)still - (int32_t)DROWSY_FROM_MS) /
                            (SLEEP_AFTER_MS - DROWSY_FROM_MS), 0, 1);
  float target = mood == SLEEPING ? 1.0f : 0.55f * drowsy;
  lidBase += (target - lidBase) * 0.08f;
}

static float blinkLid(uint32_t now) {
  if (!blinking) return 0;
  uint32_t t = now - blinkStart;
  if (t >= 160) {
    blinking = false;
    nextBlink = now + (random(0, 4) == 0 ? 250 : random(2000, 5000));
    return 0;
  }
  return sinf(PI * t / 160.0f);
}

static void startPop(uint32_t now) {
  fxStart = now;
  nameUntil = now + 1300;
  excitedUntil = now + 900;
  lastActive = now;
}

static void present(uint32_t now) {
  const int W = out.width(), H = out.height();
  const int cx = W / 2;
  const float z = (float)W / 128.0f;
  out.fillSprite(cBg);
  cv.setPivot(64, 64);
  cv.pushRotateZoomWithAA(&out, W / 2.0f, W / 2.0f, 0, z, z);

  const int py = W, ph = H - W;  // info panel below the face
  out.setTextDatum(textdatum_t::middle_center);
  out.setTextColor(cBody);
  out.setTextSize(W > 100 ? 3 : 2);
  out.drawString(CHAR_NAMES[character], cx, py + ph * 22 / 100);
  out.setTextColor(TFT_WHITE);
  out.setTextSize(1);
  out.drawString(MOOD_NAMES[mood], cx, py + ph * 44 / 100);
  int bat = (int)M5.Power.getBatteryLevel();
  if (bat < 0) bat = 0;
  if (bat > 100) bat = 100;
  char b[16];
  snprintf(b, sizeof b, "bat %d%%", bat);
  out.setTextColor(rgb(140, 152, 184));
  out.drawString(b, cx, py + ph * 60 / 100);
  int bw = W - 20, by = py + ph * 70 / 100;
  out.drawRect(10, by, bw, 7, rgb(140, 152, 184));
  out.fillRect(11, by + 1, (bw - 2) * bat / 100, 5, bat > 20 ? rgb(60, 220, 120) : rgb(255, 70, 70));
  out.drawString("A next B col", cx, py + ph * 92 / 100);
  out.pushSprite(0, 0);
}

static void draw(uint32_t now) {
  const uint8_t* t = THEMES[theme];
  gNow = now;
  cBg = rgb(12, 12, 24);
  cBody = rgb(t[0], t[1], t[2]);
  cNavy = rgb(24, 24, 48);
  cPink = rgb((t[0] + 255) / 2, (t[1] + 90) / 2, (t[2] + 120) / 2);
  gLid = fmaxf(lidBase, blinkLid(now));
  cv.fillSprite(cBg);

  float ox = 0, oy = 0;
  switch (mood) {
    case DIZZY: ox = sinf(now * 0.012f) * 5; oy = sinf(now * 0.02f) * 2; break;
    case EXCITED: oy = -fabsf(sinf(now * 0.012f)) * 6; break;
    case SCARED: ox = sinf(now * 0.09f) * 2.5f; break;
    case SLEEPING: oy = sinf(now * 0.002f) * 1.5f; break;
    default: break;
  }
  uint32_t fxAge = now - fxStart;
  if (fxAge < 500) oy -= sinf(PI * fxAge / 500.0f) * 14;  // pop hop
  X = (int)ox;
  Y = (int)oy;

  cv.fillEllipse(64, 122, 36 + (int)(oy * 0.5f), 4, rgb(30, 30, 50));
  switch (character) {
    case ROBOT: drawRobot(); break;
    case GHOST: drawGhost(); break;
    case CYCLOPS: drawCyclops(); break;
    default: drawCat(); break;
  }

  if (mood == SCARED) {
    uint16_t d = rgb(235, 248, 255);
    cv.fillCircle(106 + X, 34 + Y, 4, d);
    cv.fillTriangle(102 + X, 34 + Y, 110 + X, 34 + Y, 106 + X, 24 + Y, d);
  }
  if (mood == DIZZY) {
    for (int i = 0; i < 3; i++) {
      float a = now * 0.006f + i * 2.09f;
      star(64 + (int)(cosf(a) * 42), 8 + (int)(sinf(a) * 5), rgb(255, 230, 80));
    }
  }
  if (mood == SLEEPING) {
    cv.setTextDatum(textdatum_t::top_left);
    cv.setTextColor(TFT_WHITE);
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(now * 0.0006f + i * 0.33f, 1.0f);
      cv.setTextSize(1.0f + ph);
      cv.drawString("z", 92 + (int)(ph * 20), 50 - (int)(ph * 40));
    }
  }

  // button pop effect: expanding rings + sparkles
  if (fxAge < 500) {
    float p = fxAge / 500.0f;
    uint16_t c = rgb((int)(t[0] * (1 - p) + 12 * p), (int)(t[1] * (1 - p) + 12 * p),
                     (int)(t[2] * (1 - p) + 24 * p));
    int r = (int)(p * 95);
    cv.drawCircle(64, 64, r, c);
    cv.drawCircle(64, 64, r > 4 ? r - 4 : 0, c);
    for (int i = 0; i < 8; i++) {
      float a = i * 0.785f + 0.3f;
      float rr = 12 + p * 62;
      star(64 + (int)(cosf(a) * rr), 64 + (int)(sinf(a) * rr), c);
    }
  }
  present(now);
}

static void startPop(uint32_t now);

// Serial console (115200): ver | char N | theme N | mood N|auto | shot
static void runCommand(char* cmd) {
  int v = 0;
  if (!strcmp(cmd, "ver")) {
    Serial.printf("StickC Motion Face %s\n", FW_VERSION);
  } else if (sscanf(cmd, "char %d", &v) == 1) {
    character = ((v % NUM_CHARS) + NUM_CHARS) % NUM_CHARS;
    prefs.putUChar("char", character);
    startPop(millis());
  } else if (sscanf(cmd, "theme %d", &v) == 1) {
    theme = ((v % NUM_THEMES) + NUM_THEMES) % NUM_THEMES;
    prefs.putUChar("theme", theme);
  } else if (!strcmp(cmd, "mood auto")) {
    forcedMood = -1;
  } else if (sscanf(cmd, "mood %d", &v) == 1) {
    forcedMood = v >= 0 && v <= SLEEPING ? v : -1;
    if (forcedMood == SLEEPING) lidBase = 1.0f;
  } else if (!strcmp(cmd, "shot")) {
    shotRequested = true;
  } else {
    Serial.println("commands: ver | char 0-3 | theme 0-5 | mood 0-4|auto | shot");
  }
}

static void pollSerial() {
  static char buf[32];
  static int n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      buf[n] = 0;
      if (n) runCommand(buf);
      n = 0;
    } else if (n < (int)sizeof(buf) - 1) {
      buf[n++] = c;
    }
  }
}

// Raw RGB565 dump of the last full-screen frame, used by tools/shot.py for the README pictures.
static void sendShot() {
  Serial.printf("SHOT %d %d\n", (int)out.width(), (int)out.height());
  Serial.write((const uint8_t*)out.getBuffer(), out.width() * out.height() * 2);
  Serial.println();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(0);
  if (M5.Display.width() > M5.Display.height()) M5.Display.setRotation(1);  // want portrait
  M5.Display.setBrightness(120);
  cv.setColorDepth(16);
  cv.createSprite(128, 128);
  out.setColorDepth(16);
  out.createSprite(M5.Display.width(), M5.Display.height());
  randomSeed(esp_random());
  prefs.begin("face", false);
  character = prefs.getUChar("char", CAT) % NUM_CHARS;
  theme = prefs.getUChar("theme", 0) % NUM_THEMES;
  Serial.printf("StickC Motion Face %s, board %d, IMU %d, screen %dx%d\n", FW_VERSION, (int)M5.getBoard(), (int)M5.Imu.getType(), (int)M5.Display.width(), (int)M5.Display.height());
  lastActive = millis();
  nameUntil = millis() + 1300;
}

void loop() {
  M5.update();
  uint32_t now = millis();
  // Handle the button before the frame gate: its click flags only live until the next M5.update().
  if (M5.BtnA.wasHold()) {
    theme = (theme + 1) % NUM_THEMES;
    prefs.putUChar("theme", theme);
    startPop(now);
    nameUntil = 0;
  } else if (M5.BtnB.wasClicked()) {
    theme = (theme + 1) % NUM_THEMES;
    prefs.putUChar("theme", theme);
    startPop(now);
    nameUntil = 0;
  } else if (M5.BtnA.wasClicked()) {
    character = (character + 1) % NUM_CHARS;
    prefs.putUChar("char", character);
    startPop(now);
  }
  if (now - lastFrame < 16) return;
  lastFrame = now;

  pollSerial();
  readMotion(now);
  updateEyes(now);
  draw(now);
  if (shotRequested) {
    shotRequested = false;
    sendShot();
  }

  if (now - lastLog > 500) {
    lastLog = now;
    Serial.printf("a=%.2f %.2f %.2f e=%.1f mood=%d char=%d\n", ax, ay, az, energy, mood, character);
  }
}
