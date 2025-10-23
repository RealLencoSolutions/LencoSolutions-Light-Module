# LencoSolutions light module info
I started with the original ballance buddy project and added stuff like a brake function and knight rider animation.
The code was also revised to make it non blocking and fast.
Have fun, maybe contribute and check out my site for the hardware: LencoSolutions.nl



# Balance Buddy
Arduino based accessory board for the balance app in BV's BLDC firmware.

## Features
1. Directional LEDs for headlight/taillight with dimming, brake lights and knight rider animation
1. Buzzer for dutycycle and low voltage warnings (maybe in the feature also temp. warnings)
1. It uses CAN bus, so there are NO DOWNSIDES to adding it. JUST DO IT!

## Parts List
1. Adruino Nano
1. MCP2515 Can Bus Module Board
1. 5v piezo active buzzer
1. 2x LED Strips (WS2812 GRB is the default, but anything supported by FastLED should work)
1. SSD1306 128x64 Display (4 pin, any color, the blue/yellow works nice too)

# Wiring
## Can Module: 
The new version of this hardware has all the pin outs written on the top of the PCB
All the different part are now integrated and also include a port to connect a voltage meter and pull down resisters for the VESC ads's for your footsensors.

# Configuration
## Options and pins
Features are designed to be configured VIA the constants
1. esc.cpp: Configure CAN bus IDs, you must match the ID set in the VESC Tool
1. balance_beeper.cpp: Configure wiring and alerts
1. lennart-ballanceleds-0.10.0.ino: Main loop there yyou set nr of leds and stuff like color


## Compiling/Installing
All the required libraries are included, just hit the upload button in Arduino IDE

# Future plans
1. VESC control over the settings like the color of the lights through can bus
1. A battery indication over a LED bar/front LED in rest state