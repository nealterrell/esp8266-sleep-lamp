#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "LampConfig.h"
#include "WifiConfig.h"
#include "ColorConfig.h"

#include <TimeLib.h>
#include "AdafruitIO_WiFi.h"

/******************* Types *******************/ 
enum LightMode {
  OFF,
  BOOTING,
  CONNECTED,
  NORMAL,
  SLEEPING, 
  WAKING,    // "transition" time from sleeping to awake.
  AWAKE,
  RAINBOW
};

/******************* Wifi and NTP *******************/ 
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AdafruitIO_Feed *nap;

// We make this variable a pointer and dynamically allocate it in setup() 
// so we can set the DHCP hostname of this device before initializing the wifi.
// This order was required in MicroPython; I did not verify if it was also required here.
AdafruitIO_WiFi *io;


/******************* Operation state *******************/ 
LightMode currentMode = BOOTING;
time_t wakeTime; // time to transition to WAKING
unsigned long wakeDuration;  // length of transition from WAKING to AWAKE, in minutes.
size_t rainbowIndex[NUM_LIGHTS]; // indexes into KNOWN_COLORS for each light.


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
  WiFi.hostname("Sleep Training Lamp");
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
  timeClient.update();
  setSyncProvider(getNtpTime);
  while (timeStatus() == timeNotSet) {} // wait until time is set.

  Serial.printf("Got current time: %d:%02d\n", hour(), minute());
  delay(2000);

  // Set color to dull green for success.
  setColorAll(0, 10, 0);
  delay(1000);
  setInitialState();
}

void setInitialState() {
  currentMode = RAINBOW;
  setColor(0, KNOWN_COLORS[0].r, KNOWN_COLORS[0].g, KNOWN_COLORS[0].b);
  setColor(1, KNOWN_COLORS[1].r, KNOWN_COLORS[1].g, KNOWN_COLORS[1].b);
}

void loop() {
  
  // Watch for IO updates to change lamp state.
  io->run();

  // See if we should change state based on the nap/sleep timers.
  timeClient.update();
  double timeDiff = (now() - wakeTime) / (double)SECS_PER_MIN; // number of minutes between now and the waking time.
                                                        
#ifdef DEBUG
  Serial.printf("Time until wakeTime: %f minutes\n", timeDiff);
#endif

  // If in SLEEPING, transition to WAKING if we have reached the waking time.
  if (currentMode == SLEEPING && timeDiff >= 0) {
    Serial.println("WAKING");
    currentMode = WAKING;
    setColorAll(WAKING_LIGHT);
  }
  // If WAKING, transition to AWAKE if we have passed the waking time by wakeDuration minutes.
  else if (currentMode == WAKING && timeDiff >= wakeDuration) {
    Serial.println("AWAKE");
    currentMode = AWAKE;
    setColorAll(AWAKE_LIGHT);
  }
  // If AWAKE, transition to NORMAL after wakeDuration has again passed.
  else if (currentMode == AWAKE && timeDiff >=  2 * wakeDuration) {
    Serial.println("NORMAL");
    currentMode = NORMAL;
  }
  else if (currentMode == OFF && AUTO_SLEEP && hour() == AUTO_SLEEP_HOUR && minute() == AUTO_SLEEP_MINUTE) {
    startNightTime();
  }
  else if (currentMode == RAINBOW) {
    size_t newColors[NUM_LIGHTS] = {0};
    double rColors[NUM_LIGHTS] = {0}, gColors[NUM_LIGHTS] = {0}, bColors[NUM_LIGHTS] = {0};
    
    for (size_t i = 0; i < NUM_LIGHTS; i++) {
      newColors[i] = rainbowIndex[i];
      
      while (newColors[i] == rainbowIndex[i]) {
        newColors[i] = random(NUM_COLORS);
      }
      rColors[i] = KNOWN_COLORS[rainbowIndex[i]].r;  
      gColors[i] = KNOWN_COLORS[rainbowIndex[i]].g;  
      bColors[i] = KNOWN_COLORS[rainbowIndex[i]].b;  

#ifdef DEBUG
      Serial.printf("Changing light %d from %d,%d,%d to %d,%d,%d\n", i, 
        KNOWN_COLORS[rainbowIndex[i]].r, KNOWN_COLORS[rainbowIndex[i]].g, KNOWN_COLORS[rainbowIndex[i]].b,
        KNOWN_COLORS[newColors[i]].r, KNOWN_COLORS[newColors[i]].g, KNOWN_COLORS[newColors[i]].b);
#endif
    }

    // Use a linear interpolation to fade from one color to the next over a number of steps.
    for (size_t s = 0; s < RAINBOW_STEPS; s++) {

      // Each light has a new color to fade to; each step moves the color a linear amount towards the target.
      for (size_t i = 0; i < NUM_LIGHTS; i++) {
        size_t newIndex = newColors[i];
        size_t oldIndex = rainbowIndex[i];
        
        rColors[i] += (double)(KNOWN_COLORS[newIndex].r - KNOWN_COLORS[oldIndex].r) / RAINBOW_STEPS;
        gColors[i] += (double)(KNOWN_COLORS[newIndex].g - KNOWN_COLORS[oldIndex].g) / RAINBOW_STEPS;
        bColors[i] += (double)(KNOWN_COLORS[newIndex].b - KNOWN_COLORS[oldIndex].b) / RAINBOW_STEPS;
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

#ifdef DEBUG
  Serial.printf("Now: %d:%02d on day %d\n", hour(), minute(), day());  
  Serial.printf("Wake: %d:%02d on day %d\n", hour(wakeTime), minute(wakeTime), day(wakeTime));
#endif
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
    currentMode = OFF;
    setColorAll(NO_LIGHT);
  }
  // "rainbow"
  else if (received.endsWith("rainbow")) {
    currentMode = RAINBOW;                        
  }
  // some other word; see if a known color matches.
  else {
    for (int i = 0; i < NUM_COLORS; i++) {
      if (received.endsWith(KNOWN_COLORS[i].colorName)) {
        startNormalMode(KNOWN_COLORS[i].r, KNOWN_COLORS[i].g, KNOWN_COLORS[i].b);
        break;
      }
    }
  }
}

time_t getNtpTime() {
#ifdef DEBUG
  Serial.println("Waiting for time client");
#endif
  timeClient.update();
  return timeClient.getEpochTime();
}
