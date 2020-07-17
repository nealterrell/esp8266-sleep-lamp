# DIY sleep-training lamp with ESP8266 and voice control

This is a DIY lamp inspired by by the [LittleHippo Mella](https://www.amazon.com/dp/B078Z5FHG9) sleep-training clock. 
It has a night-time "sleep" mode where the light turns yellow at a fixed time in the morning, then green after a duration.
It also has a "nap" mode where the the light turns yellow at X minutes in the future, and then green after a duration. 
We can set the color from a fixed set of known colors, or put the lamp into a "rainbow" mode that randomly transitions
between colors on two different pairs of LEDs, giving a subtle "rainbow" effect.

An ESP8266 microcontroller gives wireless access to the Internet, allowing for remote control of the lamp. We use Google Assistant
with IFTTT for voice control, but many other methods would work as well. This build uses two pairs of 5mm/0.2W RGB LEDs for the lighting;
each pair is wired together, and the lamp's programming lets us set either all four LEDs to the same color, or light each pair
independently of the other. It does not give off a ton of light, but it is enough to illuminate a small child's lamp. The number of LEDs is
configurable in the code, along with most other aspects of the lamp's behavior.

After soldering the ESP8266 and LEDs to a perfboard, I placed the lamp inside this cute little wooden birdhouse that I found at a 
local hobby shop and painted with my toddler:

