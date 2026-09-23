# GamePad Mic Redirect

An Aroma/WUPS plugin that makes Wii U titles which insist on a headset being
connected accept the GamePad's built-in microphone.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Buy me a Coffee](https://img.shields.io/badge/Support%20My%20Work-Buy%20me%20a%20coffee%20%E2%98%95-chocolate?style=plastic)](https://buymeacoffee.com/tonytrawl)

This project is experimental. The built-in-mic compatibility path was confirmed
on a real Wii U in Black Ops II.

The plugin spoofs the following headset status values used by Wii U software:

- `VPADStatus::usingHeadphones`, returned by `VPADRead`
- bit `0x08` of `VPADStatus::micStatus`, which Black Ops II's
  `WiiU_IsDRCHeadsetConnected` checks
- `VPADBASEGetHeadphoneStatus`

The plugin keeps Black Ops II's microphone presence flag active and replaces
captured samples with silence while muted. This preserves the voice connection
while preventing speech from being sent. It does not record, upload, or add
noise cancellation to the microphone stream.

When a real headset is connected to the GamePad jack, the plugin passes through
the hardware's headset status and microphone samples unchanged. Its TV-button
controls are suspended until the headset is unplugged.

## GamePad TV button controls

- **One quick press:** mute or unmute. The notification appears after the
  350 ms double-press window.
- **Two quick presses:** switch between always-on and push-to-talk modes.
- **Push-to-talk mode:** hold the TV button for the configured time to speak;
  release it to mute. A short tap still toggles the manual mute setting.

The plugin displays an on-screen notification when the mute state or mode
changes. It reserves the GamePad TV button while enabled during gameplay, so
the normal TV remote menu is unavailable until the plugin is disabled, a real
headset is connected, or the game exits.

## Plugin menu settings

- **Default mic mode:** Always on (default) or Press to talk. This is the mode
  applied when changed in the menu, when a game starts, or when a physical
  headset is unplugged. A TV-button double-click still switches modes for the
  current session.
- **Press-to-talk hold time:** 100, 150, 200, 250 (default), 350, or 500 ms.
  This controls how long the TV button must be held before the mic opens.
- **Use built-in mic in headset-only games:** Enables or disables the shim.

## Important compatibility note

This is an experimental compatibility shim. The `micStatus` bit was identified
by inspecting Black Ops II's `WiiU_IsDRCHeadsetConnected` routine.

## Build

With Docker Desktop running:

```sh
docker build -t gamepad-mic-redirect-builder .
docker run --rm -v "${PWD}:/project" gamepad-mic-redirect-builder make
```

For a native devkitPro install, install `wiiu-dev`, WiiUPluginSystem, and
libnotifications, export `DEVKITPRO` and `DEVKITPPC`, then run `make`.

## Install

For a packaged release, extract `wiiu.zip` to the root of the Wii U SD card.
It contains exactly:

```text
wiiu/environments/aroma/plugins/GamePadMicRedirect.wps
```

Alternatively, copy a locally built `GamePadMicRedirect.wps` to:

```text
sd:/wiiu/environments/aroma/plugins/GamePadMicRedirect.wps
```

Restart Aroma or reload plugins. The compatibility shim is enabled by default
and can be toggled in Aroma's plugin configuration menu. On-screen status
messages require Aroma's `NotificationModule.wms` to be installed and loaded.

## Source and release layout

- `src/main.cpp` — plugin source.
- `Dockerfile` and `Makefile` — reproducible build configuration.
- `release.txt` — staged Unbound HQ managed-plugin feed. Its record is not live
  until the matching GitHub Release asset has been uploaded and verified.
- `dist/wiiu.zip` — local release asset staging, ignored by Git; upload it to a
  `v0.4.0` GitHub Release only after the on-console test.

## License and credit

Copyright (C) 2026 Tony (tonytrawl). This project is licensed under the GNU
General Public License version 3.0 only (`GPL-3.0-only`); see `LICENSE`.
You may use, copy, modify, and redistribute it under that license. If you
redistribute the plugin or a modified version, keep the copyright and license
notices, provide the corresponding source as required by the GPL, and mark
your changes. Credit Tony (tonytrawl) in redistributed copies. The GPL does
not require a separate in-game credit screen.
