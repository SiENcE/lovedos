# LoveDOS
A framework for making 2D DOS games in Lua. LoveDOS provides an API based on a
subset of the [LÖVE](https://love2d.org/) API.

![screenshot](https://cloud.githubusercontent.com/assets/3920290/21948750/ed49e9a6-d9e4-11e6-960a-1fac0ec41ee0.gif)

## Getting started
You can download LoveDOS from the
[releases page](https://github.com/rxi/lovedos/releases). If you're not
using DOS as your operating system then [DOSbox](http://www.dosbox.com/) can be
used to emulate a DOS computer.

When you run `love.exe` it will expect its first argument to be your game's
directory. The file `main.lua` will then be searched for and executed. If, for
example, your project was in a directory named `mygame` you would run the
following:
```batch
love mygame
```

A small example program which displays white text on a black background and
exits when the `escape` key is pressed is as follows:

```lua
function love.draw()
  love.graphics.print('Hello World!', 20, 20)
end

function love.keypressed(key)
  if key == "escape" then
    love.event.quit()
  end
end
```

The [doc/api.md](doc/api.md) file provides a reference and overview of all of
the built-in LoveDOS modules, functions and callbacks.

The [doc/packaging.md](doc/packaging.md) file provides instructions for
packaging your game for distribution.


## System requirements
* A 386 or better. A math coprocessor (387) is strongly recommended: Lua uses
  floating point for all numbers, which is very slow when emulated
* VGA
* A Sound Blaster 16 (or compatible) for sound; LoveDOS runs without one
* Enough RAM for your game's assets: all memory is locked as the audio
  interrupt handler accesses it, so DPMI virtual memory (swapping) is not used


## Performance
The [bench](bench/main.lua) directory contains a benchmark which can be run
with `love bench`. Measured in DOSBox (`cycles=fixed 5500` with `cputype=386`
approximating a 386DX-30, `cycles=fixed 26000` with `cputype=486_slow`
approximating a 486DX2-66) it gives the following; real hardware with a slow
ISA VGA card will take longer for `present`, and a real 386 takes longer for
multiplications and floating point maths than DOSBox assumes.

Operation                               | 386DX-30 | 486DX2-66
----------------------------------------|----------|----------
Audio mixing, 1 source (22kHz)          | 17% CPU  | 3.6% CPU
Audio mixing, 4 sources (22kHz)         | 41% CPU  | 8.7% CPU
Audio mixing, 4 sources (44kHz or pitched) | 85-94% CPU | 18-20% CPU
`love.graphics.clear()`                 | 3.0ms    | 0.63ms
`love.graphics.present()`               | 3.0ms    | 0.63ms
`draw()` 16x16 image                    | 0.41ms   | 0.09ms
`draw()` 16x16 image, scaled x2         | 2.9ms    | 0.61ms
`draw()` 16x16 image, rotated           | 2.4ms    | 0.50ms
`draw()` 100 16x16 images, one by one   | 55ms     | 12ms
SpriteBatch of 100 16x16 sprites        | 27ms     | 5.7ms
`print()` 43 characters                 | 4.1ms    | 0.87ms
Text object, 43 characters              | 1.5ms    | 0.32ms
`printf()` 176 characters, wrapped      | 17ms     | 3.6ms
`Source:setPosition()`                  | 0.22ms   | 0.05ms

Some tips for keeping games fast on slow machines:
* Use 22050Hz (or lower) sound files and avoid `Source:setPitch()` where
  possible: sources with other sample rates or a pitch need interpolation,
  which roughly doubles their mixing cost. When no source is playing mixing
  costs almost nothing (about 1% on a 386).
* Positional audio is calculated when a source or the listener changes, not
  while mixing, so positioned sources cost no more to play than others.
* Rotated and scaled drawing maps every pixel individually and is 6-7 times
  slower than normal drawing; mirroring with a scale of `-1` is fast.
* Text is expensive: use `love.graphics.newText()` for text which doesn't
  change every frame; drawing a Text object is 2.5 times faster than printing.
* Draw tile maps and many sprites of the same image with a SpriteBatch, which
  halves the time compared to separate `draw()` calls on a 386.
* Use 22050Hz audio and `"stream"` sources for long music: files of 256KB or
  more are streamed from disk instead of being loaded into memory.
* Draw many pixels with a single `love.graphics.points()` call rather than
  many `love.graphics.point()` calls.
* `love.graphics.setScissor()` makes clipped drawing cheaper, not more
  expensive.


## Building
Instructions for building the project from source can be found in the
[doc/building.md](doc/building.md) file.


## License
This library is free software; you can redistribute it and/or modify it under
the terms of the MIT license. See [LICENSE](LICENSE) for details.

LoveDOS includes Lua (MIT license). The full license for Lua can be found at
the bottom of the [src/lib/lua/lua.h](src/lib/lua/lua.h) file.
