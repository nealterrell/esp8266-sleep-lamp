
// A named color code, for voice control.
struct ColorCode {
  const char *colorName;
  int r;
  int g;
  int b;
};

/******************* Colors *******************/ 

#define NIGHT_LIGHT 0,0,0
#define WAKING_LIGHT 200,200,0
#define AWAKE_LIGHT 0,255,0
#define NO_LIGHT 0,0,0

constexpr int NUM_COLORS = 15;
constexpr ColorCode KNOWN_COLORS[NUM_COLORS] = {
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
