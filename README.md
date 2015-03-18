# screenduo4linux
Automatically exported from code.google.com/p/screenduo4linux

This project is an attempt to make a driver capable of at-the very minimum, drive the LCD in the ASUS ScreenDUO.

Reverse engineering is being accomplished by USB communications capture, no disassembly of the ASUS application has taken place keeping this code safe from legal implications.

NEWS: 2nd of June 2010 - We can now drive the LCD

This device acts as a mass storage device in that the data packets must be sent inside CBW (Command Block Wrapper) packets with some data about the current position of the source, total length, and some flags still yet to be determined.

Since the device is an ARM processor inside, the CB structure fields need swapping to big-endian.

I would appreciate any help on getting the buttons to work now, and learning how to push programs/scripts into it as it seems it has some processing capability as it can run without a host once it has been programmed on power up.

My ultimate goal is to turn this little screen into a XBMC controller so I don't have to turn on my rear projection screen just to play some music.

Device Features

* 320x240 TFT RGB LCD Display
* Directional Button Pad
* Enter and Return buttons
* a "1" and "2" button along the top
* ARM powered, seems to be some ASUSTek chip of sorts.
* Can run without a host once programmed (this would be very cool to figure out)
* ~4fps refresh, it may be possible to improve upon this
