#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "audio_player.h" // Access to audio buffer for visualization

void InitUI(void);
void UpdateUI(void);
void DrawUI(void);

// Accessors for saving clips
float GetSelectionStart(void);
float GetSelectionEnd(void);

#endif
