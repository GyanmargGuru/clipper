#include "audio_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AudioBuffer LoadAudioDataFFmpeg(const char *fileName, void (*progressCallback)(float)) {
    AudioBuffer buffer = {0};
    
    // First, let's get the duration of the file using ffprobe
    char probeCmd[2048];
    snprintf(probeCmd, sizeof(probeCmd), "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"", fileName);
    FILE *probeFp = popen(probeCmd, "r");
    double duration = -1.0;
    if (probeFp) {
        if (fscanf(probeFp, "%lf", &duration) != 1) {
            duration = -1.0;
        }
        pclose(probeFp);
    }
    
    // Calculate expected total samples (2 channels, 44100 Hz)
    size_t expectedTotalSamples = 0;
    if (duration > 0) {
        expectedTotalSamples = (size_t)(duration * 44100 * 2);
    }

    // Construct command
    // "ffmpeg -v error -i \"fileName\" -f f32le -ac 2 -ar 44100 pipe:1"
    
    char command[2048];
    // Check filename length to avoid overflow
    if (strlen(fileName) > 1024) {
        printf("Error: Filename too long\n");
        return buffer;
    }
    
    // We quote the filename to handle spaces. 
    // Basic escaping: replace " with \" ? 
    // For now assuming simple paths. A robust solution needs better escaping.
    snprintf(command, sizeof(command), "ffmpeg -v error -i \"%s\" -f f32le -ac 2 -ar 44100 pipe:1", fileName);
    
    printf("Executing: %s\n", command);

    FILE *fp = popen(command, "r");
    if (!fp) {
        printf("Error: Could not open ffmpeg pipe\n");
        return buffer;
    }
    
    // Read data
    // Start with 1MB capacity
    size_t capacity = 1024 * 1024; 
    size_t count = 0;
    float *data = (float *)malloc(capacity * sizeof(float));
    
    if (!data) {
        printf("Error: Memory allocation failed\n");
        pclose(fp);
        return buffer;
    }
    
    size_t samplesRead;
    float tempBuffer[4096]; // Read in chunks of floats
    size_t lastCallbackSamples = 0;
    
    while ((samplesRead = fread(tempBuffer, sizeof(float), 4096, fp)) > 0) {
        if (count + samplesRead > capacity) {
            capacity *= 2;
            float *newData = (float *)realloc(data, capacity * sizeof(float));
            if (!newData) {
                printf("Error: Memory reallocation failed\n");
                free(data);
                pclose(fp);
                return buffer;
            }
            data = newData;
        }
        memcpy(data + count, tempBuffer, samplesRead * sizeof(float));
        count += samplesRead;
        
        // Only call callback approx every 44100*2 floats (1 second of audio) to save UI thrashing
        if (progressCallback && expectedTotalSamples > 0 && (count - lastCallbackSamples >= 88200)) {
            float pct = (float)count / (float)expectedTotalSamples;
            if (pct > 1.0f) pct = 1.0f;
            progressCallback(pct);
            lastCallbackSamples = count;
        }
    }
    
    int exitCode = pclose(fp);
    if (exitCode != 0) {
        printf("Warning: FFmpeg exited with code %d. File might be incomplete or invalid.\n", exitCode);
    }
    
    if (count == 0) {
        printf("Error: No data read from ffmpeg.\n");
        free(data);
        return buffer;
    }

    buffer.samples = data;
    buffer.channels = 2; // Forced by ffmpeg command
    buffer.sampleRate = 44100; // Forced by ffmpeg command
    buffer.sampleCount = count;
    buffer.frameCount = count / 2;
    
    printf("Loaded Audio: %u frames, %u samples\n", buffer.frameCount, buffer.sampleCount);
    
    return buffer;
}

void FreeAudioBuffer(AudioBuffer buffer) {
    if (buffer.samples) {
        free(buffer.samples);
    }
}
