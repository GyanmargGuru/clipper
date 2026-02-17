# Audio Clipper

A lightweight, high-performance audio visualization and clipping tool built with C and Raylib. Designed for precision real-time audio waveform rendering and fast clipping capabilities.

![Interface Screenshot](https://gyanmarg.guru/clipper/clipper.png)

## Features

- **Real-time Waveform Rendering**: Inspect audio with millisecond precision.
- **Precision Clipping**: Select regions and save them instantly using FFmpeg.
- **Marker System**: Add and manage markers with a right-click context menu.
- **Variable Playback Speed**: Adjustable speed from 0.5x to 3.0x without pitch shift.
- **High-Quality UI**: Modern interface with custom JetBrains Mono typography.
- **Cross-Format Support**: Loads WAV, MP3, OGG, and MP4.

## Dependencies

To build and run Audio Clipper, you need the following:

- **GCC**: Version 11.0 or higher (recommended).
- **Make**: Standard build utility.
- **FFmpeg**: Required for audio decoding and saving clips (must be in your PATH).
- **Raylib 5.0**: Included in the repository (`src/vendor/raylib`).
- **Standard Libraries**: `m`, `pthread`, `dl`.

## Build Instructions

1. Clone the repository:
   ```bash
   git clone https://github.com/GyanmargGuru/clipper.git
   cd clipper
   ```

2. Compile the project:
   ```bash
   make
   ```

3. Run the application:
   ```bash
   ./bin/audio_clipper
   ```

## Usage Guide

### Mouse Controls
- **Left Click (Waveform)**: Seek to position or drag to create a selection region.
- **Middle Click Drag**: Pan the waveform view.
- **Mouse Wheel**: Zoom in/out at the cursor position.
- **Right Click**: Open the context menu to add/remove markers or select between them.

### Keyboard Shortcuts
- **Space**: Play / Pause playback.

### Marker System
- **Add Marker**: Places a yellow vertical line at the cursor.
- **Remove Marker**: Removes the nearest marker to the cursor.
- **Select Between Markers**: Automatically creates a selection region between the two markers bracketing your cursor.

---

Built with ❤️ using [Raylib](https://www.raylib.com/).

Project Manager: [Gyanmarg](https://gyanmarg.guru)

Lead Developer: [Antigravity](https://antigravity.google)
