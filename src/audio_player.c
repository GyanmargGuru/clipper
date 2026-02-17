#include "audio_player.h"
#include "sonic.h" // vendor/sonic.h
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define SAMPLE_RATE 44100
#define CHANNELS 2

static AudioStream stream;
static sonicStream timeStretcher = NULL;
static AudioBuffer *currentAudio = NULL;
static AudioBuffer localAudioBuffer = {0}; // To hold data if we load it
static size_t currentFrameIndex = 0;
static bool isPlaying = false;
static float currentSpeed = 1.0f;
static bool isSystemInitialized = false;

// Forward declaration
void AudioInputCallback(void *buffer, unsigned int frames);

void InitAudioPlayerSystem(void) {
    if (isSystemInitialized) return;
    
    // Assumes InitAudioDevice() is called by main
    stream = LoadAudioStream(SAMPLE_RATE, 32, CHANNELS);
    SetAudioStreamCallback(stream, AudioInputCallback);
    
    timeStretcher = sonicCreateStream(SAMPLE_RATE, CHANNELS);
    sonicSetSpeed(timeStretcher, currentSpeed);
    
    PlayAudioStream(stream); // Start processing callback
    
    isSystemInitialized = true;
}

void CloseAudioPlayerSystem(void) {
    if (!isSystemInitialized) return;
    
    UnloadAudioStream(stream);
    if (timeStretcher) sonicDestroyStream(timeStretcher);
    if (localAudioBuffer.samples) FreeAudioBuffer(localAudioBuffer);
    
    isSystemInitialized = false;
}

bool LoadTrack(const char *fileName) {
    // Unload previous
    if (localAudioBuffer.samples) {
        FreeAudioBuffer(localAudioBuffer);
        localAudioBuffer.samples = NULL;
    }
    
    localAudioBuffer = LoadAudioDataFFmpeg(fileName);
    if (!localAudioBuffer.samples) return false;
    
    currentAudio = &localAudioBuffer;
    currentFrameIndex = 0;
    isPlaying = false;
    
    // Reset sonic
    if (timeStretcher) {
        // Trick to clear buffer: change sample rate then change back, or just recreate
        // explicit clear function is missing, but comments say SetSampleRate drops samples
        sonicSetSampleRate(timeStretcher, SAMPLE_RATE); 
    }
    
    return true;
}

void UpdatePlayer(void) {
    // Needed if not using callback, but we are using callback.
    // However, Raylib requires UpdateAudioStream(stream) if not using callback?
    // "Since Raylib 2.5, UpdateAudioStream() is not required if using SetAudioStreamCallback()"
    // But we might want to check for end of stream state here.
}

// Callback running on audio thread (careful with mutexes if needed, but here simple flags might suffice)
void AudioInputCallback(void *buffer, unsigned int frames) {
    float *output = (float *)buffer;
    
    if (!isPlaying || !currentAudio || !currentAudio->samples) {
        memset(output, 0, frames * CHANNELS * sizeof(float));
        return;
    }
    
    int samplesNeeded = frames; 
    // sonicReadFloatFromStream needs maxSamples (frames)
    
    // Feed sonic until we have enough data
    while (sonicSamplesAvailable(timeStretcher) < samplesNeeded) {
        // Feed chunk
        if (currentFrameIndex >= currentAudio->frameCount) {
            // End of file
            // Flush leftover
            // If flushed and still not enough, fill rest with 0
            break; 
        }
        
        // Determine how much to write
        // We want to write enough to generate output.
        // If speed is 2.0, we need 2x input.
        // Let's write a reasonably large chunk, e.g. 1024 frames
        int chunkFrames = 1024;
        if (currentFrameIndex + chunkFrames > currentAudio->frameCount) {
            chunkFrames = currentAudio->frameCount - currentFrameIndex;
        }
        
        sonicWriteFloatToStream(timeStretcher, 
                                currentAudio->samples + (currentFrameIndex * CHANNELS), 
                                chunkFrames);
        
        currentFrameIndex += chunkFrames;
    }
    
    int readFrames = sonicReadFloatFromStream(timeStretcher, output, samplesNeeded);
    
    if (readFrames < samplesNeeded) {
        // Not enough data even after feeding (EOF reached)
        // Fill remainder with silence
        memset(output + (readFrames * CHANNELS), 0, (samplesNeeded - readFrames) * CHANNELS * sizeof(float));
        
        // Auto-pause at end? Or loop?
        // simple auto-pause Logic would require communicating back to main thread or setting flag
        if (currentFrameIndex >= currentAudio->frameCount && sonicSamplesAvailable(timeStretcher) == 0) {
           // We finished
           // Keep isPlaying true to allow seeking? Or set false?
           // Usually set false.
           isPlaying = false;
           currentFrameIndex = 0; // Reset for replay? Or stay at end. Stay at end is better.
        }
    }
}

void PlayTrack(void) {
    if (currentAudio && currentAudio->samples) {
        isPlaying = true;
    }
}

void PauseTrack(void) {
    isPlaying = false;
}

void StopTrack(void) {
    isPlaying = false;
    SeekTrack(0.0f);
}

bool IsTrackPlaying(void) {
    return isPlaying;
}

float GetTrackTime(void) {
    if (!currentAudio) return 0.0f;
    // Current time is roughly currentFrameIndex / SampleRate
    // But this logic is slightly off because of buffering in Sonic and Raylib.
    // Ideally we track what has been *played*, not just fed.
    // But for a simple clipper, this approximation is okay.
    return (float)currentFrameIndex / SAMPLE_RATE;
}

float GetTrackDuration(void) {
    if (!currentAudio) return 0.0f;
    return (float)currentAudio->frameCount / SAMPLE_RATE;
}

void SeekTrack(float time) {
    if (!currentAudio) return;
    
    int frame = (int)(time * SAMPLE_RATE);
    if (frame < 0) frame = 0;
    if (frame > (int)currentAudio->frameCount) frame = currentAudio->frameCount;
    
    // Critical: clear sonic buffer to avoid playing old data
    sonicSetSampleRate(timeStretcher, SAMPLE_RATE);
    
    currentFrameIndex = frame;
}

void SetTrackSpeed(float speed) {
    if (speed < 0.5f) speed = 0.5f;
    if (speed > 3.0f) speed = 3.0f;
    currentSpeed = speed;
    if (timeStretcher) {
        sonicSetSpeed(timeStretcher, speed);
    }
}

float GetTrackSpeed(void) {
    return currentSpeed;
}

AudioBuffer *GetTrackBuffer(void) {
    return currentAudio;
}
