// The hour and minute that the light switches to WAKING when "sleep" is triggered.
constexpr uint8_t NIGHT_WAKE_HOUR = 6;
constexpr uint8_t NIGHT_WAKE_MINUTE = 0;
// The duration that the light stays in WAKING mode, in minutes.
constexpr uint8_t NIGHT_WAKE_DURATION = 60;

// The duration of the "nap" mode WAKING period, in minutes.
constexpr uint8_t NAP_WAKE_DURATION = 15;


// True if the light should automatically set mode to SLEEPING and a wake time of to NIGHT_WAKE_XX
//    at the trigger time below.
constexpr bool AUTO_SLEEP = true;
// The hour and minute at which the light automatically goes to "sleep", if AUTO_SLEEP is true.
constexpr uint8_t AUTO_SLEEP_HOUR = 20;
constexpr uint8_t AUTO_SLEEP_MINUTE = 0;


// Remove the next line if you are using common cathode LEDs.
#define COMMON_ANODE_LED
