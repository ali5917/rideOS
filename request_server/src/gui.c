#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <raylib.h>
#include "../include/gui.h"
#include "../include/ipc.h"
#include "../include/generator.h"

// Layout 
#define SCREEN_W     1280
#define SCREEN_H      800
#define SIDEBAR_W     295
#define PAD            16
#define CARD_W        148
#define CARD_H         78
#define CARD_GAP        8
#define DRIVER_COLS     6

// GUI states──
typedef enum { GUI_INTRO, GUI_CONFIG, GUI_DASHBOARD, GUI_METRICS } GuiScreen;

// Brand palette──
// Signature yellow from the intro screen carried through the whole app.
static const Color C_BRAND    = {255, 212,   0, 255 }; // #FFD400 brand yellow
static const Color C_BG       = { 13,  13,  16, 255 }; // near-black
static const Color C_SIDE     = { 18,  18,  22, 255 }; // sidebar bg
static const Color C_PANEL    = { 25,  25,  31, 255 }; // card / panel
static const Color C_PANEL2   = { 30,  30,  38, 255 }; // alternate row tint
static const Color C_BORDER   = { 42,  42,  54, 255 }; // subtle border
static const Color C_GREEN    = {  0, 210, 100, 255 }; // free / completed
static const Color C_RED      = {235,  55,  45, 255 }; // busy / error
static const Color C_BLUE     = { 70, 160, 255, 255 }; // info / completion feed
static const Color C_ORANGE   = {255, 140,   0, 255 }; // PLUS category / warning
static const Color C_TEXT     = {245, 245, 250, 255 }; // primary text
static const Color C_DIM      = { 88,  88, 108, 255 }; // muted label
static const Color C_DARK     = { 18,  18,  20, 255 }; // dark text on yellow

// Asset paths─
#define INTRO_PNG_PATH  "request_server/assets/intro.png"
#define CONFIG_PNG_PATH "request_server/assets/config.png"

// Config screen hit areas
#define NUM_DRIVER_OPTIONS 4
static const Rectangle DRIVER_OPTION_RECTS[NUM_DRIVER_OPTIONS] = {
    { 200, 350, 160, 80 },
    { 420, 350, 160, 80 },
    { 640, 350, 160, 80 },
    { 860, 350, 160, 80 },
};
static const int DRIVER_OPTION_VALUES[NUM_DRIVER_OPTIONS] = { 5, 10, 15, 20 };

static const Rectangle MODE_AUTO_BTN   = { 360, 470, 240, 46 };
static const Rectangle MODE_MANUAL_BTN = { 680, 470, 240, 46 };

// End simulation button (sidebar)
static const Rectangle END_BTN = { PAD, SCREEN_H - 146, SIDEBAR_W - PAD * 2, 36 };

// Activity feed──
#define FEED_CAP 22
typedef struct { char text[84]; Color col; float age; } FeedEntry;
static FeedEntry feed[FEED_CAP];
static int       feedCount = 0;

static void feedPush(const char *msg, Color col) {
    if (feedCount < FEED_CAP) feedCount++;
    for (int i = feedCount - 1; i > 0; i--) feed[i] = feed[i - 1];
    strncpy(feed[0].text, msg, 83);
    feed[0].text[83] = '\0';
    feed[0].col = col;
    feed[0].age = 0.0f;
}

// Per-driver diffing 
typedef struct { int status; int currentRequestId; } DriverSnap;
static DriverSnap prevSnap[MAX_DRIVERS];
static int        prevDriverCount = 0;
static int        prevCancelled   = 0;
static float      driverFlash[MAX_DRIVERS];

// Helpers 
static Color alphaBlend(Color c, float a) {
    c.a = (unsigned char)(c.a * a);
    return c;
}

static Color lerpColor(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255
    };
}

static void drawPill(Rectangle r, const char *text, Color bg, Color fg, int fs) {
    DrawRectangleRounded(r, 0.35f, 6, bg);
    int tw = MeasureText(text, fs);
    DrawText(text, (int)(r.x + (r.width  - tw) / 2),
                   (int)(r.y + (r.height - fs) / 2), fs, fg);
}

// Yellow uppercase section label — brand-consistent
static void secLabel(const char *t, int x, int y) {
    DrawText(t, x, y, 11, C_BRAND);
}

static void resetDashboard(void) {
    feedCount = 0;
    memset(feed,        0, sizeof(feed));
    memset(driverFlash, 0, sizeof(driverFlash));
    memset(prevSnap,    0, sizeof(prevSnap));
    prevDriverCount = 0;
    prevCancelled   = 0;
}

// Intro screen
static void drawIntroScreen(Texture2D tex, bool loaded) {
    if (loaded) {
        // Stretch to fill window regardless of source resolution
        DrawTexturePro(tex,
            (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
            (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        // Fallback: recreate the yellow brand screen in Raylib
        ClearBackground((Color){ 255, 212, 0, 255 });

        const char *title = "RIDEOS";
        int tw = MeasureText(title, 120);
        // Chunky shadow offset
        DrawText(title, SCREEN_W / 2 - tw / 2 + 7, SCREEN_H / 2 - 108, 120,
                 (Color){ 28, 28, 28, 90 });
        DrawText(title, SCREEN_W / 2 - tw / 2,     SCREEN_H / 2 - 115, 120, WHITE);

        const char *sub = "DETERMINISTIC DISPATCH. PRIORITIZED MOBILITY";
        DrawText(sub,
                 SCREEN_W / 2 - MeasureText(sub, 17) / 2,
                 SCREEN_H / 2 + 30, 17, (Color){ 18, 55, 85, 255 });

        const char *h1 = "Press S to Start Simulation";
        DrawText(h1,
                 SCREEN_W / 2 - MeasureText(h1, 15) / 2,
                 SCREEN_H / 2 + 80, 15, (Color){ 40, 40, 50, 210 });

        const char *h2 = "Press M to Modify Configs";
        DrawText(h2,
                 SCREEN_W / 2 - MeasureText(h2, 14) / 2,
                 SCREEN_H - 55, 14, (Color){ 40, 40, 50, 180 });
    }
}

// Config screen
static void drawConfigScreen(Texture2D tex, bool loaded, int autoMode) {
    if (loaded) {
        DrawTexturePro(tex,
            (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
            (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
        // overlay mode chooser even when PNG is present
        DrawRectangleRounded(MODE_AUTO_BTN, 0.2f, 8,
                             autoMode ? C_PANEL : C_PANEL2);
        DrawRectangleRounded(MODE_MANUAL_BTN, 0.2f, 8,
                             autoMode ? C_PANEL2 : C_PANEL);
        DrawText("Random + Manual",
                 (int)(MODE_AUTO_BTN.x + 20),
                 (int)(MODE_AUTO_BTN.y + 14), 16,
                 autoMode ? C_BRAND : C_DIM);
        DrawText("Manual Only",
                 (int)(MODE_MANUAL_BTN.x + 36),
                 (int)(MODE_MANUAL_BTN.y + 14), 16,
                 autoMode ? C_DIM : C_BRAND);
        return;
    }

    // Fallback: yellow background with dark option cards
    ClearBackground((Color){ 255, 212, 0, 255 });

    const char *hdr = "Select Fleet Size";
    DrawText(hdr, SCREEN_W / 2 - MeasureText(hdr, 36) / 2,
             210, 36, (Color){ 18, 18, 22, 255 });
    const char *sub = "Choose the number of drivers for this simulation run";
    DrawText(sub, SCREEN_W / 2 - MeasureText(sub, 15) / 2,
             258, 15, (Color){ 40, 40, 50, 200 });

    for (int i = 0; i < NUM_DRIVER_OPTIONS; i++) {
        Rectangle r = DRIVER_OPTION_RECTS[i];
        DrawRectangleRounded(r, 0.22f, 8, (Color){ 18, 18, 22, 255 });
        // Yellow top stripe
        DrawRectangleRounded((Rectangle){ r.x, r.y, r.width, 4 },
                             0.5f, 4, C_BRAND);
        char num[8];
        snprintf(num, sizeof(num), "%d", DRIVER_OPTION_VALUES[i]);
        int nw = MeasureText(num, 34);
        DrawText(num, (int)(r.x + (r.width - nw) / 2), (int)(r.y + 14), 34, C_BRAND);
        const char *lbl = "drivers";
        int lw = MeasureText(lbl, 13);
        DrawText(lbl, (int)(r.x + (r.width - lw) / 2), (int)(r.y + 52), 13, C_DIM);
    }

    const char *modeHdr = "Request Mode";
    DrawText(modeHdr, SCREEN_W / 2 - MeasureText(modeHdr, 18) / 2,
             430, 18, (Color){ 18, 18, 22, 255 });

    DrawRectangleRounded(MODE_AUTO_BTN, 0.2f, 8,
                         autoMode ? C_PANEL : C_PANEL2);
    DrawRectangleRounded(MODE_MANUAL_BTN, 0.2f, 8,
                         autoMode ? C_PANEL2 : C_PANEL);
    DrawText("Random + Manual",
             (int)(MODE_AUTO_BTN.x + 20),
             (int)(MODE_AUTO_BTN.y + 14), 16,
             autoMode ? C_BRAND : C_DIM);
    DrawText("Manual Only",
             (int)(MODE_MANUAL_BTN.x + 36),
             (int)(MODE_MANUAL_BTN.y + 14), 16,
             autoMode ? C_DIM : C_BRAND);

    const char *esc = "Press ESC to go back";
    DrawText(esc, SCREEN_W / 2 - MeasureText(esc, 13) / 2,
             SCREEN_H - 50, 13, (Color){ 40, 40, 50, 160 });
}

// Metrics screen
static void drawMetricsScreen(const MetricsSnapshot *m) {
    ClearBackground(C_BG);

    // Yellow banner header — brand call-back
    DrawRectangle(0, 0, SCREEN_W, 80, C_BRAND);
    const char *banner = "SIMULATION COMPLETE";
    DrawText(banner,
             SCREEN_W / 2 - MeasureText(banner, 34) / 2,
             24, 34, C_DARK);

    const char *sub = "Final Performance Report";
    DrawText(sub,
             SCREEN_W / 2 - MeasureText(sub, 15) / 2,
             96, 15, C_DIM);

    int lx = SCREEN_W / 2 - 310;
    int rx = SCREEN_W / 2 +  70;
    int y  = 150;
    int sp = 46;
    int fs = 20;

    #define STAT(label, value, col) \
        DrawText(label, lx, y, fs, C_DIM); \
        DrawText(value, rx, y, fs, col);   \
        y += sp;

    STAT("Total Requests Created:",
         TextFormat("%d", m->totalCreated), C_TEXT)
    STAT("Completed:",
         TextFormat("%d", m->totalCompleted), C_GREEN)
    STAT("Cancelled:",
         TextFormat("%d", m->totalCancelled), C_RED)
    STAT("Cancellation Rate:",
         TextFormat("%.1f%%", m->cancellationRate * 100.0f), C_BRAND)

    DrawLine(lx, y, rx + 180, y, C_BORDER);
    y += 14;

    STAT("Avg Wait (Normal):",
         TextFormat("%.1f s", m->avgWaitNormal), C_TEXT)
    STAT("Avg Wait (VIP):",
         TextFormat("%.1f s", m->avgWaitVip), C_TEXT)
    STAT("Avg Wait (Emergency):",
         TextFormat("%.1f s", m->avgWaitEmergency), C_TEXT)

    DrawLine(lx, y, rx + 180, y, C_BORDER);
    y += 14;

    STAT("Driver Utilization:",
         TextFormat("%.1f%%", m->driverUtilization * 100.0f), C_BRAND)
    STAT("Surge Triggered:",
         m->surgeActive ? "YES" : "NO",
         m->surgeActive ? C_RED : C_GREEN)
    #undef STAT

    const char *hint = "Press ESC or close the window to exit";
    DrawText(hint,
             SCREEN_W / 2 - MeasureText(hint, 14) / 2,
             SCREEN_H - 44, 14, C_DIM);
}

// Dashboard
static void drawDashboard(const SharedState *s, int manualType, int pendingMaxRows) {
    ClearBackground(C_BG);

    // ── Sidebar─
    DrawRectangle(0, 0, SIDEBAR_W, SCREEN_H, C_SIDE);
    DrawRectangle(SIDEBAR_W - 1, 0, 1, SCREEN_H, C_BORDER);
    // 4 px brand stripe on left edge
    DrawRectangle(0, 0, 4, SCREEN_H, C_BRAND);

    DrawText("RideOS", PAD + 6, 24, 32, C_BRAND);
    DrawText("Dispatch Dashboard", PAD + 6, 60, 13, C_DIM);
    DrawLine(PAD + 6, 82, SIDEBAR_W - PAD, 82, C_BORDER);

    int sy = 92;

    // Surge badge
    if (s->metrics.surgeActive) {
        DrawRectangle(PAD + 6, sy, SIDEBAR_W - PAD * 2 - 2, 26, C_RED);
        const char *sm = "SURGE PRICING ACTIVE";
        DrawText(sm,
                 PAD + 6 + (SIDEBAR_W - PAD * 2 - 2 - MeasureText(sm, 11)) / 2,
                 sy + 7, 11, RAYWHITE);
        sy += 34;
    }

    // System panel
    secLabel("SYSTEM", PAD + 6, sy);
    sy += 14;
    DrawRectangle(PAD + 6, sy, SIDEBAR_W - PAD * 2, 54, C_PANEL);
    DrawRectangle(PAD + 6, sy, 3, 54, C_BRAND); // left accent
    DrawText(TextFormat("Tick  %d",         s->tick),
             PAD + 16, sy + 8,  15, C_TEXT);
    DrawText(TextFormat("Active rides  %d", s->activeRides),
             PAD + 16, sy + 30, 15, C_GREEN);
    sy += 66;

    // Performance panel
    secLabel("PERFORMANCE", PAD + 6, sy);
    sy += 14;
    DrawRectangle(PAD + 6, sy, SIDEBAR_W - PAD * 2, 100, C_PANEL);
    DrawRectangle(PAD + 6, sy, 3, 100, C_BRAND);
    float util = s->metrics.driverUtilization;
    int   bw   = SIDEBAR_W - PAD * 2 - 24;
    DrawText(TextFormat("Utilization  %.0f%%", util * 100.0f),
             PAD + 16, sy + 8, 15, C_BRAND);
    DrawRectangle(PAD + 16, sy + 28, bw, 5, C_BORDER); // track
    DrawRectangle(PAD + 16, sy + 28, (int)(bw * util), 5, C_BRAND); // fill
    DrawText(TextFormat("Completed  %d", s->metrics.totalCompleted),
             PAD + 16, sy + 44, 15, C_TEXT);
    DrawText(TextFormat("Cancelled   %d", s->metrics.totalCancelled),
             PAD + 16, sy + 66, 15, C_RED);
    sy += 112;

    // Queue depth badge
    secLabel("QUEUE", PAD + 6, sy);
    sy += 14;
    DrawRectangle(PAD + 6, sy, SIDEBAR_W - PAD * 2, 36, C_PANEL);
    DrawRectangle(PAD + 6, sy, 3, 36, C_BRAND);
    Color qc = (s->pendingCount >= 6) ? C_RED   :
               (s->pendingCount >= 3) ? C_ORANGE : C_GREEN;
    DrawText(TextFormat("%d pending", s->pendingCount),
             PAD + 16, sy + 10, 16, qc);

    // End simulation button
    DrawRectangleRounded(END_BTN, 0.2f, 6, (Color){ 175, 28, 28, 255 });
    const char *et = "END SIMULATION";
    DrawText(et,
             PAD + 6 + (SIDEBAR_W - PAD * 2 - MeasureText(et, 13)) / 2,
             (int)(END_BTN.y + 11), 13, RAYWHITE);

    // Manual request controls
    DrawLine(PAD + 6, SCREEN_H - 102, SIDEBAR_W - PAD, SCREEN_H - 102, C_BORDER);
    secLabel("MANUAL REQUEST", PAD + 6, SCREEN_H - 98);

    const char *typeLabel[] = { "NORMAL", "VIP", "EMERGENCY" };
    Color       typeColor[] = { C_TEXT, C_BRAND, C_RED };
    Color       typeBg[]    = {
        C_BORDER,
        (Color){ 50, 40,  0, 255 },
        (Color){ 55, 10,  8, 255 }
    };
    drawPill((Rectangle){ PAD + 6, SCREEN_H - 86, 128, 28 },
             typeLabel[manualType], typeBg[manualType],
             typeColor[manualType], 13);

    // Submit → yellow, dark text (brand-primary button)
    DrawRectangleRounded(
        (Rectangle){ PAD + 6, SCREEN_H - 50, SIDEBAR_W - PAD * 2, 40 },
        0.2f, 6, C_BRAND);
    const char *bt = "SUBMIT REQUEST";
    DrawText(bt,
             PAD + 6 + (SIDEBAR_W - PAD * 2 - MeasureText(bt, 15)) / 2,
             SCREEN_H - 40, 15, C_DARK);

    // MAIN AREA
    int mx = SIDEBAR_W + PAD;
    int mw = SCREEN_W - SIDEBAR_W - PAD * 2;

    // Yellow top bar across entire main area
    DrawRectangle(mx, 0, mw + PAD, 28, C_BRAND);
    DrawText("FLEET STATUS", mx + 8, 7, 13, C_DARK);

    // Driver grid 
    int gridTop = 36;
    for (int i = 0; i < s->numDrivers; i++) {
        int col = i % DRIVER_COLS;
        int row = i / DRIVER_COLS;
        int cx  = mx + col * (CARD_W + CARD_GAP);
        int cy  = gridTop + row * (CARD_H + CARD_GAP);

        int busy = (s->drivers[i].status == DRIVER_BUSY);

        // Flash lerp on newly assigned / just-freed drivers
        Color cardBg = C_PANEL;
        if (driverFlash[i] > 0.0f) {
            Color tint = busy ? (Color){ 0, 50, 22, 255 }
                              : (Color){ 8, 26, 50, 255 };
            cardBg = lerpColor(C_PANEL, tint, driverFlash[i]);
        }
        DrawRectangleRounded((Rectangle){ cx, cy, CARD_W, CARD_H },
                             0.15f, 6, cardBg);

        // Top stripe colour by category
        Color stripeCol =
            (s->drivers[i].category == DRIVER_PLUS)  ? C_ORANGE : C_BORDER;
        DrawRectangleRounded((Rectangle){ cx, cy, CARD_W, 3 },
                             0.5f, 4, stripeCol);

        DrawCircle(cx + 11, cy + 14, 4, busy ? C_RED : C_GREEN);
        DrawText(TextFormat("#%d", s->drivers[i].id),
                 cx + 22, cy + 7, 16, C_TEXT);

        const char *cat =
            (s->drivers[i].category == DRIVER_PLUS)  ? "PLUS" : "STD";
        DrawText(cat, cx + 22, cy + 27, 12, stripeCol);

        if (busy)
            DrawText(TextFormat("Req #%d", s->drivers[i].currentRequestId),
                     cx + 10, cy + 56, 13, C_GREEN);
        else
            DrawText("Idle", cx + 10, cy + 56, 13, C_DIM);
    }

    // Divider
    int driverRows = (s->numDrivers + DRIVER_COLS - 1) / DRIVER_COLS;
    int divY = gridTop + driverRows * (CARD_H + CARD_GAP) + PAD - 4;
    if (divY < 300) divY = 300;
    DrawLine(mx, divY, SCREEN_W - PAD, divY, C_BORDER);
    // Small yellow rotated square at centre of divider
    int dmx = (mx + SCREEN_W - PAD) / 2;
    DrawRectanglePro((Rectangle){ dmx, divY, 8, 8 },
                     (Vector2){ 4, 4 }, 45.0f, C_BRAND);

    // Bottom split: queue | feed
    int botY = divY + PAD;
    int botH = SCREEN_H - botY - PAD;
    int qW   = mw / 2 - PAD / 2;
    int fX   = mx + qW + PAD;
    int fW   = mw - qW - PAD;

    // Pending queue
    secLabel("PENDING QUEUE", mx, botY);
    DrawRectangle(mx, botY + 16, qW, botH - 16, C_PANEL);

    if (s->pendingCount == 0) {
        const char *em = "Queue is empty";
        DrawText(em,
                 mx + (qW - MeasureText(em, 14)) / 2,
                 botY + 16 + (botH - 16) / 2 - 7, 14, C_DIM);
    } else {
        int qy   = botY + 24;
        DrawText("ID",   mx + 10,  qy, 12, C_DIM);
        DrawText("TYPE", mx + 65,  qy, 12, C_DIM);
        DrawText("FARE", mx + 160, qy, 12, C_DIM);
        DrawText("WAIT", mx + 230, qy, 12, C_DIM);
        DrawLine(mx + 8, qy + 15, mx + qW - 8, qy + 15, C_BORDER);

        int rowH = 21;
        for (int i = 0; i < s->pendingCount && i < pendingMaxRows; i++) {
            int ry = botY + 44 + i * rowH;
            if (i % 2 == 0)
                DrawRectangle(mx, ry - 2, qW, rowH, C_PANEL2);

            const char *ts =
                (s->pendingRequests[i].type == EMERGENCY) ? "EMER" :
                (s->pendingRequests[i].type == VIP)       ? "VIP"  : "NORM";
            Color tc =
                (s->pendingRequests[i].type == EMERGENCY) ? C_RED   :
                (s->pendingRequests[i].type == VIP)       ? C_BRAND : C_TEXT;

            DrawText(TextFormat("#%d",  s->pendingRequests[i].id),      mx + 10,  ry, 13, C_TEXT);
            DrawText(ts,                                                  mx + 65,  ry, 13, tc);
            DrawText(TextFormat("$%d",  s->pendingRequests[i].fare),    mx + 160, ry, 13, C_TEXT);
            DrawText(TextFormat("%dt",  s->pendingRequests[i].waitTicks), mx + 230, ry, 13, C_DIM);
        }
        if (s->pendingCount > pendingMaxRows)
            DrawText(TextFormat("+%d more", s->pendingCount - pendingMaxRows),
                     mx + 10, botY + 44 + pendingMaxRows * rowH + 2, 12, C_DIM);
    }

    // Live activity feed
    secLabel("LIVE ACTIVITY", fX, botY);
    DrawRectangle(fX, botY + 16, fW, botH - 16, C_PANEL);

    if (feedCount == 0) {
        const char *em = "No events yet";
        DrawText(em,
                 fX + (fW - MeasureText(em, 14)) / 2,
                 botY + 16 + (botH - 16) / 2 - 7, 14, C_DIM);
    } else {
        int rowH    = 19;
        int maxRows = (botH - 26) / rowH;
        for (int i = 0; i < feedCount && i < maxRows; i++) {
            float fade = 1.0f - (feed[i].age / 25.0f);
            if (fade < 0.18f) fade = 0.18f;
            // Newest entry: yellow left accent bar
            if (i == 0)
                DrawRectangle(fX, botY + 22, 3, rowH, C_BRAND);
            DrawText(feed[i].text, fX + 10,
                     botY + 24 + i * rowH, 13,
                     alphaBlend(feed[i].col, fade));
        }
    }
}

// Main loop
void runGui(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "RideOS | Dispatch Dashboard");
    SetTargetFPS(60);

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

    GuiScreen screen          = GUI_INTRO;
    int       selectedDrivers = 10;
    int       nextManualId    = 9000;
    int       manualType      = 0;
    int       autoMode        = 1;

    bool            generatorStarted = false;
    bool            simulationEnded  = false;
    MetricsSnapshot finalMetrics     = {0};
    pthread_t       generatorTid;

    SharedState state = {0};
    int mainPid = -1;
    resetDashboard();

    // Macro: launch generator and jump to dashboard
    #define START_SIM() do {                                                     
        if (!generatorStarted) {                                                  
            if (pthread_create(&generatorTid, NULL, generatorLoop, NULL) == 0) { 
                generatorStarted = true;                                          
                pthread_detach(generatorTid);                                     
                printf("GUI --- Generator started (%d drivers).\n",              
                       selectedDrivers);                                           
            } else {                                                              
                perror("GUI --- Failed to start generator thread");               
            }                                                                     
        }                                                                         
        resetDashboard();                                                         
        screen = GUI_DASHBOARD;                                                   
    } while (0)

    while (!WindowShouldClose()) {
        float   dt      = GetFrameTime();
        Vector2 mouse   = GetMousePosition();
        bool    clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        switch (screen) {

            case GUI_INTRO:
                // S → start with default fleet (10 drivers)
                if (IsKeyPressed(KEY_S)) {
                    selectedDrivers = 10;
                    START_SIM();
                }
                // M → config screen to pick fleet size
                if (IsKeyPressed(KEY_M))
                    screen = GUI_CONFIG;
                // Fallback: any click goes to config
                if (clicked)
                    screen = GUI_CONFIG;
                break;

            case GUI_CONFIG:
                if (clicked) {
                    if (CheckCollisionPointRec(mouse, MODE_AUTO_BTN)) {
                        autoMode = 1;
                    }
                    if (CheckCollisionPointRec(mouse, MODE_MANUAL_BTN)) {
                        autoMode = 0;
                    }
                    for (int i = 0; i < NUM_DRIVER_OPTIONS; i++) {
                        if (CheckCollisionPointRec(mouse, DRIVER_OPTION_RECTS[i])) {
                            selectedDrivers = DRIVER_OPTION_VALUES[i];
                            PipeRequest cfg = {0};
                            cfg.msgType = PIPE_MSG_CONFIG;
                            cfg.configDrivers = selectedDrivers;
                            writePipeRequest(&cfg);

                            if (autoMode) {
                                START_SIM();
                            } else {
                                resetDashboard();
                                screen = GUI_DASHBOARD;
                            }
                            break;
                        }
                    }
                }
                if (IsKeyPressed(KEY_ESCAPE))
                    screen = GUI_INTRO;
                break;

            case GUI_DASHBOARD: {
                // Check shutdown flag from main server
                SharedState tmp;
                if (readSharedState(&tmp) == 0 && tmp.shutdownFlag
                        && !simulationEnded) {
                    if (tmp.mainPid > 0) mainPid = tmp.mainPid;
                    simulationEnded = true;
                    finalMetrics    = tmp.metrics;
                    generatorStop();
                    screen = GUI_METRICS;
                    break;
                }

                // State diffing → feed events
                if (readSharedState(&state) == 0) {
                    if (state.mainPid > 0) mainPid = state.mainPid;
                    for (int i = prevDriverCount; i < state.numDrivers; i++) {
                        prevSnap[i].status           = DRIVER_ONLINE;
                        prevSnap[i].currentRequestId = -1;
                    }
                    for (int i = 0; i < state.numDrivers && i < MAX_DRIVERS; i++) {
                        int wasBusy = (prevSnap[i].status == DRIVER_BUSY);
                        int isBusy  = (state.drivers[i].status == DRIVER_BUSY);

                        if (!wasBusy && isBusy) {
                            const char *cat =
                                (state.drivers[i].category == DRIVER_PLUS) ? "PLUS" : "STD";
                            char buf[84];
                            snprintf(buf, sizeof(buf),
                                     "Driver #%d (%s) → Req #%d",
                                     state.drivers[i].id, cat,
                                     state.drivers[i].currentRequestId);
                            feedPush(buf, C_GREEN);
                            driverFlash[i] = 1.0f;
                        } else if (wasBusy && !isBusy) {
                            char buf[84];
                            snprintf(buf, sizeof(buf),
                                     "Driver #%d completed ride",
                                     state.drivers[i].id);
                            feedPush(buf, C_BLUE);
                            driverFlash[i] = 0.6f;
                        }
                        prevSnap[i].status           = state.drivers[i].status;
                        prevSnap[i].currentRequestId = state.drivers[i].currentRequestId;
                    }
                    prevDriverCount = state.numDrivers;

                    int newCanc = state.metrics.totalCancelled - prevCancelled;
                    if (newCanc > 0) {
                        char buf[84];
                        snprintf(buf, sizeof(buf),
                                 "%d request(s) timed out / cancelled", newCanc);
                        feedPush(buf, C_RED);
                    }
                    prevCancelled = state.metrics.totalCancelled;
                }

                for (int i = 0; i < feedCount;    i++) feed[i].age    += dt;
                for (int i = 0; i < MAX_DRIVERS;  i++)
                    if (driverFlash[i] > 0.0f) driverFlash[i] -= dt;

                if (clicked) {
                    Rectangle typeBtn = { PAD + 6, SCREEN_H - 86, 128, 28 };
                    if (CheckCollisionPointRec(mouse, typeBtn))
                        manualType = (manualType + 1) % 3;

                    Rectangle submitBtn = { PAD + 6, SCREEN_H - 50,
                                            SIDEBAR_W - PAD * 2, 40 };
                    if (CheckCollisionPointRec(mouse, submitBtn)) {
                        PipeRequest req  = {0};
                        req.msgType      = PIPE_MSG_REQUEST;
                        req.id           = nextManualId++;
                        req.type         = manualType;
                        req.rideDuration = (rand() % 10) + 5;
                        req.requestTime  = time(NULL);
                        req.baseFare     = (manualType == 2) ? 60 :
                                           (manualType == 1) ? 40 : 20;
                        req.configDrivers = 0;
                        writePipeRequest(&req);

                        const char *ts = (manualType == 2) ? "EMERGENCY" :
                                         (manualType == 1) ? "VIP" : "NORMAL";
                        Color tc = (manualType == 2) ? C_RED   :
                                   (manualType == 1) ? C_BRAND : C_BLUE;
                        char buf[84];
                        snprintf(buf, sizeof(buf),
                                 "Manual req #%d (%s) submitted", req.id, ts);
                        feedPush(buf, tc);
                    }

                    if (CheckCollisionPointRec(mouse, END_BTN) && !simulationEnded) {
                        simulationEnded = true;
                        finalMetrics    = state.metrics;
                        generatorStop();
                        screen = GUI_METRICS;
                    }
                }
                break;
            }

            case GUI_METRICS:
                if (IsKeyPressed(KEY_ESCAPE))
                    goto cleanup;
                break;
        }

        BeginDrawing();

        switch (screen) {
            case GUI_INTRO:
                drawIntroScreen(introTex, introLoaded);
                break;

            case GUI_CONFIG:
                drawConfigScreen(configTex, configLoaded, autoMode);
                break;

            case GUI_DASHBOARD:
                if (readSharedState(&state) != 0) {
                    ClearBackground(C_BG);
                    DrawRectangle(0, 0, SCREEN_W, 28, C_BRAND);
                    const char *msg = "CONNECTING TO MAIN SERVER...";
                    DrawText(msg,
                             SCREEN_W / 2 - MeasureText(msg, 18) / 2,
                             SCREEN_H / 2 - 9, 18, C_DIM);
                } else {
                    int pmr = (SCREEN_H - 16 - 26 - 300) / 21;
                    if (pmr < 3) pmr = 3;
                    drawDashboard(&state, manualType, pmr);
                }
                break;

            case GUI_METRICS:
                drawMetricsScreen(&finalMetrics);
                break;
        }

        EndDrawing();
    }

cleanup:
    #undef START_SIM
    if (mainPid > 0) {
        kill(mainPid, SIGINT);
    }
    generatorStop();
    if (introLoaded)  UnloadTexture(introTex);
    if (configLoaded) UnloadTexture(configTex);
    CloseWindow();
}