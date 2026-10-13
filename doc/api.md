# LoveDOS API

##### [Modules](#modules-1)
* [love](#love)
* [love.system](#lovesystem)
* [love.graphics](#lovegraphics)
* [love.timer](#lovetimer)
* [love.keyboard](#lovekeyboard)
* [love.mouse](#lovemouse)
* [love.filesystem](#lovefilesystem)
* [love.audio](#loveaudio)
* [love.sound](#lovesound)
* [love.math](#lovemath)
* [love.joystick](#lovejoystick)
* [love.event](#loveevent)

##### [Objects](#objects-1)
* [Image](#image)
* [Quad](#quad)
* [Font](#font)
* [SpriteBatch](#spritebatch)
* [Text](#text)
* [Joystick](#joystick)
* [File](#file)
* [FileData](#filedata)
* [RandomGenerator](#randomgenerator)
* [BezierCurve](#beziercurve)
* [Transform](#transform)
* [Source](#source)
* [SoundData](#sounddata)
* [Decoder](#decoder)

##### [Callbacks](#callbacks-1)

##### [Configuration (conf.lua)](#configuration-conflua)


## Modules

### love

##### love.getVersion()
Returns the version of LoveDOS as a string.

##### love.isVersionCompatible(version)
##### love.isVersionCompatible(major, minor, revision)
Returns `true` if a game written for the given version of LÖVE can be run.
LoveDOS implements parts of the LÖVE 0.10 and 11 APIs, so this is `true` for
`0.10.x` and `11.x`.

##### love.hasDeprecationOutput()
##### love.setDeprecationOutput(enable)
Gets and sets whether deprecation warnings are output. LoveDOS doesn't output
any; these exist for compatibility.

### love.system
Provides access to information about the user's system.

##### love.system.getOS()
Returns the operating system which LoveDOS is running on.

##### love.system.getMemUsage()
Returns the amount of memory in kilobytes which is being used by LoveDOS. This
includes the memory used by both the loaded assets and lua.


### love.graphics
Provides functions for drawing lines, shapes, text and images.

##### love.graphics.getDimensions()
Returns the width and height of the screen in pixels as two numbers.

##### love.graphics.getWidth()
Returns the width of the screen in pixels.

##### love.graphics.getHeight()
Returns the height of the screen in pixels.

##### love.graphics.getBackgroundColor()
Returns the currently set background color.

##### love.graphics.setBackgroundColor(red, green, blue)
Sets the the background color used when `love.graphics.clear()` is called
without any arguments. If called with no arguments the back color is set to
black.

##### love.graphics.getColor()
Returns the currently set color.

##### love.graphics.setColor(red, green, blue)
Sets the the color used when drawing. If called with no arguments the color is
set to white.

##### love.graphics.getBlendMode()
Returns the currently set blend mode.

##### love.graphics.setBlendMode([mode])
Sets the current blend mode used by `love.graphics.draw()`. If no `mode`
argument is passed then the blend mode is set to the default (`"normal"`).

Mode        | Description
------------|------------------------------------------------------------------
`"normal"`  | Draws normally with transparency
`"fast"`    | Draws without transparency, this is the fastest blend mode
`"and"`     | Binary ANDs the source and destination pixels
`"or"`      | Binary ORs the source and destination pixels
`"color"`   | Draws opaque pixels using the `love.graphics.setColor()` color

##### love.graphics.getFont()
Returns the current font.

##### love.graphics.setFont([font])
Sets the current font. If `font` is nil then the font is reset to the default.

##### love.graphics.getCanvas()
Returns the current canvas.

##### love.graphics.setCanvas([image])
Sets the current canvas which all the draw operations will draw to. If the
`image` argument is not set then the canvas is reset to the default canvas
representing the user's screen.

##### love.graphics.getScissor()
Returns the `x`, `y`, `width` and `height` of the scissor rectangle, or nothing
if no scissor is set.

##### love.graphics.setScissor([x, y, width, height])
Limits all drawing -- including `love.graphics.clear()` -- to the given
rectangle. The rectangle is in screen (or canvas) coordinates and is not
affected by `love.graphics.translate()`. If called without arguments the
scissor is disabled.

##### love.graphics.push()
Saves the current transformation (see `love.graphics.translate()`) on a stack;
up to 64 transformations can be pushed.

##### love.graphics.pop()
Restores the transformation saved by the last `love.graphics.push()`.

##### love.graphics.origin()
Resets the transformation so that nothing is translated.

##### love.graphics.translate(dx, dy)
Moves the origin of all following drawing operations by `dx`, `dy`. Unlike in
LÖVE rotation and scaling of the coordinate system are not supported, as these
would make all of the drawing functions too slow on DOS machines; images can
still be rotated and scaled individually with `love.graphics.draw()`.

##### love.graphics.reset()
Resets the font, color, background color, canvas, blend mode, scissor and
transformation to their defaults.

##### love.graphics.clear(red, green, blue)
Clears the screen (or canvas) to the color. If no color argument is given
then the background color is used (see `love.graphics.setBackgroundColor()`).

##### love.graphics.draw(drawable [, quad] [, x, y, r, sx, sy, ox, oy])
Draws the `drawable` -- an Image, a [SpriteBatch](#spritebatch) or a
[Text](#text) -- to the screen with its origin at the given `x`, `y` position.
If a `quad` argument is provided then the image is clipped to the provided quad
when drawn. `r` is the rotation in radians, `sx` and `sy` the horizontal and
vertical scale (`sy` defaults to `sx`) and `ox`, `oy` the origin within the
image around which the image is rotated and scaled.

Unrotated images with a scale of `1` or `-1` (mirrored horizontally) are drawn
with a fast copy; any other rotation or scale maps each pixel individually and
is several times slower.

For compatibility with older versions of LoveDOS
`love.graphics.draw(image [, quad], x, y, flip)` is also supported: if `flip`
is `true` the image is flipped horizontally.

##### love.graphics.point(x, y)
Draws a pixel.

##### love.graphics.points(x, y, ...)
##### love.graphics.points(points)
Draws pixels. `points` can be a table of numbers (`{x1, y1, x2, y2, ...}`) or
a table of tables (`{{x1, y1}, {x2, y2}, ...}`). Drawing many points in one
call is much faster than calling `love.graphics.point()` repeatedly.

##### love.graphics.line(x, y, x2, y2 [, ...])
##### love.graphics.line(points)
Draws a line from the positition `x`, `y` to `x2`, `y2`. You can continue
passing point positions to draw a polyline. The points can also be given as a
table as for `love.graphics.points()`.

##### love.graphics.rectangle(mode, x, y, width, height)
Draws a rectange and the `x`, `y` position of the given `width` and `height`.
`mode` should be either `"fill"` or `"line"`.

##### love.graphics.circle(mode, x, y, radius)
Draws a circle of a given `radius` with its center at the `x`, `y` position.
`mode` should be either `"fill"` or `"line"`.

##### love.graphics.ellipse(mode, x, y, radiusx [, radiusy])
Draws an ellipse with its center at the `x`, `y` position. `mode` should be
either `"fill"` or `"line"`.

##### love.graphics.polygon(mode, x1, y1, x2, y2, x3, y3 [, ...])
##### love.graphics.polygon(mode, points)
Draws a polygon with at least three vertices, which can also be given as a
table as for `love.graphics.points()`. `mode` should be either `"fill"` or
`"line"`. Filled polygons may be concave or self-intersecting (the even-odd
rule is used).

##### love.graphics.print(text [, x, y])
Draws the `text` string in the current font with its top left at the `x`, `y`
position.

##### love.graphics.printf(text, x, y, limit [, align])
Draws the `text` string, wrapped at spaces so that no line is wider than
`limit` pixels. `align` can be `"left"` (the default), `"center"`, `"right"`
or `"justify"`.

##### love.graphics.newImage(filename)
Creates and returns a new image. `filename` should be the name of an image file.
LoveDOS is limited to a palette of 255 unique colors in any given game; it is up
to the user not to exceed this limit.

##### love.graphics.newCanvas([width, height])
Creates and returns a new blank image of the size `width`, `height`. If a
`width` and `height` are not provided then the image will be the same
dimensions as the screen.

##### love.graphics.newQuad(x, y, width, height)
Creates and returns a new quad.

##### love.graphics.newFont([filename ,] ptsize)
Creates and returns a new font. `filename` should be the name of a ttf file and
`ptsize` its size. If no `filename` is provided the built in font is used.

##### love.graphics.setNewFont([filename ,] ptsize)
Creates a font like `love.graphics.newFont()`, sets it as the current font and
returns it.

##### love.graphics.newImageFont(image, glyphs [, extraspacing])
Creates and returns a font from an image (or the filename of one) containing
the characters of the `glyphs` string side by side. The glyphs are separated
by columns whose top pixel has the color of the image's top left pixel;
pixels of this color within glyphs are transparent. `extraspacing` adds
pixels between characters. Unlike other fonts the glyphs keep their own colors
instead of being drawn in the current color. Only ASCII characters are
supported; other characters in `glyphs` skip their glyph.

##### love.graphics.newSpriteBatch(image [, maxsprites])
Creates and returns a new [SpriteBatch](#spritebatch) for drawing many copies
(or parts) of `image` with a single call. `maxsprites` is the initial size of
the buffer, which grows as needed.

##### love.graphics.newText(font [, text])
Creates and returns a new [Text](#text) object, text which is laid out and
rendered once and can then be drawn quickly.

##### love.graphics.present()
Flips the current screen buffer with the displayed screen buffer. This is
called automatically after the `love.draw()` callback.


### love.timer
Provides an interface to your system's clock.

##### love.timer.getDelta()
Returns the time between the last two frames.

##### love.timer.getAverageDelta()
Returns the average delta time over the last second.

##### love.timer.getFps()
Returns the current frames per second

##### love.timer.getTime()
Returns the number of seconds since some time in the past. The value returned
by this function should be used only as a comparison with other values returned
by this function.

##### love.timer.step()
Measures the time between two frames and returns it. This is automatically
called each frame.

##### love.timer.sleep(seconds)
Pauses the thread for the specified number of `seconds`. During this time no
callbacks are called.


### love.keyboard
##### love.keyboard.isDown(key, ...)
Returns true if any of the given keys are currently pressed.

##### love.keyboard.setKeyRepeat(enable)
Dictates whether repeat `keypressed` events should occur if a key is held down.
By default this is `false`.


### love.mouse
##### love.mouse.getPosition()
Returns 2 values: the current horizontal and vertical position of the mouse.

##### love.mouse.getX()
Returns the horizontal position of the mouse.

##### love.mouse.getY()
Returns the vertical position of the mouse.

##### love.mouse.isDown(button, ...)
Returns true if any of the given mouse buttons are currently pressed.  `button`
should be the value `1` (left), `2` (right) or `3` (middle).


### love.filesystem
##### love.filesystem.setIdentity(name)
Sets the save directory: files are written to `save\<name>`, where `<name>`
is shortened to a valid DOS directory name of up to 8 characters. Without an
identity (the default) files are written to `save`. The identity is usually
set in [conf.lua](#configuration-conflua).

##### love.filesystem.getIdentity()
Returns the identity, or `nil` if none is set.

##### love.filesystem.mount(path)
Mounts the given path. `path` should be either a directory or tar archive.
Returns `nil` and an error message on failure.

##### love.filesystem.unmount(path)
Unmounts the given `path`. Returns `nil` and an error message on failure.

##### love.filesystem.exists(path)
Returns `true` if the given path exists.

##### love.filesystem.isFile(filename)     
Returns `true` if `filename` is a file.

##### love.filesystem.isDirectory(dir)
Returns `true` if `dir` is a directory.

##### love.filesystem.read(filename)
Reads and returns the contents of the file at `filename`.

##### love.filesystem.write(filename, string)
Writes `string` to the given `filename` in the game's save directory.

##### love.filesystem.append(filename, string [, size])
Appends `string` (or its first `size` bytes) to the given `filename` in the
save directory. Returns `true`, or `false` and an error message.

##### love.filesystem.remove(name)
Removes a file or an empty directory from the save directory. Returns `true`
on success.

##### love.filesystem.createDirectory(name)
Creates a directory (and its parent directories) in the save directory.
Returns `true` on success.

##### love.filesystem.getInfo(path [, filtertype] [, info])
Returns a table with the fields `type` (`"file"` or `"directory"`), `size`
and `modtime` (seconds since the epoch), or `nil` if the path doesn't exist.
If `filtertype` is given, `nil` is also returned if the path is of another
type. If an `info` table is given it is filled and returned instead of a new
table.

##### love.filesystem.getDirectoryItems(dir)
Returns a table with the names of the files and directories in `dir`, in all
mounted directories and archives.

##### love.filesystem.lines(filename)
Returns an iterator over the lines of a file.

##### love.filesystem.load(filename)
Loads a Lua file and returns it as a function (without running it), or `nil`
and an error message.

##### love.filesystem.getSize(filename)
##### love.filesystem.getLastModified(filename)
Return the size of a file in bytes and the time it was last modified (LÖVE
0.10; see `love.filesystem.getInfo()`).

##### love.filesystem.getRealDirectory(filename)
Returns the mounted directory or archive which contains the file.

##### love.filesystem.getSaveDirectory()
Returns the full path of the save directory.

##### love.filesystem.getWorkingDirectory()
Returns the current working directory. `getUserDirectory()` and
`getAppdataDirectory()` return the same.

##### love.filesystem.getSource()
Returns the path of the game: its directory, archive or packaged exe.

##### love.filesystem.getSourceBaseDirectory()
Returns the directory containing the game's source.

##### love.filesystem.isFused()
Returns `true` if the game is packaged into the exe (see
[packaging](packaging.md)).

##### love.filesystem.newFile(filename [, mode])
Returns a new [File](#file); if `mode` (`"r"`, `"w"` or `"a"`) is given the file
is opened, and `nil` and an error message are returned if this fails.

##### love.filesystem.newFileData(filename)
##### love.filesystem.newFileData(contents, name)
Returns a new [FileData](#filedata) with the contents of a file, or of the
given string.

##### love.filesystem.getRequirePath()
##### love.filesystem.setRequirePath(paths)
Gets and sets the paths searched by `require`, by default `"?.lua;?/init.lua"`.


### love.audio
Provides an interface for playing sounds. Audio is output through a Sound
Blaster 16 (or compatible) as 16 bit stereo at 22050Hz; the card is configured
by the `BLASTER` environment variable (eg. `A220 I7 D1 H5 T6`, as set by
DOSBox). If no Sound Blaster 16 is found LoveDOS still runs, but no sound is
played and `Source:play()` returns `false`.

Sources which are playing or paused are kept alive by the audio system, so a
source does not need to be kept in a variable for it to play to its end.

##### love.audio.newSource(filename [, type])
##### love.audio.newSource(soundData)
##### love.audio.newSource(decoder)
Creates and returns a new audio source. `filename` should be the filename of
the `.wav` file to load; 8 and 16 bit PCM files in mono or stereo with any
sample rate are supported. `type` can be `"static"` or `"stream"` (the
default). `"stream"` sources from files of 256KB or more are streamed from
disk while they play, so long music doesn't have to fit into memory; other
sources are loaded into memory. If a `SoundData` is passed a `"static"` source
is created which plays a copy of the sound data; a [Decoder](#decoder) creates
a streamed source of its file.

Streamed sources are refilled once per frame (by `love.event.pump()`) and
hold about 1.5 seconds of audio (at 22050Hz, 16 bit, mono), so frames
shouldn't take longer than that while they play.

##### love.audio.newQueueableSource(samplerate, bitdepth, channels [, buffercount])
Creates a source which plays the [SoundData](#sounddata) queued with
`Source:queue()`, for example generated audio. It has up to `buffercount`
(by default 8, at most 15) buffers and stops playing when it runs out of
data.

##### love.audio.play(source, ...)
Plays the given sources. Each argument can either be a source or a table
containing sources. Returns `true` if all the sources started playing.

##### love.audio.pause([source, ...])
Pauses the given sources (each argument can be a source or a table of
sources). If no arguments are given all the playing sources are paused and a
table containing them is returned.

##### love.audio.resume([source, ...])
Resumes the given paused sources, or all the paused sources if no arguments are
given.

##### love.audio.stop([source, ...])
Stops the given sources, or all the sources if no arguments are given.

##### love.audio.rewind([source, ...])
Rewinds the given sources, or all the playing and paused sources if no
arguments are given.

##### love.audio.getActiveSourceCount()
Returns the number of sources which are currently playing or paused.
`love.audio.getSourceCount()` is an alias of this function.

##### love.audio.getActiveSources()
Returns a table containing all the sources which are currently playing or
paused.

##### love.audio.setVolume(volume)
Sets the master volume, by default this is `1`.

##### love.audio.getVolume()
Returns the master volume.

##### love.audio.setPosition(x, y [, z])
Sets the position of the listener, by default this is `0, 0, 0`. Mono sources
are attenuated and panned between the left and right speaker depending on
their position relative to the listener (see `Source:setPosition()`); stereo
sources are not affected. The positional audio is calculated whenever a source
or the listener changes, so it adds no cost to mixing the audio.

##### love.audio.getPosition()
Returns the position of the listener.

##### love.audio.setOrientation(fx, fy, fz, ux, uy, uz)
Sets the orientation of the listener as a forward and an up vector. The
default orientation is `0, 0, -1, 0, 1, 0`: looking into the screen with `+x`
to the right, which suits 2D games.

##### love.audio.getOrientation()
Returns the forward and up vectors of the listener.

##### love.audio.setVelocity(x, y [, z])
Sets the velocity of the listener, which is used for the doppler effect.

##### love.audio.getVelocity()
Returns the velocity of the listener.

##### love.audio.setDistanceModel(model)
Sets how sources are attenuated with distance. `model` is one of `"none"`,
`"inverse"`, `"inverseclamped"` (the default), `"linear"`, `"linearclamped"`,
`"exponent"` and `"exponentclamped"`; these work as in OpenAL.

##### love.audio.getDistanceModel()
Returns the distance model.

##### love.audio.setDopplerScale(scale)
Sets the strength of the doppler effect, by default this is `1`. Velocities
are in units per second with the speed of sound being `343.3`.

##### love.audio.getDopplerScale()
Returns the doppler scale.


### love.sound
Provides functions for creating and loading raw audio data.

##### love.sound.newSoundData(filename)
Loads the `.wav` file `filename` and returns its audio data as a new
`SoundData`.

##### love.sound.newDecoder(filename [, buffersize])
Returns a new [Decoder](#decoder) which reads a `.wav` file in chunks of
`buffersize` bytes (16384 by default).

##### love.sound.newSoundData(samples [, rate, bits, channels])
Creates and returns a new silent `SoundData` which is `samples` samples long
(per channel). `rate` is the sample rate and defaults to `44100`, `bits` is the
bit depth -- `8` or `16` (the default) -- and `channels` is either `1` (mono)
or `2` (stereo, the default).


### love.math
Provides random numbers, noise and geometry functions.

##### love.math.random([[min,] max])
Returns a random number from the default [RandomGenerator](#randomgenerator):
without arguments a number between `0` and `1`, otherwise an integer between
`min` (by default `1`) and `max`. The generator is the same as LÖVE's, so the
same seed gives the same numbers; it is seeded from the time at startup.

##### love.math.randomNormal([stddev [, mean]])
Returns a normally distributed random number.

##### love.math.setRandomSeed(seed)
##### love.math.setRandomSeed(low, high)
##### love.math.getRandomSeed()
Sets and gets the seed of the default generator; `getRandomSeed()` returns
its low and high 32 bits.

##### love.math.getRandomState()
##### love.math.setRandomState(state)
Gets and sets the state of the default generator as a string.

##### love.math.newRandomGenerator([seed])
Returns a new [RandomGenerator](#randomgenerator).

##### love.math.noise(x [, y [, z [, w]]])
Returns simplex (1 and 2 dimensions) or Perlin (3 and 4 dimensions) noise in
the range `0` to `1`.

##### love.math.isConvex(vertices)
Returns `true` if the polygon (`x1, y1, x2, y2, ...` or a table of them) is
convex.

##### love.math.triangulate(vertices)
Splits a simple polygon into triangles; returns a table of triangles, each a
table `{x1, y1, x2, y2, x3, y3}`.

##### love.math.newBezierCurve(vertices)
Returns a new [BezierCurve](#beziercurve) with the given control points.

##### love.math.newTransform([x, y, angle, sx, sy, ox, oy, kx, ky])
Returns a new [Transform](#transform).

##### love.math.colorToBytes(r, g, b [, a])
##### love.math.colorFromBytes(r, g, b [, a])
Convert colors between the ranges `0` to `1` and `0` to `255`.

##### love.math.gammaToLinear(r, g, b)
##### love.math.linearToGamma(r, g, b)
Convert colors between sRGB and linear RGB.


### love.joystick
Joysticks connected to the PC gameport: up to two joysticks with two axes and
two buttons each, or one joystick with four buttons. Joysticks are detected
at startup, when they should be centered; their range is calibrated as they
are moved. If a joystick is found the gameport is read once per frame, which
takes up to about 1.5ms.

##### love.joystick.getJoysticks()
Returns a table of the connected [Joysticks](#joystick).

##### love.joystick.getJoystickCount()
Returns the number of connected joysticks.

`setGamepadMapping()`, `loadGamepadMappings()`, `saveGamepadMappings()` and
`getGamepadMappingString()` exist for compatibility and have no effect.


### love.event
##### love.event.quit([status])
Pushes the `quit` event with the given `status`. `status` is `0` by default.

##### love.event.push(name, ...)
Adds an event to the queue. The default `love.run()` calls
`love.handlers[name](...)` for each event; add a function to `love.handlers`
to handle custom events.

##### love.event.poll()
Returns an iterator over the queued events:
`for name, a, b, c, d, e, f in love.event.poll() do ... end`.

##### love.event.pump()
Adds the events from the keyboard, mouse and joysticks to the queue.

##### love.event.wait()
Waits for an event and returns it.

##### love.event.clear()
Removes all queued events.


## Objects
### Image
A loaded image or canvas which can be drawn.

##### Image:getDimensions()
Returns the width and height of the image in pixels as two numbers.

##### Image:getWidth()
Returns the width in pixels of the image.

##### Image:getHeight()
Returns the height in pixels of the image.

##### Image:getPixel(x, y)
Returns the color of the pixel at the position `x`, `y` of the image. If the
position is out of bounds or the pixel is set to transparent then `nil` is
returned.

##### Image:setPixel(x, y [, red, green, blue])
Sets the pixel of the image at position `x`, `y` to the given color; if no color
is provided the pixel is set to transparent. If the position is out of bounds
then no change is made.


### Quad
A rectangle used to represent the clipping region of an image when drawing.

##### Quad:setViewport(x, y, width, height)
Sets the position and dimensions of the quad

##### Quad:getViewport()
Returns the position (`x`, `y`) and dimensions (`width`, `height`) of the quad,
4 numerical values in total.


### Font
A font used by `love.graphics.print()`. The current font can be set using the
`love.graphics.setFont()` function.

##### Font:getWidth(text)
Returns the width in pixels that the `text` string would take when printed using
this font. For text with multiple lines the width of the widest line is
returned.

##### Font:getHeight()
Returns the height of the font in pixels.

##### Font:getWrap(text, limit)
Returns the width of the widest line and a table containing the lines the
`text` string is split into when wrapped at `limit` pixels, as done by
`love.graphics.printf()`.


### Source
A sound which can be played; created with `love.audio.newSource()`. Functions
which take a `unit` argument accept `"seconds"` (the default) or `"samples"`.

##### Source:play()
Plays the audio source. If the source is already playing then this function has
no effect; if it is paused playback continues where it left off. To play back
from the start call `Source:stop()` before calling this function. Returns
`true` on success, or `false` if no sound device is available.

##### Source:pause()
Pauses the source's playback. This stops playback without losing the current
position, calling `Source:play()` will continue playing where it left off.

##### Source:resume()
Resumes playback of a paused source.

##### Source:stop()
Stops playing and rewinds the source's play position back to the beginning.

##### Source:rewind()
Moves the source's play position back to the beginning without changing
whether it is playing.

##### Source:seek(offset [, unit])
Sets the play position to `offset`. If the source is stopped playback will
start from this position when `Source:play()` is called.

##### Source:tell([unit])
Returns the current play position.

##### Source:getDuration([unit])
Returns the length of the source's audio data.

##### Source:clone()
Returns a new stopped source which plays the same audio data and has the same
settings (volume, pitch, looping and volume limits) as this one. Clones share
their audio data, which makes them cheap to create -- this is useful for
playing the same sound effect several times at once.

##### Source:setVolume(volume)
Sets the volume -- by default this is `1`.

##### Source:getVolume()
Returns the volume.

##### Source:setVolumeLimits(min, max)
Sets the minimum and maximum volume of the source; both values must be between
`0` and `1`. The volume set by `Source:setVolume()` is clamped to these limits
when playing. By default the limits are `0` and `1`.

##### Source:getVolumeLimits()
Returns the minimum and maximum volume limits.

##### Source:setPitch(pitch)
Sets the pitch (playback speed). By default this is `1`. `0.5` is half the
pitch, `2` is double the pitch. `pitch` must be larger than `0`.

##### Source:getPitch()
Returns the pitch.

##### Source:setLooping(enable)
Enables looping if `enable` is `true`. By default looping is disabled.

##### Source:isLooping()
Returns `true` if looping is enabled.

##### Source:isPlaying()
Returns `true` if the source is currently playing.

##### Source:isPaused()
Returns `true` if the source is currently paused.

##### Source:isStopped()
Returns `true` if the source is currently stopped.

##### Source:getType()
Returns the type of the source: `"static"`, `"stream"` or `"queue"`.

##### Source:queue(soundData)
Adds a copy of the sound data, which must have the same format as the source,
to a queueable source's queue. Returns `false` if all buffers are in use.

##### Source:getFreeBufferCount()
Returns the number of free buffers of a queueable source.

##### Source:getChannelCount()
Returns the number of channels of the source's audio data: `1` for mono or `2`
for stereo. `Source:getChannels()` is an alias of this function.

The following positional audio functions can only be used with mono sources;
using them with stereo sources raises an error.

##### Source:setPosition(x, y [, z])
Sets the position of the source, by default this is `0, 0, 0`.

##### Source:getPosition()
Returns the position of the source.

##### Source:setVelocity(x, y [, z])
Sets the velocity of the source, which is used for the doppler effect.

##### Source:getVelocity()
Returns the velocity of the source.

##### Source:setRelative(enable)
If `enable` is `true` the source's position is relative to the listener (in
the listener's coordinate system) rather than absolute.

##### Source:isRelative()
Returns `true` if the source's position is relative to the listener.

##### Source:setAttenuationDistances(reference, max)
Sets the distance below which the source is played at full volume (by default
`1`) and the distance beyond which it is no longer attenuated any further.

##### Source:getAttenuationDistances()
Returns the reference and maximum attenuation distances.

##### Source:setRolloff(rolloff)
Sets how fast the source is attenuated with distance, by default this is `1`.

##### Source:getRolloff()
Returns the rolloff factor.

##### Source:setDirection(x, y [, z])
Sets the direction the source points to. By default this is `0, 0, 0`, which
means it is omnidirectional.

##### Source:getDirection()
Returns the direction of the source.

##### Source:setCone(innerAngle, outerAngle [, outerVolume])
Sets the cone of a directional source: inside `innerAngle` (in radians) the
source has its full volume, outside `outerAngle` it plays at `outerVolume`
(by default `0`) and in between the volume is interpolated.

##### Source:getCone()
Returns the inner angle, outer angle and outer volume of the source's cone.


### Decoder
Reads audio from a file in chunks, created with `love.sound.newDecoder()`.

##### Decoder:decode()
Returns the next chunk as a new [SoundData](#sounddata), or `nil` at the end
of the file.

##### Decoder:seek(offset)
##### Decoder:tell()
Set and get the position in seconds.

##### Decoder:getDuration()
##### Decoder:getSampleRate()
##### Decoder:getBitDepth()
##### Decoder:getChannelCount()
Return the duration in seconds and the format of the audio.

##### Decoder:clone()
Returns a new decoder for the same file.


### SoundData
Raw audio data which can be read and modified sample by sample, for example to
generate sound effects; created with `love.sound.newSoundData()`. Sample values
are numbers between `-1` and `1`. Sample indices start at `0`.

##### SoundData:getSample(i)
##### SoundData:getSample(i, channel)
Returns the value of a sample. With one argument `i` is the index of the
sample in the interleaved data -- for stereo data even indices are on the left
channel and odd ones on the right channel. If `channel` (`1` or `2`) is given,
`i` is the index of the sample within that channel.

##### SoundData:setSample(i, value)
##### SoundData:setSample(i, channel, value)
Sets the value of a sample; the arguments `i` and `channel` work as in
`SoundData:getSample()`. `value` is clamped to the range `-1` to `1`.

##### SoundData:getSampleCount()
Returns the number of samples per channel.

##### SoundData:getSampleRate()
Returns the sample rate.

##### SoundData:getBitDepth()
Returns the bit depth: `8` or `16`.

##### SoundData:getChannelCount()
Returns the number of channels. `SoundData:getChannels()` is an alias of this
function.

##### SoundData:getDuration()
Returns the duration in seconds.

##### SoundData:getSize()
Returns the size of the raw audio data in bytes.

##### SoundData:getString()
Returns the raw audio data as a string.

##### SoundData:clone()
Returns a copy of the sound data.


### SpriteBatch
Many sprites drawn with a single call to `love.graphics.draw()`, which is much
faster than drawing them separately: on a 386 most of the time of drawing a
small image is spent calling it from Lua. Sprite ids start at `0`, or at `1`
for games written for LÖVE 11 (see [conf.lua](#configuration-conflua)).

##### SpriteBatch:add([quad,] x, y, r, sx, sy, ox, oy)
Adds a sprite: the image (or the part of it given by `quad`) with the same
transformation arguments as `love.graphics.draw()`. Returns the sprite's id.

##### SpriteBatch:set(id, [quad,] x, y, r, sx, sy, ox, oy)
Changes the sprite with the given id.

##### SpriteBatch:clear()
Removes all sprites.

##### SpriteBatch:getCount()
Returns the number of sprites.

##### SpriteBatch:getBufferSize()
##### SpriteBatch:setBufferSize(size)
Gets and sets the number of sprites which fit without growing the buffer.

##### SpriteBatch:getTexture()
##### SpriteBatch:setTexture(image)
Gets and sets the image used by the sprites.

##### SpriteBatch:setDrawRange([start, count])
Only draws `count` sprites starting with the `start`th one (counted from `1`).
Without arguments all sprites are drawn.

##### SpriteBatch:getDrawRange()
Returns the first sprite and the number of sprites which are drawn.

Sprite batches are drawn with `love.graphics.draw(spritebatch, x, y)`; they
can't be rotated or scaled as a whole (their sprites can).
`SpriteBatch:setColor()` and `flush()` have no effect.


### Text
Text which is laid out and rendered once; drawing it costs about as much as
drawing an image of the same size and is several times faster than printing
the text. Like printed text it is drawn in the current color.

##### Text:set(text)
Replaces the text. `text` can also be a table of colors and strings as in
LÖVE 11, whose colors are ignored.

##### Text:setf(text, wraplimit, align)
Replaces the text with text wrapped and aligned as by
`love.graphics.printf()`.

##### Text:add(text [, x, y])
##### Text:addf(text, wraplimit, align [, x, y])
Adds text at the position `x`, `y`; returns its index.

##### Text:clear()
Removes all text.

##### Text:getWidth()
##### Text:getHeight()
##### Text:getDimensions()
Return the size of the text in pixels.

##### Text:getFont()
##### Text:setFont(font)
Gets and sets the font used by the text.


### Joystick
A gameport joystick. `Joystick:getAxis(i)` returns the position of axis `i`
(`1` horizontal, `2` vertical) from `-1` to `1`, `getAxes()` both positions,
`isDown(button, ...)` whether any of the buttons is pressed,
`getAxisCount()`, `getButtonCount()`, `getName()`, `getID()` and
`isConnected()` describe it. Hats, gamepad functions and vibration are not
supported by gameport joysticks: `getHatCount()` returns `0`, `isGamepad()`
and `isVibrationSupported()` return `false`.


### File
A file opened with `love.filesystem.newFile()`. Files opened for reading
(`"r"`) are read into memory, which also works for files in archives; files
opened for writing (`"w"`) or appending (`"a"`) are written to the save
directory.

##### File:open(mode)
##### File:close()
Open and close the file.

##### File:read([bytes])
Reads `bytes` bytes (by default the rest of the file); returns the data and
the number of bytes read.

##### File:write(data [, size])
Writes `data`, or its first `size` bytes.

##### File:lines()
Returns an iterator over the remaining lines.

##### File:seek(position)
##### File:tell()
Set and get the position in bytes.

##### File:getSize()
##### File:isEOF()
##### File:isOpen()
##### File:getMode()
##### File:getFilename()
Return the size, whether the end was reached, whether the file is open, its
mode (`"c"` when closed) and its name.


### FileData
The contents of a file in memory, created with `love.filesystem.newFileData()`.
`FileData:getString()`, `getSize()`, `getFilename()` and `getExtension()`
return its contents, size, name and extension.


### RandomGenerator
An independent random number generator, created with
`love.math.newRandomGenerator()`. It has the methods `random()`,
`randomNormal()`, `setSeed()`, `getSeed()`, `setState()`, `getState()` which
work like the `love.math` functions of the same names, and `clone()`.


### BezierCurve
A Bézier curve, created with `love.math.newBezierCurve()`.

* `BezierCurve:evaluate(t)` returns the point at `t` (`0` to `1`)
* `BezierCurve:render([depth])` returns the curve as points
  `{x1, y1, x2, y2, ...}` for `love.graphics.line()`; it has `2^depth + 1`
  points (`depth` is `5` by default)
* `BezierCurve:renderSegment(t1, t2 [, depth])` and `getSegment(t1, t2)`
  work on the part from `t1` to `t2`
* `BezierCurve:getDerivative()` returns the derivative as a new curve
* `getControlPoint(i)`, `setControlPoint(i, x, y)`,
  `insertControlPoint(x, y [, i])`, `removeControlPoint(i)`,
  `getControlPointCount()` and `getDegree()` edit the control points (negative
  indices count from the end)
* `translate(dx, dy)`, `rotate(angle [, ox, oy])` and `scale(s [, ox, oy])`
  transform the curve


### Transform
A 2D transformation, created with `love.math.newTransform()`. It has the
methods `translate(x, y)`, `rotate(angle)`, `scale(sx [, sy])`,
`shear(kx, ky)`, `apply(other)`, `setTransformation(...)` (with the same
arguments as `love.math.newTransform()`), `transformPoint(x, y)`,
`inverseTransformPoint(x, y)`, `inverse()`, `clone()`, `reset()`,
`getMatrix()` and `setMatrix(...)`; transforms can be multiplied with `*`.
Transforms are only used for calculations: `love.graphics` can only translate
the coordinate system (see `love.graphics.translate()`).


## Callbacks
##### love.load(args)
Called when LoveDOS is started. `args` is a table containing the command line
arguments passed to LoveDOS.

##### love.update(dt)
Called at the beginning of each frame, `dt` is the amount of time in seconds
which has passed since the last frame. This is where all the game logic should
take place.

##### love.draw()
Called when the frame is ready to be drawn. All your draw calls should take
place in this function.

##### love.keypressed(key, code, isrepeat)
Called when the user presses a key. `key` is the key that was pressed, `code` is
the scancode of the pressed key, `isrepeat` is true if this key press event is a
repeat.

##### love.keyreleased(key, code)
Called when the user releases a key. `key` is the key that was released, `code`
is the scancode for the released key.

##### love.textinput(text)
Called when text has been entered by the user. For example if shift-2 is pressed
on an American keyboard layout, the text "@" will be generated.

##### love.mousemoved(x, y, dx, dy)
Called when the user moves the mouse. `x` and `y` are the mouse's current
position, `dx` and `dy` is the amount moved relative to the last mouse position.

##### love.mousepressed(x, y, button)
Called when the user presses a mouse button. `x` and `y` are the mouse's current
position. `button` is the value `1` (left), `2` (right) or `3` (middle).

##### love.joystickadded(joystick)
Called at startup for each joystick which is found.

##### love.joystickpressed(joystick, button)
##### love.joystickreleased(joystick, button)
Called when a joystick button is pressed or released.

##### love.joystickaxis(joystick, axis, value)
Called when a joystick axis moves; `value` is from `-1` to `1`.

##### love.mousereleased(x, y, button)
Called when the user releases a mouse button. `x` and `y` are the mouse's
current position. `button` is the value `1` (left), `2` (right) or `3` (middle).

##### love.errhand(err)
##### love.errorhandler(err)
Called when an unprotected error occurs; `err` is the error message. By default
this function displays the error message and stacktrace on screen and waits for
the `escape` key to be pressed before exiting. `love.errorhandler` is the
LÖVE 11 name and is used if it is defined; like in LÖVE 11 it may return a
function which is then called repeatedly until it returns a value.

##### love.run()
The main loop, which calls the other callbacks. It can be replaced by a game:
either with a function that runs the loop itself (LÖVE 0.10) or, like in
LÖVE 11, with a function that returns a function to be called once per frame
until it returns an exit status. Each frame the default loop resets the
transformation (`love.graphics.origin()`) and clears the screen before calling
`love.draw()`.


## Configuration (conf.lua)
If the game contains a `conf.lua` file it is run before `main.lua`. It should
define a `love.conf(t)` function which receives a table of settings with the
same fields and defaults as in LÖVE; for example:

```lua
function love.conf(t)
  t.identity = "mygame"   -- Save directory, see love.filesystem.setIdentity()
  t.version = "11.4"      -- The LÖVE version the game was written for
  t.modules.audio = false -- Don't use the audio module
end
```

The following fields have an effect in LoveDOS; the others are accepted and
ignored:

Field              | Effect
-------------------|-------------------------------------------------------
`t.identity`       | The save directory, see `love.filesystem.setIdentity()`
`t.version`        | `"11.0"` or later selects LÖVE 11's color range (below)
`t.modules.<name>` | `false` removes `love.<name>`. Disabling `audio` also leaves the Sound Blaster untouched, which saves the CPU time used for mixing

#### Colors
By default colors are given and returned as red, green and blue values from
`0` to `255`, as in LÖVE 0.10. Games which set `t.version` to `"11.0"` or later
use values from `0` to `1`, as in LÖVE 11. This applies to
`love.graphics.setColor()`, `getColor()`, `setBackgroundColor()`,
`getBackgroundColor()`, `clear()` and `Image:getPixel()`/`setPixel()`. Colors
can also be given as a table (`{r, g, b}`); an alpha value is accepted and
ignored, and returned as the maximum (`255` or `1`).
