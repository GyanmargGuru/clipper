#ifndef AUDIO_LOADER_H
#define AUDIO_LOADER_H

typedef struct {
    float *samples;
    unsigned int sampleCount; // Total float count (channels * frames)
    unsigned int frameCount;  // Total frames
    int sampleRate;
    int channels;
} AudioBuffer;

// Loads audio data using ffmpeg pipe. Resamples to 44100Hz Stereo Float32.
AudioBuffer LoadAudioDataFFmpeg(const char *fileName);
void FreeAudioBuffer(AudioBuffer buffer);

#endif
