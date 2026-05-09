#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <raylib.h>
#include "../include/gui.h"
#include "../include/ipc.h"
#include "../include/generator.h"


// screen dimensions - should match with PNG sizes
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT  800


// GUI states
typedef enum {
    GUI_INTRO,       // Intro/splash screen
    GUI_CONFIG,      // Driver count selection screen
    GUI_DASHBOARD,   // Live simulation dashboard
    GUI_METRICS      // Post-simulation metrics summary
} GuiScreen;


// Color Palette

static const Color BG_COLOR    = { 15,  15,  15,  255 };
static const Color PANEL_COLOR = { 28,  28,  30,  255 };
static const Color ACCENT_GOLD = { 255, 191,  0,  255 };
static const Color STATUS_OK   = {  0,  230, 118, 255 };
static const Color STATUS_BUSY = { 255,  61,   0, 255 };


// intro screen
// TODO: Place intro.png in request_server/assets/ and update this path.
#define INTRO_PNG_PATH   "request_server/assets/intro.png"

// TODO: Set the pixel rectangle (x, y, width, height) of the START button
//       as it appears on intro.png.
//       Example: the button is a 200x60 region starting at (540, 650)
static const Rectangle INTRO_BTN = { 540, 650, 200, 60 };   // <-- CHANGE ME


// config screen (driver count selection)
// TODO: Place the config screen PNG in request_server/assets/ and update path.
#define CONFIG_PNG_PATH  "request_server/assets/config.png"

// Number of driver-count options shown on the config screen
#define NUM_DRIVER_OPTIONS 4

// TODO: For each of the 4 option boxes, set:
//   - The hit-box rectangle (x, y, w, h) as it appears on config.png
//   - The driver count value that option represents
static const Rectangle DRIVER_OPTION_RECTS[NUM_DRIVER_OPTIONS] = {
    { 200, 350, 160, 80 },   // Option 0  <-- CHANGE ME
    { 420, 350, 160, 80 },   // Option 1  <-- CHANGE ME
    { 640, 350, 160, 80 },   // Option 2  <-- CHANGE ME
    { 860, 350, 160, 80 },   // Option 3  <-- CHANGE ME
};

static const int DRIVER_OPTION_VALUES[NUM_DRIVER_OPTIONS] = {
    5,    // Option 0  <-- CHANGE ME
    10,   // Option 1  <-- CHANGE ME
    15,   // Option 2  <-- CHANGE ME
    20,   // Option 3  <-- CHANGE ME
};

// end simulation button pixels
static const Rectangle END_BTN = { 40, 730, 240, 40 };      

// Draw a filled rectangle with a centred label - used as a fallback when a
// PNG is missing so the screen still shows something useful.
static void drawFallbackScreen(const char *title, const char *subtitle) {
    ClearBackground(BG_COLOR);
    int tw = MeasureText(title, 36);
    DrawText(title, SCREEN_WIDTH / 2 - tw / 2, SCREEN_HEIGHT / 2 - 60, 36, ACCENT_GOLD);
    int sw = MeasureText(subtitle, 18);
    DrawText(subtitle, SCREEN_WIDTH / 2 - sw / 2, SCREEN_HEIGHT / 2, 18, LIGHTGRAY);
}

// draw a highlight box around a rectangle (for debug / hover feedback).
static void drawHitboxOutline(Rectangle r, Color c) {
    DrawRectangleLinesEx(r, 2, c);
}

static void drawIntroScreen(Texture2D tex, bool texLoaded) {
    if (texLoaded) {
        DrawTexture(tex, 0, 0, WHITE);
    } else {
        drawFallbackScreen("RideOS", "Click the start button to begin");
        // Draw a visible placeholder button when PNG is missing
        DrawRectangleRounded(INTRO_BTN, 0.2f, 8, DARKBLUE);
        int tw = MeasureText("START", 22);
        DrawText("START",
                 (int)(INTRO_BTN.x + INTRO_BTN.width  / 2 - tw / 2),
                 (int)(INTRO_BTN.y + INTRO_BTN.height / 2 - 11),
                 22, RAYWHITE);
    }
    // Debug: show the hitbox outline so you can verify placement
    drawHitboxOutline(INTRO_BTN, GREEN);
}

static void drawConfigScreen(Texture2D tex, bool texLoaded) {
    if (texLoaded) {
        DrawTexture(tex, 0, 0, WHITE);
    } else {
        drawFallbackScreen("Select Number of Drivers", "Click one of the 4 options below");
        // Draw placeholder option boxes when PNG is missing
        for (int i = 0; i < NUM_DRIVER_OPTIONS; i++) {
            DrawRectangleRounded(DRIVER_OPTION_RECTS[i], 0.2f, 8, DARKBLUE);
            char label[16];
            snprintf(label, sizeof(label), "%d", DRIVER_OPTION_VALUES[i]);
            int tw = MeasureText(label, 24);
            DrawText(label,
                     (int)(DRIVER_OPTION_RECTS[i].x + DRIVER_OPTION_RECTS[i].width  / 2 - tw / 2),
                     (int)(DRIVER_OPTION_RECTS[i].y + DRIVER_OPTION_RECTS[i].height / 2 - 12),
                     24, RAYWHITE);
        }
    }
    // Debug: show hitbox outlines
    for (int i = 0; i < NUM_DRIVER_OPTIONS; i++) {
        drawHitboxOutline(DRIVER_OPTION_RECTS[i], GREEN);
    }
}

static void drawDashboard(const SharedState *state) {
    ClearBackground(BG_COLOR);

    // sidebar
    DrawRectangle(0, 0, 320, SCREEN_HEIGHT, (Color){ 22, 22, 24, 255 });
    DrawText("RideOS", 40, 40, 36, ACCENT_GOLD);
    DrawText("Autonomous Dispatch", 40, 80, 16, GRAY);

    // system health
    DrawText("SYSTEM STATUS", 40, 140, 14, GRAY);
    DrawRectangle(40, 165, 240, 80, PANEL_COLOR);
    DrawText(TextFormat("Tick: %d",        state->tick),       55, 180, 18, RAYWHITE);
    DrawText(TextFormat("Active Rides: %d", state->activeRides), 55, 210, 18, STATUS_OK);

    // metrics panel
    DrawText("FLEET PERFORMANCE", 40, 280, 14, GRAY);
    DrawRectangle(40, 305, 240, 150, PANEL_COLOR);
    DrawText("Completed:",  55, 325, 16, LIGHTGRAY);
    DrawText(TextFormat("%d", state->metrics.totalCompleted),         180, 325, 16, RAYWHITE);
    DrawText("Cancelled:",  55, 355, 16, LIGHTGRAY);
    DrawText(TextFormat("%d", state->metrics.totalCancelled),         180, 355, 16, STATUS_BUSY);
    DrawText("Utilization:", 55, 385, 16, LIGHTGRAY);
    DrawText(TextFormat("%.1f%%", state->metrics.driverUtilization * 100.0f), 180, 385, 16, ACCENT_GOLD);

    // end simulation Button
    DrawRectangleRounded(END_BTN, 0.2f, 8, (Color){ 180, 30, 30, 255 });
    int tw = MeasureText("END SIMULATION", 18);
    DrawText("END SIMULATION",
             (int)(END_BTN.x + END_BTN.width  / 2 - tw / 2),
             (int)(END_BTN.y + END_BTN.height / 2 - 9),
             18, RAYWHITE);

    // driver fleet grid
    int gridX = 320 + 20;
    int gridY = 85;
    DrawText("DRIVERS ONLINE", gridX, 40, 24, RAYWHITE);

    for (int i = 0; i < state->numDrivers; i++) {
        int row = i / 6;
        int col = i % 6;
        int x = gridX + col * 155;
        int y = gridY + row * 95;

        Color statusColor = (state->drivers[i].status == DRIVER_BUSY) ? STATUS_BUSY : STATUS_OK;
        DrawRectangleRounded((Rectangle){ (float)x, (float)y, 145, 85 }, 0.15f, 8, PANEL_COLOR);
        DrawCircle(x + 15, y + 20, 5, statusColor);
        DrawText(TextFormat("ID #%d", state->drivers[i].id), x + 30, y + 12, 16, RAYWHITE);

        const char *cat = (state->drivers[i].category == DRIVER_ELITE) ? "ELITE"
                        : (state->drivers[i].category == DRIVER_PLUS)  ? "PLUS" : "STD";
        DrawText(cat, x + 15, y + 40, 14, ACCENT_GOLD);

        if (state->drivers[i].status == DRIVER_BUSY) {
            DrawText(TextFormat("Ride: %d", state->drivers[i].currentRequestId), x + 15, y + 60, 14, LIGHTGRAY);
        } else {
            DrawText("Idle", x + 15, y + 60, 14, GRAY);
        }
    }

    // pending queue
    int queueY = 500;
    DrawText("PENDING QUEUE", gridX, queueY, 20, RAYWHITE);
    DrawRectangle(gridX, queueY + 30, 920, 240, (Color){ 20, 20, 22, 255 });
    DrawText("REQ ID",    gridX + 20,  queueY + 45, 14, GRAY);
    DrawText("PRIORITY",  gridX + 120, queueY + 45, 14, GRAY);
    DrawText("FARE",      gridX + 240, queueY + 45, 14, GRAY);
    DrawText("WAIT TIME", gridX + 360, queueY + 45, 14, GRAY);
    DrawLine(gridX + 20, queueY + 65, gridX + 920, queueY + 65, (Color){ 45, 45, 48, 255 });

    for (int i = 0; i < state->pendingCount && i < 8; i++) {
        int yPos = queueY + 80 + i * 20;
        Color pColor = (state->pendingRequests[i].type == EMERGENCY) ? RED
                     : (state->pendingRequests[i].type == VIP)       ? ACCENT_GOLD
                                                                      : RAYWHITE;
        const char *typeStr = (state->pendingRequests[i].type == EMERGENCY) ? "EMERGENCY"
                            : (state->pendingRequests[i].type == VIP)       ? "VIP" : "NORMAL";
        DrawText(TextFormat("#%d", state->pendingRequests[i].id),          gridX + 20,  yPos, 14, RAYWHITE);
        DrawText(typeStr,                                                   gridX + 120, yPos, 14, pColor);
        DrawText(TextFormat("$%d", state->pendingRequests[i].fare),        gridX + 240, yPos, 14, RAYWHITE);
        DrawText(TextFormat("%d ticks", state->pendingRequests[i].waitTicks), gridX + 360, yPos, 14, LIGHTGRAY);
    }
}

static void drawMetricsScreen(const MetricsSnapshot *m) {
    ClearBackground(BG_COLOR);

    int cx = SCREEN_WIDTH / 2;

    DrawText("SIMULATION COMPLETE", cx - MeasureText("SIMULATION COMPLETE", 36) / 2, 60, 36, ACCENT_GOLD);
    DrawText("Final Metrics", cx - MeasureText("Final Metrics", 20) / 2, 110, 20, GRAY);

    // metric rows
    int y = 180;
    int labelX = cx - 280;
    int valueX = cx + 80;
    int spacing = 45;
    int fs = 22;

    DrawText("Total Requests Created:",  labelX, y,              fs, LIGHTGRAY);
    DrawText(TextFormat("%d", m->totalCreated),   valueX, y,              fs, RAYWHITE);

    DrawText("Total Completed:",         labelX, y + spacing,    fs, LIGHTGRAY);
    DrawText(TextFormat("%d", m->totalCompleted), valueX, y + spacing,    fs, STATUS_OK);

    DrawText("Total Cancelled:",         labelX, y + spacing * 2, fs, LIGHTGRAY);
    DrawText(TextFormat("%d", m->totalCancelled), valueX, y + spacing * 2, fs, STATUS_BUSY);

    DrawText("Cancellation Rate:",       labelX, y + spacing * 3, fs, LIGHTGRAY);
    DrawText(TextFormat("%.1f%%", m->cancellationRate * 100.0f), valueX, y + spacing * 3, fs, ACCENT_GOLD);

    DrawText("Avg Wait (Normal):",       labelX, y + spacing * 4, fs, LIGHTGRAY);
    DrawText(TextFormat("%.1fs", m->avgWaitNormal),   valueX, y + spacing * 4, fs, RAYWHITE);

    DrawText("Avg Wait (VIP):",          labelX, y + spacing * 5, fs, LIGHTGRAY);
    DrawText(TextFormat("%.1fs", m->avgWaitVip),      valueX, y + spacing * 5, fs, RAYWHITE);

    DrawText("Avg Wait (Emergency):",    labelX, y + spacing * 6, fs, LIGHTGRAY);
    DrawText(TextFormat("%.1fs", m->avgWaitEmergency), valueX, y + spacing * 6, fs, RAYWHITE);

    DrawText("Driver Utilization:",      labelX, y + spacing * 7, fs, LIGHTGRAY);
    DrawText(TextFormat("%.1f%%", m->driverUtilization * 100.0f), valueX, y + spacing * 7, fs, ACCENT_GOLD);

    DrawText("Surge Active:",            labelX, y + spacing * 8, fs, LIGHTGRAY);
    DrawText(m->surgeActive ? "YES" : "NO", valueX, y + spacing * 8, fs, m->surgeActive ? STATUS_BUSY : STATUS_OK);

    // close hint
    DrawText("Press [ESC] or close the window to exit.",
             cx - MeasureText("Press [ESC] or close the window to exit.", 18) / 2,
             SCREEN_HEIGHT - 60, 18, GRAY);
}

void runGui(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "RideOS | Dispatch Dashboard");
    SetTargetFPS(60);

    // load PNG textures
    Texture2D introTex  = { 0 };
    Texture2D configTex = { 0 };
    bool introLoaded  = false;
    bool configLoaded = false;

    if (FileExists(INTRO_PNG_PATH)) {
        introTex    = LoadTexture(INTRO_PNG_PATH);
        introLoaded = true;
    } else {
        printf("GUI --- intro PNG not found at '%s', using fallback.\n", INTRO_PNG_PATH);
    }

    if (FileExists(CONFIG_PNG_PATH)) {
        configTex    = LoadTexture(CONFIG_PNG_PATH);
        configLoaded = true;
    } else {
        printf("GUI --- config PNG not found at '%s', using fallback.\n", CONFIG_PNG_PATH);
    }

    GuiScreen screen = GUI_INTRO;
    int selectedDrivers = 10;          // default, overwritten on config screen click
    bool generatorStarted = false;
    bool simulationEnded  = false;
    MetricsSnapshot finalMetrics = {0};
    pthread_t generatorTid;

    SharedState state = {0};

    while (!WindowShouldClose()) {
        Vector2 mouse   = GetMousePosition();
        bool   clicked  = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        switch (screen) {

            // intro screen
            case GUI_INTRO:
                if (clicked && CheckCollisionPointRec(mouse, INTRO_BTN)) {
                    screen = GUI_CONFIG;
                }
                break;

            // config screen
            case GUI_CONFIG:
                if (clicked) {
                    for (int i = 0; i < NUM_DRIVER_OPTIONS; i++) {
                        if (CheckCollisionPointRec(mouse, DRIVER_OPTION_RECTS[i])) {
                            selectedDrivers = DRIVER_OPTION_VALUES[i];
                            printf("GUI --- Driver count selected: %d\n", selectedDrivers);

                            // TODO: If you want to communicate the chosen driver
                            //       count to the Main Server before it starts,
                            //       do so here (e.g., write to shared memory or
                            //       send a special pipe message).
                            //       For now the Main Server uses its config.txt value.

                            // Start the automatic request generator now that
                            // the user has chosen a configuration.
                            if (!generatorStarted) {
                                if (pthread_create(&generatorTid, NULL, generatorLoop, NULL) == 0) {
                                    generatorStarted = true;
                                    pthread_detach(generatorTid);
                                    printf("GUI --- Generator thread started.\n");
                                } else {
                                    perror("GUI --- Failed to start generator thread");
                                }
                            }

                            screen = GUI_DASHBOARD;
                            break;
                        }
                    }
                }
                break;

            // dashboard
            case GUI_DASHBOARD: {
                int shmStatus = readSharedState(&state);

                // transitions to metrics if main server signals shutdown
                if (shmStatus == 0 && state.shutdownFlag && !simulationEnded) {
                    simulationEnded = true;
                    finalMetrics    = state.metrics;
                    generatorStop();
                    screen = GUI_METRICS;
                    break;
                }

                // end simulation button
                if (clicked && CheckCollisionPointRec(mouse, END_BTN) && !simulationEnded) {
                    simulationEnded = true;
                    finalMetrics    = state.metrics;
                    generatorStop();
                    // we don't kill the main server here, it will run 
                    // until its own duration expires or we send SIGINT manually.
                    screen = GUI_METRICS;
                }
                break;
            }

            // metrics screen
            case GUI_METRICS:
                // Nothing to handle - ESC / window close exits the loop
                break;
        }

        
        // rendering
        BeginDrawing();

        switch (screen) {

            case GUI_INTRO:
                drawIntroScreen(introTex, introLoaded);
                break;

            case GUI_CONFIG:
                drawConfigScreen(configTex, configLoaded);
                break;

            case GUI_DASHBOARD:
                // Show "connecting" overlay while shared memory isn't ready yet
                if (readSharedState(&state) != 0) {
                    ClearBackground(BG_COLOR);
                    const char *msg = "CONNECTING TO MAIN SERVER...";
                    DrawText(msg,
                             SCREEN_WIDTH / 2 - MeasureText(msg, 20) / 2,
                             SCREEN_HEIGHT / 2 - 10, 20, DARKGRAY);
                } else {
                    drawDashboard(&state);
                }
                break;

            case GUI_METRICS:
                drawMetricsScreen(&finalMetrics);
                break;
        }

        EndDrawing();
    }

    // cleanup
    generatorStop();
    if (introLoaded)  UnloadTexture(introTex);
    if (configLoaded) UnloadTexture(configTex);
    CloseWindow();
}