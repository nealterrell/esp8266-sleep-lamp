#define DEBUG

#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "LampConfig.h"
#include "WifiConfig.h"

#include <TimeLib.h>
#include "AdafruitIO_WiFi.h"

/******************* Types *******************/ 
enum LightMode {
  BOOTING,
  CONNECTED,
  DISABLED,
  NORMAL,
  SLEEPING, 
  WAKING,    // "transition" time from sleeping to awake.
  AWAKE,
  RAINBOW
};

// A named color code, for voice control.
struct ColorCode {
  const char *colorName;
  int r;
  int g;
  int b;
};



/******************* Pins and constants *******************/ 
constexpr size_t NUM_LIGHTS = 2;
constexpr uint8_t RED_PINS[NUM_LIGHTS]   = {13, 2};
constexpr uint8_t GREEN_PINS[NUM_LIGHTS] = {12, 0};
constexpr uint8_t BLUE_PINS[NUM_LIGHTS]  = {14, 4};

constexpr uint32_t RAINBOW_STEPS = 100;
constexpr uint32_t RAINBOW_DURATION = 5000;


/******************* Wifi and NTP *******************/ 
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AdafruitIO_Feed *nap;

// We make this variable a pointer and dynamically allocate it in setup() 
// so we can set the DHCP hostname of this device before initializing the wifi.
// This order was required in MicroPython; I did not verify if it was also required here.
AdafruitIO_WiFi *io;


/******************* Colors *******************/ 

#define NIGHT_LIGHT 0,0,0
#define WAKING_LIGHT 200,200,0
#define AWAKE_LIGHT 0,255,0
#define NO_LIGHT 0,0,0

constexpr int NUM_COLORS = 15;
ColorCode knownColors[NUM_COLORS] = {
  {"red", 255, 0, 0},
  {"orange", 255, 165, 0},
  {"yellow", 255, 255, 0},
  {"green", 0, 255, 0},
  {"blue", 0, 0, 255},
  {"purple", 102, 51, 153},
  {"white", 255, 255, 255},
  {"brown", 165, 42, 42},
  {"pink", 255, 105, 180},
  {"teal", 0, 128, 128},
  {"cyan", 0, 255, 255},
  {"magenta", 255, 0, 255},
  {"crimson", 153, 0, 0},
  {"lime", 50, 205, 50},
  {"midnight", 0, 51, 102}
};

size_t rainbowIndex[NUM_LIGHTS];


/******************* Operation state *******************/ 
LightMode currentMode = BOOTING;
time_t wakeTime; // time to transition to WAKING
unsigned long wakeDuration;  // length of transition from WAKING to AWAKE, in minutes.



void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  // Initialize pins to output mode.
  for (size_t i = 0; i < NUM_LIGHTS; i++) {
    pinMode(RED_PINS[i], OUTPUT);
    pinMode(GREEN_PINS[i], OUTPUT);
    pinMode(BLUE_PINS[i], OUTPUT);

    // Initialize the rainbow indexes to 0, 1, 2, ...
    rainbowIndex[i] = i;
  }

  // Set color to dull red while connecting to wifi.
  setColorAll(10, 0, 0);


  // Initialize wifi and AdafruitIO connections.
  WiFi.hostname("Ada Lamp");
  io = new AdafruitIO_WiFi(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
  nap = io->feed(IO_FEED);
  while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print("*");
  }
  
  Serial.println("");
  Serial.println("WiFi connection Successful");
  Serial.print("IP Address:");
  Serial.println(WiFi.localIP());// Print the IP address

  io->connect();
  nap->onMessage(handleMessage);
  Serial.printf("Connected to AdafruitIO feed %s\n", IO_FEED);

  
  currentMode = CONNECTED;

  // Set color to yellow while getting time updates.
  setColorAll(10, 10, 0);
  timeClient.begin();
  timeClient.setTimeOffset(TIME_OFFSET);
  setSyncProvider(getNtpTime);
  while (timeStatus() == timeNotSet) {} // wait until time is set.

  Serial.printf("Got current time: %d:%d\n", hour(), minute());
  delay(2000);

  // Set color to dull green for success.
  setColorAll(0, 10, 0);
  delay(1000);
  setInitialState();
}

void setInitialState() {
  currentMode = NORMAL;


#ifdef DEBUG
  Serial.printf("hour: %d, min: %d\n", timeClient.getHours(), timeClient.getMinutes());
#endif
  
  if (timeClient.getHours() >= 20 || timeClient.getHours() <= NIGHT_WAKE_HOUR) {
    startNightTime();
  }
  else {
    currentMode = RAINBOW;
    setColor(0, knownColors[0].r, knownColors[0].g, knownColors[0].b);
    setColor(1, knownColors[1].r, knownColors[1].g, knownColors[1].b);
  }
}

void loop() {
  
  // Watch for IO updates to change lamp state.
  io->run();

  // See if we should change state based on the nap/sleep timers.
  timeClient.update();
  time_t timeDiff = (now() - wakeTime) / SECS_PER_MIN; // number of minutes between now and the waking time.
                                                       // will be negative if we have passed the waking time.

  // If in SLEEPING, transition to WAKING if we have reached the waking time.
  if (currentMode == SLEEPING && timeDiff <= 0) {
    Serial.println("WAKING");
    currentMode = WAKING;
    setColorAll(WAKING_LIGHT);
  }
  // If WAKING, transition to AWAKE if we have passed the waking time by wakeDuration minutes.
  else if (currentMode == WAKING && timeDiff <= -wakeDuration) {
    Serial.println("AWAKE");
    currentMode = AWAKE;
    setColorAll(AWAKE_LIGHT);
  }
  // If AWAKE, transition to NORMAL after wakeDuration has again passed.
  else if (currentMode == AWAKE && timeDiff <= - 2 * wakeDuration) {
    Serial.println("NORMAL");
    currentMode = NORMAL;
  }
  else if (currentMode == RAINBOW) {
    size_t newColors[NUM_LIGHTS] = {0};
    double rColors[NUM_LIGHTS] = {0}, gColors[NUM_LIGHTS] = {0}, bColors[NUM_LIGHTS] = {0};
    
    for (size_t i = 0; i < NUM_LIGHTS; i++) {
      newColors[i] = rainbowIndex[i];
      
      while (newColors[i] == rainbowIndex[i]) {
        newColors[i] = random(NUM_COLORS);
      }
      rColors[i] = knownColors[rainbowIndex[i]].r;  
      gColors[i] = knownColors[rainbowIndex[i]].g;  
      bColors[i] = knownColors[rainbowIndex[i]].b;  

#ifdef DEBUG
      Serial.printf("Changing light %d from %d,%d,%d to %d,%d,%d\n", i, 
        knownColors[rainbowIndex[i]].r, knownColors[rainbowIndex[i]].g, knownColors[rainbowIndex[i]].b,
        knownColors[newColors[i]].r, knownColors[newColors[i]].g, knownColors[newColors[i]].b);
#endif
    }

    // Use a linear interpolation to fade from one color to the next over a number of steps.
    for (size_t s = 0; s < RAINBOW_STEPS; s++) {

      // Each light has a new color to fade to; each step moves the color a linear amount towards the target.
      for (size_t i = 0; i < NUM_LIGHTS; i++) {
        size_t newIndex = newColors[i];
        size_t oldIndex = rainbowIndex[i];
        
        rColors[i] += (double)(knownColors[newIndex].r - knownColors[oldIndex].r) / RAINBOW_STEPS;
        gColors[i] += (double)(knownColors[newIndex].g - knownColors[oldIndex].g) / RAINBOW_STEPS;
        bColors[i] += (double)(knownColors[newIndex].b - knownColors[oldIndex].b) / RAINBOW_STEPS;
        setColor(i, rColors[i], gColors[i], bColors[i]);
      }
      delay(RAINBOW_DURATION / RAINBOW_STEPS);
    }

    // Save the current colors for the next cycle.
    for (size_t i = 0; i < NUM_LIGHTS; i++) {
      rainbowIndex[i] = newColors[i];
    }
  }
  
  delay(1000);
}

uint16_t scale(uint8_t c) {
  // The ESP8266 uses 10-bit PWM resolution, so values go from 0 to 1023. 
  // Need to scale our 8-bit color values to 10 bits.
#ifdef COMMON_ANODE_LED
  return map(c, 0, 255, 1023, 0);
#else
  return map(c, 0, 255, 0, 1023);
#endif
}


// Sets the selected light number to the given color.
void setColor(size_t lightNumber, uint8_t red, uint8_t green, uint8_t blue) {
  uint16_t r = scale(red);
  uint16_t g = scale(green);
  uint16_t b = scale(blue);

  lightNumber = constrain(lightNumber, 0, NUM_LIGHTS);
  analogWrite(RED_PINS[lightNumber], r);
  analogWrite(GREEN_PINS[lightNumber], g);
  analogWrite(BLUE_PINS[lightNumber], b);
}


// Sets the given color on all lights.
void setColorAll(int red, int green, int blue) {
  for (size_t i = 0; i < NUM_LIGHTS; i++) {
    setColor(i, red, green, blue);
  }
}

void startNightTime() {
  time_t midnight = hour() > 12 ? nextMidnight(now()) : previousMidnight(now());

  // These xxToTime_t macros are BROKEN in the official library. See https://github.com/PaulStoffregen/Time/pull/145/files for a fix.
  // (Need to change your local copy.)
  wakeTime = midnight + hoursToTime_t(NIGHT_WAKE_HOUR) + minutesToTime_t(NIGHT_WAKE_MINUTE);
  wakeDuration = NIGHT_WAKE_DURATION;
  
  setColorAll(NIGHT_LIGHT);
  currentMode = SLEEPING;
}

void startNapTime(int minutes) {
  // See comment in startNightTime().
  wakeTime = now() + minutesToTime_t(minutes);
  wakeDuration = NAP_WAKE_DURATION;

#ifdef DEBUG
  Serial.printf("Nap until %d:%d\n", hour(wakeTime), minute(wakeTime));
#endif
  
  setColorAll(NIGHT_LIGHT);
  currentMode = SLEEPING;
}

void startNormalMode(int r, int g, int b) {
  setColorAll(r, g, b);
  currentMode = NORMAL;
}

void handleMessage(AdafruitIO_Data *data) {
  int param = -1;
  String received = data->toString();
  received.toLowerCase(); // edits the string in-place

  // Decode the message.
  // "sleep"
  if (received.indexOf("sleep") >= 0) {
    startNightTime();
  }
  // "nap XX"
  else if ((param = received.indexOf("nap")) >= 0) {
    String timeStr = received.substring(param + 4); // skip past the letters "nap " to the nap length
    int minutes = timeStr.toInt();
    if (minutes > 0) {
      startNapTime(minutes);
    }
    else {
      startNormalMode(WAKING_LIGHT);
    }
  }
  // "off"
  else if (received.endsWith("off")) {
    startNormalMode(NO_LIGHT);
  }
  // "rainbow"
  else if (received.endsWith("rainbow")) {
    currentMode = RAINBOW;                        
  }
  // some other word; see if a known color matches.
  else {
    for (int i = 0; i < NUM_COLORS; i++) {
      if (received.endsWith(knownColors[i].colorName)) {
        startNormalMode(knownColors[i].r, knownColors[i].g, knownColors[i].b);
        break;
      }
    }
  }
}

time_t getNtpTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}
