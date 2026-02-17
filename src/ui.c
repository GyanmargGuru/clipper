#include "ui.h"
#include "raylib.h"
#include "audio_player.h"
#include "tinyfiledialogs.h" // vendor
#include <stdio.h>
#include <math.h>
#include <stdlib.h> // for system
#include <string.h> // for snprintf

// UI Configuration
#define WAVEFORM_HEIGHT 300
#define CONTROL_PANEL_HEIGHT 150
#define MARGIN 50
#define BUTTON_HEIGHT 30
#define BUTTON_WIDTH 100
#define UI_TEXT_SIZE 20
#define BUTTON_TEXT_SIZE 16

// State
static float zoomLevel = 1.0f; // Scale factor
static float panOffset = 0.0f; // Time offset in seconds
static float selectionStart = -1.0f;
static float selectionEnd = -1.0f;
static bool isDraggingStart = false; 
static char saveFilename[256] = "clip-01.wav";

// Clipped regions tracking
#define MAX_CLIPPED_REGIONS 256
typedef struct { float start; float end; } ClipRegion;
static ClipRegion clippedRegions[MAX_CLIPPED_REGIONS];
static int clippedRegionCount = 0;

// Markers
#define MAX_MARKERS 64
typedef struct {
    float time;    // position in seconds
} Marker;
static Marker markers[MAX_MARKERS];
static int markerCount = 0;

// Context Menu
#define CONTEXT_MENU_WIDTH 200
#define CONTEXT_MENU_ITEM_HEIGHT 28
#define CONTEXT_MENU_ITEMS 3
static bool contextMenuVisible = false;
static Vector2 contextMenuPos = {0, 0};
static float contextMenuTime = 0.0f;  // time at right-click position

// Custom Font
static Font uiFont = {0};
static bool fontLoaded = false;

// Helper: draw text with custom font (falls back to default if not loaded)
static void UIDrawText(const char *text, int posX, int posY, int fontSize, Color color) {
    if (fontLoaded) {
        DrawTextEx(uiFont, text, (Vector2){(float)posX, (float)posY}, (float)fontSize, 1.0f, color);
    } else {
        DrawText(text, posX, posY, fontSize, color);
    }
}

// Helper: measure text width with custom font
static int UIMeasureText(const char *text, int fontSize) {
    if (fontLoaded) {
        Vector2 size = MeasureTextEx(uiFont, text, (float)fontSize, 1.0f);
        return (int)size.x;
    }
    return MeasureText(text, fontSize);
}

static bool IsTimeClipped(float t) {
    for (int i = 0; i < clippedRegionCount; i++) {
        if (t >= clippedRegions[i].start && t <= clippedRegions[i].end) return true;
    }
    return false;
}

// Forward Declarations
static bool GuiButton(Rectangle bounds, const char *text);
static float GuiSlider(Rectangle bounds, const char *textLeft, const char *textRight, float value, float min, float max);

void InitUI(void) {
    // Load custom font
    uiFont = LoadFontEx("assets/JetBrainsMono-Regular.ttf", 32, NULL, 0);
    if (uiFont.glyphCount > 0) {
        fontLoaded = true;
        SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
    }
}

// UI layout constants
#define SIDE_MARGIN 50

static Rectangle GetWaveformBounds(void) {
    return (Rectangle){ 
        SIDE_MARGIN, 
        0, 
        GetScreenWidth() - 2 * SIDE_MARGIN, 
        GetScreenHeight() - CONTROL_PANEL_HEIGHT 
    };
}

// Helper to get X position for time
static int GetXForTime(float time) {
    Rectangle bounds = GetWaveformBounds();
    return (int)((time - panOffset) * zoomLevel * 100.0f) + bounds.x;
}

// Helper to get time from X position
static float GetTimeForX(int x) {
    Rectangle bounds = GetWaveformBounds();
    return ((x - bounds.x) / (zoomLevel * 100.0f)) + panOffset;
}

static bool IsTrackLoaded(void) {
    return GetTrackDuration() > 0;
}

void UpdateUI(void) {
    float duration = GetTrackDuration();
    if (duration <= 0) duration = 1.0f; 
    
    Rectangle bounds = GetWaveformBounds();

    // Calculate min zoom to fit entire duration in bounds
    float minZoom = (float)bounds.width / (duration * 100.0f);
    if (minZoom > 1.0f) minZoom = 1.0f;
    if (minZoom < 0.001f) minZoom = 0.001f;

    // Zoom with scroll
    if (!IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            float mouseTime = GetTimeForX(GetMouseX());
            // Clamp mouse logic time to valid range for zooming
            // (Just to ensure we zoom into somewhere sensible)
            
            // float oldZoom = zoomLevel;
            zoomLevel *= (1.0f + wheel * 0.1f);
            
            if (zoomLevel < minZoom) zoomLevel = minZoom;
            if (zoomLevel > 100.0f) zoomLevel = 100.0f;
            
            // Adjust pan
            // mouseX = (time - pan) * zoom + x
            // pan = time - (mouseX - x) / zoom
            panOffset = mouseTime - (GetMouseX() - bounds.x) / (zoomLevel * 100.0f);
        }
    }
    
    // Pan (middle-click only)
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        panOffset -= delta.x / (zoomLevel * 100.0f);
    }

    // Context Menu: dismiss on left-click outside menu
    if (contextMenuVisible && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Rectangle menuRect = { contextMenuPos.x, contextMenuPos.y,
                               CONTEXT_MENU_WIDTH, CONTEXT_MENU_ITEM_HEIGHT * CONTEXT_MENU_ITEMS };
        Vector2 mouse = GetMousePosition();
        if (!CheckCollisionPointRec(mouse, menuRect)) {
            contextMenuVisible = false;
        }
    }

    // Context Menu: handle item clicks
    if (contextMenuVisible && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < CONTEXT_MENU_ITEMS; i++) {
            Rectangle itemRect = { contextMenuPos.x, contextMenuPos.y + i * CONTEXT_MENU_ITEM_HEIGHT,
                                   CONTEXT_MENU_WIDTH, CONTEXT_MENU_ITEM_HEIGHT };
            if (CheckCollisionPointRec(mouse, itemRect)) {
                if (i == 0) {
                    // Add Marker
                    if (markerCount < MAX_MARKERS) {
                        markers[markerCount].time = contextMenuTime;
                        markerCount++;
                    }
                } else if (i == 1) {
                    // Remove Marker (nearest within 1 second)
                    int nearest = -1;
                    float nearestDist = 1.0f;
                    for (int m = 0; m < markerCount; m++) {
                        float dist = fabsf(markers[m].time - contextMenuTime);
                        if (dist < nearestDist) {
                            nearestDist = dist;
                            nearest = m;
                        }
                    }
                    if (nearest >= 0) {
                        for (int m = nearest; m < markerCount - 1; m++) {
                            markers[m] = markers[m + 1];
                        }
                        markerCount--;
                    }
                } else if (i == 2) {
                    // Select Between Markers
                    float leftMarker = -1.0f;
                    float rightMarker = -1.0f;
                    for (int m = 0; m < markerCount; m++) {
                        float mt = markers[m].time;
                        if (mt <= contextMenuTime) {
                            if (leftMarker < 0 || mt > leftMarker) leftMarker = mt;
                        }
                        if (mt >= contextMenuTime) {
                            if (rightMarker < 0 || mt < rightMarker) rightMarker = mt;
                        }
                    }
                    if (leftMarker >= 0 && rightMarker >= 0 && leftMarker < rightMarker) {
                        selectionStart = leftMarker;
                        selectionEnd = rightMarker;
                    }
                }
                contextMenuVisible = false;
                break;
            }
        }
    }

    // Context Menu: open on right-click in waveform area
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (GetMouseY() < bounds.height && IsTrackLoaded()) {
            float clickTime = GetTimeForX(GetMouseX());
            if (clickTime >= 0 && clickTime <= duration) {
                contextMenuVisible = true;
                contextMenuPos = GetMousePosition();
                contextMenuTime = clickTime;
            }
        }
    }
    
    // Clamp panOffset: at minimum zoom (full view), snap to 0
    float visibleDuration = bounds.width / (zoomLevel * 100.0f);
    if (visibleDuration >= duration) {
        panOffset = 0.0f;
    } else {
        // Allow small negative pan but not too much
        if (panOffset < 0.0f) panOffset = 0.0f;
        // Don't pan past end of audio
        if (panOffset > duration - visibleDuration) panOffset = duration - visibleDuration;
    }
    
    // Auto-scroll when playhead goes past visible range during playback
    if (IsTrackPlaying()) {
        float playTime = GetTrackTime();
        float viewEnd = panOffset + visibleDuration;
        if (playTime > viewEnd) {
            // Jump forward so playhead is at the left edge
            panOffset = playTime;
            // Re-clamp
            if (panOffset > duration - visibleDuration) panOffset = duration - visibleDuration;
            if (panOffset < 0.0f) panOffset = 0.0f;
        }
    }
    
    // Selection Logic
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Only if clicking in waveform area (y check)
        // And x check?
        if (GetMouseY() < bounds.height) {
             // Allow clicking in margins to seek to start/end of view?
             // Or just clamp.
            float clickTime = GetTimeForX(GetMouseX());
            if (clickTime < 0) clickTime = 0;
            if (clickTime > duration) clickTime = duration;

            SeekTrack(clickTime);
            selectionStart = clickTime;
            selectionEnd = clickTime;
            isDraggingStart = true;
        }
    }
    
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && isDraggingStart) {
        if (GetMouseY() < bounds.height) {
            float time = GetTimeForX(GetMouseX());
            if (time < 0) time = 0;
            if (time > duration) time = duration;
            selectionEnd = time;
        }
    }
    
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (isDraggingStart) {
            isDraggingStart = false;
            // Normalize
            if (selectionStart > selectionEnd) {
                float temp = selectionStart;
                selectionStart = selectionEnd;
                selectionEnd = temp;
            }
            if (fabs(selectionStart - selectionEnd) < 0.05f) {
                selectionStart = -1.0f; 
                selectionEnd = -1.0f;
            }
        }
    }
    
    // Controls Shortcuts
    if (IsKeyPressed(KEY_SPACE)) {
        if (IsTrackPlaying()) PauseTrack();
        else PlayTrack();
    }
    
    // Open File
    if (IsFileDropped()) {
        FilePathList droppedFiles = LoadDroppedFiles();
        if (droppedFiles.count > 0) {
            if (LoadTrack(droppedFiles.paths[0])) {
                panOffset = 0;
                clippedRegionCount = 0;  // Reset clip history
                float newDur = GetTrackDuration();
                zoomLevel = (float)bounds.width / (newDur * 100.0f);
                if (zoomLevel > 1.0f) zoomLevel = 1.0f;
                SetWindowTitle(TextFormat("Audio Clipper - %s", GetFileName(droppedFiles.paths[0])));
                sprintf(saveFilename, "%s-clip-01.wav", droppedFiles.paths[0]);
            }
        }
        UnloadDroppedFiles(droppedFiles);
    }
}

static void DrawWaveform(void) {
    Rectangle bounds = GetWaveformBounds();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.width;
    int h = bounds.height;
    // printf("x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);

    AudioBuffer *buffer = GetTrackBuffer();
    if (!buffer || !buffer->samples) {
        UIDrawText("No Audio Loaded. Drag & Drop a file here.", x + w/2 - 150, y + h/2 - 10, 20, RAYWHITE);
        return;
    }
    
    // Background
    DrawRectangle(x, y, w, h, GetColor(0x121212FF));
    DrawRectangleLines(x, y, w, h, DARKGRAY);
    
    int centerY = y + h/2;
    
    // Draw Grid (Time)
    float startTime = panOffset;
    float endTime = GetTimeForX(x + w);
    
    // Determine grid step
    float timeSpan = endTime - startTime;
    float timeStep = 1.0f;
    if (timeSpan < 1.0f) timeStep = 0.1f;
    else if (timeSpan < 10.0f) timeStep = 1.0f;
    else if (timeSpan < 60.0f) timeStep = 5.0f;
    else if (timeSpan < 600.0f) timeStep = 60.0f;
    else timeStep = 120.0f;
    
    // Align start
    float t = ceilf(startTime / timeStep) * timeStep;
    while (t < endTime) {
        int gx = GetXForTime(t);
        if (gx >= x && gx < x + w) {
            DrawLine(gx, y, gx, y + h, GetColor(0x333333FF));
            UIDrawText(TextFormat("%.1f", t), gx + 2, y + h - 15, 10, GRAY);
        }
        t += timeStep;
    }
    
    // Draw Grid (Amplitude)
    // -1, -0.5, 0, 0.5, 1
    // 0 is centerY.
    // Scale is h/2 * 0.8 (gain from before)
    float gain = 0.8f;
    float ampStep = 0.5f;
    for (float a = -1.0f; a <= 1.0f; a += ampStep) {
        int gy = centerY - (int)(a * h/2 * gain);
        Color gCol = (a == 0.0f) ? DARKGRAY : GetColor(0x222222FF);
        DrawLine(x, gy, x + w, gy, gCol);
        if (a != 0) UIDrawText(TextFormat("%.1f", a), x + 2, gy - 10, 10, GRAY);
    }

    // Prepare Waveform Draw
    // Use zoomLevel to determine samples per pixel for consistent scaling
    float pixelsPerSec = zoomLevel * 100.0f;
    if (pixelsPerSec < 1.0f) pixelsPerSec = 1.0f;
    float samplesPerPixel = (float)buffer->sampleRate / pixelsPerSec;
    
    int step = (int)samplesPerPixel;
    if (step < 1) step = 1;

    // Draw loop - use per-pixel time to stay consistent with grid
    for (int i = 0; i < w; i++) {
        // Calculate the time this pixel represents (same formula as grid)
        float pixelTime = startTime + (float)i / pixelsPerSec;
        if (pixelTime < 0.0f) continue;  // Before audio start
        
        long long currentS = (long long)(pixelTime * buffer->sampleRate);
        if (currentS >= buffer->frameCount) break;

        float minVal = 0.0f;
        float maxVal = 0.0f;
        int checkCount = (step > 100) ? 100 : step;
        
        if (checkCount <= 1) {
            // High zoom, single sample
             minVal = buffer->samples[currentS * buffer->channels];
             maxVal = minVal;
        } else {
            for (int k = 0; k < checkCount; k++) {
                if (currentS + k >= buffer->frameCount) break;
                float s = buffer->samples[(currentS + k) * buffer->channels];
                if (s < minVal) minVal = s;
                if (s > maxVal) maxVal = s;
            }
        }
        
        int yMin = centerY - (int)(minVal * h/2 * gain);
        int yMax = centerY - (int)(maxVal * h/2 * gain);
        
        if (yMax > yMin) { int tmp = yMax; yMax = yMin; yMin = tmp; } // Swap if inverted
        if (yMax == yMin) { yMin += 1; }
        
        Color waveColor = IsTimeClipped(pixelTime) ? GRAY : GREEN;
        DrawRectangle(x + i, yMax, 1, yMin - yMax, waveColor);
    }
    
    // Draw Selection Overlay
    if (selectionStart >= 0 && selectionEnd >= 0) {
        int x1 = GetXForTime(selectionStart);
        int x2 = GetXForTime(selectionEnd);
        
        // Clamp to view
        if (x1 < x) x1 = x;
        if (x2 > x + w) x2 = x + w;
        
        if (x2 > x1) {
            DrawRectangle(x1, y, x2 - x1, h, Fade(BLUE, 0.3f));
            DrawRectangleLines(x1, y, x2 - x1, h, BLUE);
        }
    }
    
    // Draw Playhead
    float playTime = GetTrackTime();
    int playX = GetXForTime(playTime);
    
    // Check strict bounds for playhead
    if (playX >= x && playX < x + w) {
        DrawLine(playX, y, playX, y + h, RED);
        DrawLine(playX+1, y, playX+1, y + h, RED);
    }

    // Draw Markers
    for (int i = 0; i < markerCount; i++) {
        int mx = GetXForTime(markers[i].time);
        if (mx >= x && mx < x + w) {
            DrawLine(mx, y, mx, y + h, YELLOW);
            DrawLine(mx + 1, y, mx + 1, y + h, YELLOW);
            // Small triangle at top
            DrawTriangle(
                (Vector2){(float)mx - 5, (float)y},
                (Vector2){(float)mx + 5, (float)y},
                (Vector2){(float)mx, (float)y + 8},
                YELLOW);
            UIDrawText(TextFormat("M%d", i + 1), mx + 4, y + 2, 10, YELLOW);
        }
    }
}

void DrawUI(void) {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    
    Color PANEL_COLOR = GetColor(0x202020FF); 
    Color TEXT_COLOR = RAYWHITE;
    
    DrawWaveform(); // Now self-contained with bounds
    
    // Controls
    int y = screenHeight - CONTROL_PANEL_HEIGHT;
    DrawRectangle(0, y, screenWidth, CONTROL_PANEL_HEIGHT, PANEL_COLOR);
    
    // Buttons
    float button_y_offset = (float)(y + MARGIN - 10);

    if (GuiButton((Rectangle){2*MARGIN + BUTTON_WIDTH, button_y_offset, BUTTON_WIDTH, BUTTON_HEIGHT}, IsTrackPlaying() ? "PAUSE" : "PLAY")) {
        if (IsTrackPlaying()) PauseTrack(); else PlayTrack();
    }
    
    if (GuiButton((Rectangle){3*MARGIN + 2*BUTTON_WIDTH, button_y_offset, BUTTON_WIDTH, BUTTON_HEIGHT}, "STOP")) {
        StopTrack();
    }    

    if (GuiButton((Rectangle){4*MARGIN + 3*BUTTON_WIDTH, button_y_offset, BUTTON_WIDTH, BUTTON_HEIGHT}, "ZOOM ALL")) {
        Rectangle bounds = GetWaveformBounds();
        float duration = GetTrackDuration();
        float minZoom = (float)bounds.width / (duration * 100.0f);
        if (minZoom > 1.0f) minZoom = 1.0f;
        if (minZoom < 0.001f) minZoom = 0.001f;
        zoomLevel = minZoom;
        panOffset = 0.0f;
    }
    
    if (GuiButton((Rectangle){MARGIN, button_y_offset, BUTTON_WIDTH, BUTTON_HEIGHT}, "LOAD FILE")) {
        // DrawText("Loading...", screenWidth/2 - 150, screenHeight/2 - 10, 20, RAYWHITE);

         const char *filters[] = {"*.wav", "*.mp3", "*.ogg", "*.mp4"};
         const char *file = tinyfd_openFileDialog("Open Audio", "", 4, filters, "Audio Files", 0);
         if (file) {
             if (LoadTrack(file)) {
                 panOffset = 0;
                 clippedRegionCount = 0;  // Reset clip history
                 float newDur = GetTrackDuration();
                 Rectangle wb = GetWaveformBounds();
                 zoomLevel = (float)wb.width / (newDur * 100.0f);
                 if (zoomLevel > 1.0f) zoomLevel = 1.0f;
                 SetWindowTitle(TextFormat("Audio Clipper - %s", GetFileName(file)));
                 sprintf(saveFilename, "%s-clip-01.wav", file);
             }
         }
    }
    
    int y_offset = 100; //for info text

    // Save Clip
    if (selectionStart >= 0 && selectionEnd >= 0 && IsTrackLoaded()) {
        // Save Button aligned right
        if (GuiButton((Rectangle){(float)screenWidth - BUTTON_WIDTH - MARGIN, button_y_offset, BUTTON_WIDTH, BUTTON_HEIGHT}, "SAVE CLIP")) {
             const char *filters[] = {"*.wav"};
             const char *file = tinyfd_saveFileDialog("Save Clip", saveFilename, 1, filters, "WAV Files");
             if (file) {
                 char cmd[4096];
                 snprintf(cmd, sizeof(cmd), "ffmpeg -v error -f f32le -ar 44100 -ac 2 -i pipe:0 -y \"%s\"", file);
                 FILE *fp = popen(cmd, "w");
                 if (fp) {
                     AudioBuffer *ab = GetTrackBuffer();
                     if (ab) {
                         long long startSamp = (long long)(selectionStart * ab->sampleRate);
                         long long endSamp = (long long)(selectionEnd * ab->sampleRate);
                         if (startSamp < 0) startSamp = 0;
                         if (endSamp > (long long)ab->frameCount) endSamp = ab->frameCount;
                         
                         long long framesToWrite = endSamp - startSamp;
                         if (framesToWrite > 0) {
                             fwrite(ab->samples + (startSamp * ab->channels), sizeof(float) * ab->channels, framesToWrite, fp);
                         }
                     }
                     pclose(fp);
                 }
                 // Remember clipped region
                 if (clippedRegionCount < MAX_CLIPPED_REGIONS) {
                     clippedRegions[clippedRegionCount].start = selectionStart;
                     clippedRegions[clippedRegionCount].end = selectionEnd;
                     clippedRegionCount++;
                 }
                 selectionStart = -1; selectionEnd = -1;
             }
        }
        // Selection Text
        const char *text = TextFormat("Selected: %.2fs - %.2fs (%.2fs)", 8888.0f, 8888.0f, 8888.0f);
        int textW = UIMeasureText(text, UI_TEXT_SIZE);
        int text_sel_x = screenWidth - textW - MARGIN;
        UIDrawText(TextFormat("Selected: %07.2fs", selectionStart), text_sel_x, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);
        UIDrawText(TextFormat(" - %07.2fs", selectionEnd), text_sel_x + 185, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);
        UIDrawText(TextFormat(" (%07.2fs)", selectionEnd - selectionStart), text_sel_x + 300, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);
    }
    
    // Speed Slider
    float speed = GetTrackSpeed();
    UIDrawText("Speed:", MARGIN, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);
    float newSpeed = GuiSlider((Rectangle){MARGIN + 70, (float)y + y_offset, 200, 20}, "0.5x", "3.0x", speed, 0.5f, 3.0f);
    if (newSpeed != speed) SetTrackSpeed(newSpeed);
    UIDrawText(TextFormat("%.2gx", speed), MARGIN + 280, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);
    
    // Zoom Level Display
    float duration = GetTrackDuration();
    if (duration <= 0) duration = 1.0f;
    Rectangle wb2 = GetWaveformBounds();
    float minZoom = (float)wb2.width / (duration * 100.0f);
    if (minZoom > 1.0f) minZoom = 1.0f;
    if (minZoom < 0.001f) minZoom = 0.001f;
    
    float zoomPct = 0.0f;
    if (zoomLevel >= 100.0f) zoomPct = 100.0f;
    else if (zoomLevel <= minZoom) zoomPct = 0.0f;
    else {
        zoomPct = (zoomLevel - minZoom) / (100.0f - minZoom) * 100.0f;
    }
    
    UIDrawText(TextFormat("Zoom: %.2f%%", zoomPct), MARGIN + 620, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);
    
    // Time Display
    UIDrawText(TextFormat("Time: %07.2f ", GetTrackTime()), MARGIN + 350, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);
    UIDrawText(TextFormat(" / %07.2f", GetTrackDuration()), MARGIN + 480, y + y_offset, UI_TEXT_SIZE, TEXT_COLOR);

    // Draw Context Menu (last, so it renders on top)
    if (contextMenuVisible) {
        float mx = contextMenuPos.x;
        float my = contextMenuPos.y;
        float mw = CONTEXT_MENU_WIDTH;
        float totalH = CONTEXT_MENU_ITEM_HEIGHT * CONTEXT_MENU_ITEMS;

        // Clamp to screen
        if (mx + mw > screenWidth) mx = screenWidth - mw;
        if (my + totalH > screenHeight) my = screenHeight - totalH;
        // Update stored position so click detection matches
        contextMenuPos.x = mx;
        contextMenuPos.y = my;

        // Shadow
        DrawRectangle((int)mx + 3, (int)my + 3, (int)mw, (int)totalH, Fade(BLACK, 0.4f));
        // Background
        DrawRectangle((int)mx, (int)my, (int)mw, (int)totalH, GetColor(0x2A2A2AFF));
        DrawRectangleLinesEx((Rectangle){mx, my, mw, totalH}, 1, GetColor(0x666666FF));

        const char *labels[CONTEXT_MENU_ITEMS] = {
            "  Add Marker",
            "  Remove Marker",
            "  Select Between Markers"
        };

        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < CONTEXT_MENU_ITEMS; i++) {
            Rectangle itemRect = { mx, my + i * CONTEXT_MENU_ITEM_HEIGHT, mw, CONTEXT_MENU_ITEM_HEIGHT };
            bool hovering = CheckCollisionPointRec(mouse, itemRect);
            if (hovering) {
                DrawRectangleRec(itemRect, GetColor(0x0078D7FF));
            }
            // Separator line
            if (i > 0) {
                DrawLine((int)mx + 4, (int)(my + i * CONTEXT_MENU_ITEM_HEIGHT),
                         (int)(mx + mw - 4), (int)(my + i * CONTEXT_MENU_ITEM_HEIGHT),
                         GetColor(0x444444FF));
            }
            UIDrawText(labels[i], (int)mx + 4, (int)(my + i * CONTEXT_MENU_ITEM_HEIGHT + 8), BUTTON_TEXT_SIZE, RAYWHITE);
        }
    }
}

// Accessors
float GetSelectionStart(void) { return selectionStart; }
float GetSelectionEnd(void) { return selectionEnd; }

// Implementation of helpers
static bool GuiButton(Rectangle bounds, const char *text) {
    Vector2 mouse = GetMousePosition();
    bool hovering = CheckCollisionPointRec(mouse, bounds);
    bool clicked = false;
    
    Color normalColor = GetColor(0x404040FF);
    Color hoverColor = GetColor(0x606060FF);
    Color borderColor = GetColor(0x808080FF);
    
    if (hovering) {
        DrawRectangleRec(bounds, hoverColor);
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = true;
    } else {
        DrawRectangleRec(bounds, normalColor);
    }
    DrawRectangleLinesEx(bounds, 1, borderColor);
    
    int textW = UIMeasureText(text, BUTTON_TEXT_SIZE);
    UIDrawText(text, bounds.x + bounds.width/2 - textW/2, bounds.y + bounds.height/2 - 5, BUTTON_TEXT_SIZE, RAYWHITE);
    return clicked;
}

static float GuiSlider(Rectangle bounds, const char *textLeft, const char *textRight, float value, float min, float max) {
    (void)textLeft; (void)textRight; // Unused for now
    
    Color barColor = GetColor(0x404040FF);
    Color borderColor = GetColor(0x808080FF);
    Color knobColor = GetColor(0x0078D7FF); // Blue-ish
    
    // Draw Bar
    DrawRectangleRec(bounds, barColor);
    DrawRectangleLinesEx(bounds, 1, borderColor);
    
    // Draw Knob
    float ratio = (value - min) / (max - min);
    float knobX = bounds.x + ratio * bounds.width;
    // DrawRectangle(knobX - 5, bounds.y - 2, 10, bounds.height + 4, knobColor);
    DrawRectangle(knobX - 5, bounds.y - 2, 10, bounds.height + 4, knobColor);
    DrawRectangleLinesEx((Rectangle){knobX - 5, bounds.y - 2, 10, bounds.height + 4}, 1, borderColor);
    
    // Interaction
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, (Rectangle){bounds.x - 10, bounds.y - 10, bounds.width + 20, bounds.height + 20})) {
            float newRatio = (mouse.x - bounds.x) / bounds.width;
            if (newRatio < 0) newRatio = 0;
            if (newRatio > 1) newRatio = 1;
            value = min + newRatio * (max - min);
        }
    }
    
    return value;
}
