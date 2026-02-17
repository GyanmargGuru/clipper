#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "raylib.h"
#include "audio_loader.h"
#include <stdbool.h>

// Initializes the audio player system
void InitAudioPlayerSystem(void);

// Closes and cleans up audio player system
void CloseAudioPlayerSystem(void);

// Loads an audio file and prepares for playback
// Returns true if successful
bool LoadTrack(const char *fileName);

// Updates the player state (if using non-callback updates, though callback is preferred)
void UpdatePlayer(void);

// Playback controls
void PlayTrack(void);
void PauseTrack(void);
void StopTrack(void);
bool IsTrackPlaying(void);

// Visualization info
float GetTrackTime(void); // Current playback time in seconds
float GetTrackDuration(void); // Total duration in seconds
void SeekTrack(float time);

// Speed control (0.5x to 3.0x)
void SetTrackSpeed(float speed);
float GetTrackSpeed(void);

// Logic properties
AudioBuffer *GetTrackBuffer(void); // For visualization access

#endif
