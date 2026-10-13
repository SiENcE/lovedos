# LoveDOS roadmap: closing the gap to the LÖVE API

LoveDOS implements a subset of the [LÖVE API](https://love2d.org/wiki). This
plan adds the parts of the LÖVE 11.5 API that make sense on DOS machines (a
386 or 486 with VGA and a Sound Blaster), in order of urgency. Anything that
needs a GPU, threads or far more CPU than a 486 has (shaders, meshes, 3D
textures, `love.physics`, `love.thread`, `love.video`, `love.touch`, audio
effects, hashing/compression) is deliberately left out.

Every phase is tested in DOSBox with test games in [test/](../test) and
anything which runs per frame is benchmarked for a 386DX-30 and a 486DX2-66
(see the README's Performance section).

Status: `[ ]` todo, `[x]` done.


## Phase 1: compatibility with LÖVE 11 games (most urgent)

- [x] **conf.lua**: run `conf.lua` (if present) with LÖVE's default `t`
      table before `main.lua`
  - `t.identity` selects the save directory (`love.filesystem.setIdentity`)
  - `t.version` selects the color range (below)
  - `t.window.title` etc. are stored for `love.window`
  - `t.modules.<name> = false` disables a module; `t.modules.audio = false`
    also skips initializing the Sound Blaster, saving its CPU time
- [x] **Color range**: LÖVE 11 uses colors from 0 to 1, LoveDOS (like LÖVE
      0.10) from 0 to 255. Games declaring `t.version = "11.x"` get the 0-1
      range in all color functions (`setColor`, `setBackgroundColor`,
      `clear`, `getColor`, `Image:getPixel/setPixel`); colors may also be
      given as tables. The alpha component is accepted and ignored.
- [x] `love.errorhandler` (LÖVE 11 name of `love.errhand`),
      `love.isVersionCompatible`, `love.get/setDeprecationOutput`; LÖVE 11
      style `love.run` returning a loop function
- [x] Found while testing: Lua parsed number literals inaccurately (`0.5` was
      0.49999999999999991) because of DJGPP's `strtod()`


## Phase 2: worth adding

- [ ] **SpriteBatch** (`love.graphics.newSpriteBatch`): many sprites drawn by
      one Lua call; saves the ~0.1ms Lua call overhead per sprite on a 386
- [ ] **Text** (`love.graphics.newText`): text laid out and rendered once,
      drawn as fast as an image in the current color
- [ ] **ImageFont** (`love.graphics.newImageFont`): bitmap fonts from an image
- [ ] **love.joystick** for the PC gameport: `getJoysticks`,
      `getJoystickCount`, `Joystick` objects and the `joystickpressed`,
      `joystickreleased` and `joystickaxis` callbacks
- [ ] **Tracker music**: `love.audio.newSource("song.mod")` for MOD files
      (and S3M/XM if a suitable small integer-only player is available)
- [ ] **love.filesystem (LÖVE 11)**: `getInfo`, `getDirectoryItems`,
      `lines`, `load`, `append`, `remove`, `createDirectory`,
      `getIdentity/setIdentity`, `getSaveDirectory`, `getWorkingDirectory`,
      `getSource`, `getSize`, `getLastModified`, `newFile` / `File` objects
- [ ] **love.math**: `random`, `randomNormal`, `get/setRandomSeed`,
      `get/setRandomState`, `newRandomGenerator` / `RandomGenerator`,
      `noise`, `triangulate`, `isConvex`, `newBezierCurve` / `BezierCurve`,
      `newTransform` / `Transform`, `colorFromBytes`, `colorToBytes`,
      `gammaToLinear`, `linearToGamma`
- [ ] **Queueable sources and decoders**: `love.audio.newQueueableSource`,
      `Source:queue`, `Source:getFreeBufferCount`, `love.sound.newDecoder`
      for streaming audio which doesn't fit into memory


## Phase 3: small but useful

- [ ] `love.event.push`, `clear`, `wait` (custom events reach
      `love.handlers` / `love.<name>` callbacks)
- [ ] `love.mouse`: `setPosition`, `setX`, `setY`, `setVisible`/`isVisible`
      (software cursor), `setRelativeMode`/`getRelativeMode`,
      `setGrabbed`/`isGrabbed`, `isCursorSupported`, `newCursor`,
      `setCursor`/`getCursor`, `getSystemCursor`; the `wheelmoved` callback
      with CuteMouse compatible drivers
- [ ] `love.keyboard`: `hasKeyRepeat`, `isScancodeDown`,
      `getKeyFromScancode`, `getScancodeFromKey`, `setTextInput`,
      `hasTextInput`, `hasScreenKeyboard`
- [ ] `love.graphics`: `arc`, `setLineWidth`/`getLineWidth`,
      `setPointSize`/`getPointSize`, `setLineStyle`/`getLineStyle`,
      `setLineJoin`/`getLineJoin`, `intersectScissor`, `getStackDepth`,
      `transformPoint`, `inverseTransformPoint`, `captureScreenshot`,
      `newImageData` (Image doubles as ImageData), `setNewFont`,
      `setDefaultFilter`/`getDefaultFilter`
- [ ] `ImageData:paste`, `mapPixel`, `encode` (BMP/TGA/PNG), `getFormat`;
      `Image`/`Canvas` texture methods (`getFilter`, `setFilter`, `getWrap`,
      `setWrap`, `getPixelDimensions`, `getDPIScale`, ...)
- [ ] `Font`: `getAscent`, `getDescent`, `getBaseline`, `getLineHeight`,
      `setLineHeight`, `hasGlyphs`, `getKerning`, `setFallbacks`,
      `getFilter`/`setFilter`, `getDPIScale`
- [ ] `Object:type`, `Object:typeOf`, `Object:release` for all objects
- [ ] `love.window.setVSync`/`getVSync`: wait for the VGA vertical retrace
      for tear-free output
- [ ] `love.data`: `encode`/`decode` (base64, hex), `newByteData`, `pack`,
      `unpack`, `getPackedSize`


## Phase 4: compatibility stubs

Functions which have nothing to do on DOS but are often called by LÖVE games,
so that ports don't need changes:

- [ ] `love.window`: `getMode`, `setMode`, `updateMode`, `getTitle`,
      `setTitle`, `getFullscreen`, `setFullscreen`, `getFullscreenModes`,
      `getDesktopDimensions`, `getDisplayCount`, `getDisplayName`,
      `getDisplayOrientation`, `getDPIScale`, `toPixels`, `fromPixels`,
      `getSafeArea`, `hasFocus`, `hasMouseFocus`, `isOpen`, `isVisible`,
      `isMinimized`, `isMaximized`, `minimize`, `maximize`, `restore`,
      `requestAttention`, `getPosition`, `setPosition`, `getIcon`, `setIcon`,
      `isDisplaySleepEnabled`, `setDisplaySleepEnabled`, `showMessageBox`,
      `close`
- [ ] `love.system`: `getProcessorCount`, `getPowerInfo`,
      `getClipboardText`/`setClipboardText` (within the program),
      `openURL`, `vibrate`, `hasBackgroundMusic`
- [ ] `love.graphics`: `getRendererInfo`, `getStats`, `getSupported`,
      `getSystemLimits`, `getCanvasFormats`, `getImageFormats`,
      `getTextureTypes`, `isGammaCorrect`, `isActive`, `getDPIScale`,
      `getPixelDimensions`, `getPixelWidth`, `getPixelHeight`
