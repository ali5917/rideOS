#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <raylib.h>
#include "../include/gui.h"
#include "../include/ipc.h"
#include "../include/generator.h"

// Design Constants
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800
#define SIDEBAR_WIDTH 320
#define MARGIN 20

// Custom Palette
const Color BG_COLOR    = (Color){ 15, 15, 15, 255 };
const Color PANEL_COLOR = (Color){ 28, 28, 30, 255 };
const Color ACCENT_GOLD = (Color){ 255, 191, 0, 255 };
const Color STATUS_OK   = (Color){ 0, 230, 118, 255 };
const Color STATUS_BUSY = (Color){ 255, 61, 0, 255 };

void runGui(void) {
    // 1. Initialize Raylib
    SetConfigFlags(FLAG_MSAA_4X_HINT); // Anti-aliasing
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "RideOS | Dispatch Dashboard");
    SetTargetFPS(60);

    SharedState state;
    int nextManualId = 9000; // ID range for manual clicks

    while (!WindowShouldClose()) {
        // 2. Fetch Latest State from Shared Memory
        int shmStatus = readSharedState(&state);
        if (shmStatus == 0 && state.shutdownFlag) {
            break;
        }

        // 3. Update Logic (Input Handling)
        if (shmStatus == 0) {
            Vector2 mouse = GetMousePosition();
            // Check manual request button click (Rectangle: 40, 710, 240, 50)
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mouse, (Rectangle){ 40, 710, 240, 50 })) {
                    PipeRequest manualReq;
                    manualReq.id = nextManualId++;
                    manualReq.type = (rand() % 10 < 2) ? VIP : NORMAL; // 20% chance for VIP
                    manualReq.rideDuration = (rand() % 10) + 5;
                    manualReq.requestTime = time(NULL);
                    manualReq.baseFare = (manualReq.type == VIP) ? 40 : 20;
                    writePipeRequest(&manualReq);
                }
            }
        }

        // 4. Rendering
        BeginDrawing();
        ClearBackground(BG_COLOR);

        if (shmStatus != 0) {
            // Error State / Waiting for Server
            DrawText("ESTABLISHING CONNECTION TO MAIN SERVER...", 
                     SCREEN_WIDTH/2 - 250, SCREEN_HEIGHT/2 - 10, 20, DARKGRAY);
            EndDrawing();
            continue;
        }

        // --- SIDEBAR ---
        DrawRectangle(0, 0, SIDEBAR_WIDTH, SCREEN_HEIGHT, (Color){ 22, 22, 24, 255 });
        DrawText("RideOS", 40, 40, 36, ACCENT_GOLD);
        DrawText("Autonomous Dispatch", 40, 80, 16, GRAY);
        
        // System Health
        DrawText("SYSTEM STATUS", 40, 140, 14, GRAY);
        DrawRectangle(40, 165, 240, 80, PANEL_COLOR);
        DrawText(TextFormat("Tick: %d", state.tick), 55, 180, 18, RAYWHITE);
        DrawText(TextFormat("Active Rides: %d", state.activeRides), 55, 210, 18, STATUS_OK);

        // Metrics Panel
        DrawText("FLEET PERFORMANCE", 40, 280, 14, GRAY);
        DrawRectangle(40, 305, 240, 150, PANEL_COLOR);
        DrawText("Completed:", 55, 325, 16, LIGHTGRAY);
        DrawText(TextFormat("%d", state.metrics.totalCompleted), 180, 325, 16, RAYWHITE);
        DrawText("Cancelled:", 55, 355, 16, LIGHTGRAY);
        DrawText(TextFormat("%d", state.metrics.totalCancelled), 180, 355, 16, STATUS_BUSY);
        DrawText("Utilization:", 55, 385, 16, LIGHTGRAY);
        DrawText(TextFormat("%.1f%%", state.metrics.driverUtilization * 100), 180, 385, 16, ACCENT_GOLD);

        // Manual Request Button
        DrawRectangleRounded((Rectangle){ 40, 710, 240, 50 }, 0.2, 8, DARKBLUE);
        DrawText("SUBMIT MANUAL REQ", 65, 727, 18, RAYWHITE);

        // --- DRIVER FLEET GRID ---
        DrawText("DRIVERS ONLINE", SIDEBAR_WIDTH + MARGIN, 40, 24, RAYWHITE);
        int gridX = SIDEBAR_WIDTH + MARGIN;
        int gridY = 85;
        
        for (int i = 0; i < state.numDrivers; i++) {
            int row = i / 6;
            int col = i % 6;
            int x = gridX + col * 155;
            int y = gridY + row * 95;

            Color statusColor = (state.drivers[i].status == DRIVER_BUSY) ? STATUS_BUSY : STATUS_OK;
            
            // Draw Driver Card
            DrawRectangleRounded((Rectangle){ x, y, 145, 85 }, 0.15, 8, PANEL_COLOR);
            DrawCircle(x + 15, y + 20, 5, statusColor);
            DrawText(TextFormat("ID #%d", state.drivers[i].id), x + 30, y + 12, 16, RAYWHITE);
            
            const char* cat = (state.drivers[i].category == DRIVER_ELITE) ? "ELITE" : 
                             ((state.drivers[i].category == DRIVER_PLUS) ? "PLUS" : "STD");
            DrawText(cat, x + 15, y + 40, 14, ACCENT_GOLD);
            
            if (state.drivers[i].status == DRIVER_BUSY) {
                DrawText(TextFormat("Ride: %d", state.drivers[i].currentRequestId), x + 15, y + 60, 14, LIGHTGRAY);
            } else {
                DrawText("Idle", x + 15, y + 60, 14, GRAY);
            }
        }

        // --- PRIORITY QUEUE ---
        int queueY = 500;
        DrawText("PENDING QUEUE", SIDEBAR_WIDTH + MARGIN, queueY, 20, RAYWHITE);
        DrawRectangle(SIDEBAR_WIDTH + MARGIN, queueY + 30, 920, 240, (Color){ 20, 20, 22, 255 });
        
        // Header
        DrawText("REQ ID", SIDEBAR_WIDTH + 40, queueY + 45, 14, GRAY);
        DrawText("PRIORITY", SIDEBAR_WIDTH + 140, queueY + 45, 14, GRAY);
        DrawText("FARE", SIDEBAR_WIDTH + 260, queueY + 45, 14, GRAY);
        DrawText("WAIT TIME", SIDEBAR_WIDTH + 380, queueY + 45, 14, GRAY);
        DrawLine(SIDEBAR_WIDTH + 40, queueY + 65, SIDEBAR_WIDTH + 940, queueY + 65, (Color){ 45, 45, 48, 255 });

        for (int i = 0; i < state.pendingCount && i < 8; i++) {
            int yPos = queueY + 80 + i * 20;
            Color pColor = (state.pendingRequests[i].type == EMERGENCY) ? RED : 
                          ((state.pendingRequests[i].type == VIP) ? ACCENT_GOLD : RAYWHITE);
            
            const char* typeStr = (state.pendingRequests[i].type == EMERGENCY) ? "EMERGENCY" : 
                                 ((state.pendingRequests[i].type == VIP) ? "VIP" : "NORMAL");

            DrawText(TextFormat("#%d", state.pendingRequests[i].id), SIDEBAR_WIDTH + 40, yPos, 14, RAYWHITE);
            DrawText(typeStr, SIDEBAR_WIDTH + 140, yPos, 14, pColor);
            DrawText(TextFormat("$%d", state.pendingRequests[i].fare), SIDEBAR_WIDTH + 260, yPos, 14, RAYWHITE);
            DrawText(TextFormat("%d ticks", state.pendingRequests[i].waitTicks), SIDEBAR_WIDTH + 380, yPos, 14, LIGHTGRAY);
        }

        EndDrawing();
    }

    // 5. Cleanup
    generatorStop();
    CloseWindow();
}
