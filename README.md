# XPT2046 Bitbang Arduino Library

XPT2046_Bitbang is a library for the XPT2046 resistive touchscreen controllers used on many low cost TFT displays. It is based on [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) but uses software-based SPI instead with some additional enhancements.

![ILI9431Test Example Program](doc/ILI9431Test.jpg)

## Setup Functions

First, create an instance of the library for your touchscreen.

```cpp
#define TS_SCK   25
#define TS_MOSI  32
#define TS_MISO  39
#define TS_CS    33

XPT2046_Bitbang ts(TS_SCK, TS_MISO, TS_MOSI, TS_CS);
```

The use of the Touch interrupt pin can be optionally specified. If the Teensy pin specified is actively connected to the T_IRQ display pin then the normal touch calls will respond, but can be called more often as each call returns without hardware access when no interrupt was recorded.

```cpp
#define TS_IRQ   36

XPT2046_Bitbang ts(TS_SCK, TS_MISO, TS_MOSI, TS_CS, TS_IRQ);
```

In `setup()`, use the `begin()` function to initialize the touchscreen. You can also use `setDimensions`, `setCalibration`, and optionally `setRotation` to get the appropriate values from the `getPoint()` function.

```cpp
ts.begin();
ts.setRotation(0);
ts.setDimensions(320, 240);
ts.setCalibration(240, 3800, 200, 3700);
```

## Reading Touch Info

The `touched()` function tells if the display is currently being touched,
returning true or false.

```cpp
if (ts.touched()) {
  // do something....
}
```

You can get the touch coordinates with `getPoint()`, which returns a `TS_Point` object:

```cpp
TS_Point p = ts.getPoint();
Serial.print("x = ");
Serial.print(p.x);
Serial.print(", y = ");
Serial.print(p.y);
```

The Z coordinate represents the amount of pressure applied to the screen.

## Using The Interrupt Pin : Built in support when connected nothing else is needed. When specified as above

no SPI calls are made unless a Touch was detected. On normal connections - this means the Teensy LED
won't blink on every touch query.
