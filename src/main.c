#include "raylib.h"
#include "audio_player.h"
#include "ui.h"
#include <stdio.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

int main(void) {
    // Initialization
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Audio Clipper");
    
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        printf("Error: Audio device could not be initialized.\n");
        return 1;
    }
    
    InitAudioPlayerSystem();
    InitUI();

    SetTargetFPS(60);

    // Main game loop
    while (!WindowShouldClose()) {
        // Update
        UpdateUI();
        UpdatePlayer(); // Audio updates (if using non-callback fallback, usually no-op)

        // Draw
        BeginDrawing();
            ClearBackground(GetColor(0x202020FF));
            DrawUI();
        EndDrawing();
    }

    // De-Initialization
    CloseAudioPlayerSystem();
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
