# Video and presentation plan

## Goal

Separate Melee's original simulation (60 Hz) from presentation, keeping the former as the
source of truth while offering configurable aspect ratio, resolution/upscaling and
presentation rate without altering frame data, physics, inputs or determinism.

## Current state (implemented)

The Esc menu now offers five working options, inspired by Ship of Harkinian:

1. **RESOLUTION** — internal resolution multiplier: 1x (native 640x528), 2x (1280x1056),
   3x (1920x1584), 4x (2560x2112), 5x (3200x2640). The render framebuffer is recreated
   dynamically; the game rasterizes at high resolution without stretching, and the image
   composited into the window preserves aspect ratio with letterboxing/pillarboxing.

2. **ASPECT** — 4:3 (original), 16:9, 21:9. Applied by computing letterbox/pillarbox in the
   blit to the window. The game's projection and logic do not change.

3. **FILTER** — Nearest (pixel-perfect) or Linear (bilinear). Applied in `glBlitFramebuffer`
   every frame.

4. **WINDOW** — Windowed, Fullscreen (SDL fullscreen) or Borderless (borderless maximized).
   Applied immediately through the SDL APIs and restored when the game is reopened.

5. **RATE** — 60, 120, 144, 165 or 240 FPS. Prepared for future visual pose interpolation;
   the simulation stays at 60 Hz.

All settings persist in `~/.local/share/MeleePC/MeleePC/video-settings.txt`.

## Future implementation order

1. Measure simulation, rasterization, composition and present CPU time separately, showing
   the effective FPS and the bottleneck in the menu.
2. Keep the previous and current visual poses and implement interpolation for the 120, 144,
   165 and 240 FPS targets. Camera cuts, teleports, spawns, scene changes and effects with
   no interpolable data must hold the valid pose.
3. Validate every mode against 60 Hz: simulation and input tests must be identical;
   consecutive captures must prove that the extra frames are distinct and that no mode
   degrades below the chosen target.
4. Support HOR+ widescreen in the camera's field of view (as SoH does), expanding the
   horizontal FOV without stretching the image.

## Acceptance criteria

- Opening the menu does not measurably reduce the presentation rate.
- Aspect, resolution and filter change the image observably and persist.
- 60 FPS remains the original behaviour; targets above it do not run extra game ticks.
- Changing a setting loses no input and does not restart the match.
- Switching to fullscreen or borderless works without flicker or crashes.
