// Track LED pace controller
// - listens on Serial (9600) for commands from your Node.js server
// - controls a WS2812 / NeoPixel strip
//
// Commands (examples):
// START:60,0,#001A57|75,100,#6b7280|65,200,#8b5cf6|
// STOP
// PACE:60|75|65
// COLOR:2,#ff0000

#include <Adafruit_NeoPixel.h>

#define LED_PIN       8     // data pin to your LED strip
#define NUM_LEDS      128   // total LEDs on the strip
#define TRACK_METERS  400   // track lap distance (used to convert start line meters -> LED index)

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

struct Pace {
  bool enabled;
  uint16_t splitSeconds; // seconds per lap
  uint16_t startMeters;  // starting line in meters
  uint32_t color;        // packed RGB (0xRRGGBB)
  uint16_t ledIndex;     // computed LED index for startMeters
  unsigned long stepIntervalMs; // ms between LED steps
  unsigned long lastStepTime;
  int position;          // current LED position (0..NUM_LEDS-1)
};

Pace paces[4]; // support up to 4 paces (matches website)
bool running = false;

void setup() {
  Serial.begin(9600); // must match app.js
  while (!Serial) {
    ; // wait for serial port to connect
  }
  
  strip.begin();
  strip.show(); // initialize all to 'off'

  // default paces
  for (int i = 0; i < 4; ++i) {
    paces[i].enabled = false;
    paces[i].splitSeconds = 60;
    paces[i].startMeters = 0;
    paces[i].color = 0x001A57;
    paces[i].ledIndex = 0;
    paces[i].stepIntervalMs = 1000;
    paces[i].lastStepTime = 0;
    paces[i].position = 0;
  }

  Serial.println("Arduino ready");
  Serial.flush();
}

void loop() {
  // Read serial lines
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      Serial.print("CMD recv: ");
      Serial.println(line);
      Serial.flush();
      handleCommand(line);
    }
  }

  // Animate paces if running
  if (running) {
    unsigned long now = millis();
    // clear strip each frame (we'll re-draw all pace lights)
    strip.clear();

    for (int i = 0; i < 4; ++i) {
      if (!paces[i].enabled) continue;

      // time to step?
      if ((unsigned long)(now - paces[i].lastStepTime) >= paces[i].stepIntervalMs) {
        paces[i].position = (paces[i].position + 1) % NUM_LEDS;
        paces[i].lastStepTime = now;
      }

      // write 10 LEDs in a line for this pace
      uint32_t c = paces[i].color;
      for (int j = 0; j < 10; j++) {
        int index = (paces[i].ledIndex + paces[i].position + j) % NUM_LEDS;
        strip.setPixelColor(index, c);
      }
    }

    strip.show();
  } else {
    // if not running, keep strip off
    // (you could show idle pattern here)
  }
}

// ---------- command parsing ----------

void handleCommand(const String &cmd) {
  if (cmd.startsWith("START:")) {
    String payload = cmd.substring(6);
    parseStartPayload(payload);
    running = true;
    // reset lastStepTime for smooth start
    unsigned long now = millis();
    for (int i = 0; i < 4; ++i) paces[i].lastStepTime = now;
    Serial.println("Pacer started");
    Serial.flush();
  } else if (cmd == "STOP") {
    running = false;
    strip.clear();
    strip.show();
    Serial.println("Pacer stopped");
    Serial.flush();
  } else if (cmd.startsWith("PACE:")) {
    // PACE:split|split|...
    String payload = cmd.substring(5);
    parsePacePayload(payload);
    Serial.println("Pace updated");
    Serial.flush();
  } else if (cmd.startsWith("COLOR:")) {
    // COLOR:paceIndex,#RRGGBB
    String payload = cmd.substring(6);
    parseColorPayload(payload);
    Serial.println("Color updated");
    Serial.flush();
  } else {
    Serial.print("Unknown cmd: ");
    Serial.println(cmd);
    Serial.flush();
  }
}

void parseStartPayload(String payload) {
  // format: split,start,color|split,start,color|...
  // clear all paces first
  for (int i = 0; i < 4; ++i) {
    paces[i].enabled = false;
  }

  int idx = 0;
  int start = 0;
  while (start < payload.length() && idx < 4) {
    int bar = payload.indexOf('|', start);
    String chunk;
    if (bar == -1) {
      chunk = payload.substring(start);
      start = payload.length();
    } else {
      chunk = payload.substring(start, bar);
      start = bar + 1;
    }
    chunk.trim();
    if (chunk.length() == 0) continue;

    // chunk is split,start,color
    int p1 = chunk.indexOf(',');
    int p2 = chunk.indexOf(',', p1 + 1);
    if (p1 == -1 || p2 == -1) continue;

    String sSplit = chunk.substring(0, p1);
    String sStart = chunk.substring(p1 + 1, p2);
    String sColor = chunk.substring(p2 + 1);

    uint16_t splitSec = (uint16_t) sSplit.toInt();
    uint16_t startMeters = (uint16_t) sStart.toInt();
    uint32_t color = parseHexColor(sColor);

    paces[idx].enabled = true;
    paces[idx].splitSeconds = splitSec;
    paces[idx].startMeters = startMeters;
    paces[idx].color = color;
    paces[idx].ledIndex = metersToLed(startMeters);
    paces[idx].position = 0;
    // compute interval: splitSeconds is seconds per lap; each lap = NUM_LEDS steps
    unsigned long interval = 1;
    if (splitSec > 0) {
      interval = (unsigned long)splitSec * 1000UL / (unsigned long)NUM_LEDS;
      if (interval == 0) interval = 1;
    }
    paces[idx].stepIntervalMs = interval;
    paces[idx].lastStepTime = millis();

    Serial.print("Configured pace ");
    Serial.print(idx+1);
    Serial.print(": split=");
    Serial.print(splitSec);
    Serial.print("s, start=");
    Serial.print(startMeters);
    Serial.print("m -> LED ");
    Serial.print(paces[idx].ledIndex);
    Serial.print(", color=0x");
    Serial.println(colorToHex(paces[idx].color));
    Serial.flush();

    idx++;
  }
}

void parsePacePayload(String payload) {
  // payload: split1|split2|...
  int start = 0;
  int i = 0;
  while (start < payload.length() && i < 4) {
    int bar = payload.indexOf('|', start);
    String token;
    if (bar == -1) {
      token = payload.substring(start);
      start = payload.length();
    } else {
      token = payload.substring(start, bar);
      start = bar + 1;
    }
    token.trim();
    if (token.length() == 0) { i++; continue; }

    uint16_t splitSec = (uint16_t) token.toInt();
    if (paces[i].enabled) {
      paces[i].splitSeconds = splitSec;
      paces[i].stepIntervalMs = (splitSec > 0) ? (unsigned long)splitSec * 1000UL / (unsigned long)NUM_LEDS : 1;
      Serial.print("Pace ");
      Serial.print(i+1);
      Serial.print(" new split ");
      Serial.println(splitSec);
      Serial.flush();
    }
    i++;
  }
}

void parseColorPayload(String payload) {
  // COLOR:paceIndex,#RRGGBB  (paceIndex is 1-based)
  payload.trim();
  int comma = payload.indexOf(',');
  if (comma == -1) return;
  int paceIndex = payload.substring(0, comma).toInt() - 1;
  String sColor = payload.substring(comma + 1);
  sColor.trim();
  if (paceIndex < 0 || paceIndex >= 4) return;
  uint32_t color = parseHexColor(sColor);
  paces[paceIndex].color = color;
  Serial.print("Pace ");
  Serial.print(paceIndex+1);
  Serial.print(" color -> 0x");
  Serial.println(colorToHex(color));
  Serial.flush();
}

// map meters (0..TRACK_METERS) to LED index (0..NUM_LEDS-1)
int metersToLed(uint16_t meters) {
  if (meters >= TRACK_METERS) meters = TRACK_METERS - 1;
  float ratio = (float)meters / (float)TRACK_METERS;
  int index = (int)round(ratio * (NUM_LEDS - 1));
  return constrain(index, 0, NUM_LEDS - 1);
}

// parse "#RRGGBB" or "RRGGBB" into 0xRRGGBB
uint32_t parseHexColor(const String &s) {
  String t = s;
  t.trim();
  if (t.startsWith("#")) t = t.substring(1);
  if (t.length() != 6) return 0x001A57; // default
  long val = (long) strtol(t.c_str(), NULL, 16);
  return (uint32_t)val;
}

// helper to print hex as string
String colorToHex(uint32_t c) {
  char buf[8];
  sprintf(buf, "%06lX", (unsigned long)c);
  return String(buf);
}