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

// GUI states
typedef enum { GUI_INTRO, GUI_CONFIG, GUI_DASHBOARD, GUI_METRICS } GuiScreen;

int num_drivers_grid[10][9] = {
    { 0, 0,  0,  0,  0,  0,  0, 0, 0},
    { 0, 0,  0,  0,  0,  0,  0, 0, 0},
    { 0, 0,  0,  0,  0,  0,  0, 0, 0},
    { 0, 0,  5,  5,  0, 10, 10, 0, 0},
    { 0, 0,  5,  5,  0, 10, 10, 0, 0},
    { 0, 0,  0,  0,  0,  0,  0, 0, 0},
    { 0, 0,  0,  0,  0,  0,  0, 0, 0},
    { 0, 0, 15, 15,  0, 20, 20, 0, 0},
    { 0, 0, 15, 15,  0, 20, 20, 0, 0},
    { 0, 0,  0,  0,  0,  0,  0, 0, 0},
};

// 10 rows, 9 columns
int bg_grid[10][9] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// Brand palette (matching bg.png)
static const Color C_BRAND    = {  6,  92, 120, 255 };
static const Color C_BG       = {232, 248, 250, 255 };
static const Color C_SIDE     = {214, 238, 242, 255 };
static const Color C_PANEL    = {204, 231, 237, 255 };
static const Color C_PANEL2   = {194, 223, 230, 255 };
static const Color C_BORDER   = {150, 192, 201, 255 };
static const Color C_GREEN    = {  0, 160, 120, 255 };
static const Color C_RED      = {220,  70,  60, 255 };
static const Color C_BLUE     = { 40, 120, 190, 255 };
static const Color C_ORANGE   = {230, 145,  60, 255 };
static const Color C_TEXT     = { 14,  50,  68, 255 };
static const Color C_DIM      = { 58, 104, 124, 255 };
static const Color C_DARK     = { 10,  40,  55, 255 };

// Additional palette entries used by the improved dashboard
static const Color C_SIDE_HDR = {  4,  58,  76, 255 };  // dark sidebar header bg
static const Color C_SIDE_MID = {  8,  72,  96, 255 };  // slightly lighter sidebar mid
static const Color C_PANEL3   = {188, 218, 226, 255 };  // darker panel variant
static const Color C_AMBER    = {230, 175,  20, 255 };  // warm amber for surge

// Asset paths
#define INTRO_PNG_PATH  "assets/bg.png"
#define CONFIG_PNG_PATH "assets/selectDrivers.png"

// End simulation button (sidebar — fixed position from bottom)
static const Rectangle END_BTN = { PAD, SCREEN_H - 56, SIDEBAR_W - PAD * 2, 36 };

// Activity feed
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

// ── Helpers ──────────────────────────────────────────────────────────────────

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

// Filled rounded rectangle with centred text
static void drawPill(Rectangle r, const char *text, Color bg, Color fg, int fs) {
    DrawRectangleRounded(r, 0.35f, 6, bg);
    int tw = MeasureText(text, fs);
    DrawText(text, (int)(r.x + (r.width  - tw) / 2),
                   (int)(r.y + (r.height - fs) / 2), fs, fg);
}

// Small caps section label in brand colour
static void secLabel(const char *t, int x, int y) {
    DrawText(t, x, y, 11, C_BRAND);
}

// Horizontal progress bar with rounded end-cap on the fill
static void drawBar(int x, int y, int w, int h, float frac,
                    Color track, Color fill) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    DrawRectangle(x, y, w, h, track);
    int fw = (int)(w * frac);
    if (fw > 0) {
        DrawRectangle(x, y, fw, h, fill);
        // Small rounded cap at fill edge
        if (fw >= h)
            DrawCircle(x + fw, y + h / 2, h / 2.0f, fill);
    }
}

// ── Sidebar panel block ───────────────────────────────────────────────────────
// Draws a labelled panel card with a coloured left accent, returns new y.
static int sidePanel(int sy, int h, Color accent, const char *label) {
    secLabel(label, PAD + 6, sy);
    sy += 14;
    DrawRectangle(PAD + 4, sy, SIDEBAR_W - PAD * 2 + 2, h, C_PANEL);
    DrawRectangle(PAD + 4, sy, 3, h, accent);
    return sy;  // caller draws content relative to this y, then advances by h+gap
}

static void resetDashboard(void) {
    feedCount = 0;
    memset(feed,        0, sizeof(feed));
    memset(driverFlash, 0, sizeof(driverFlash));
    memset(prevSnap,    0, sizeof(prevSnap));
    prevDriverCount = 0;
    prevCancelled   = 0;
}

static void submitManualRequest(int *nextId, int manualType) {
    if (nextId == NULL) return;
    PipeRequest req = {0};
    req.msgType = PIPE_MSG_REQUEST;
    req.id = (*nextId)++;
    req.type = manualType;
    req.rideDuration = (rand() % 10) + 5;
    req.requestTime = time(NULL);
    req.baseFare = (manualType == 2) ? 60 : (manualType == 1) ? 40 : 20;
    req.configDrivers = 0;
    writePipeRequest(&req);

    const char *ts = (manualType == 2) ? "EMERGENCY" :
                     (manualType == 1) ? "VIP" : "NORMAL";
    Color tc = (manualType == 2) ? C_RED :
               (manualType == 1) ? C_BRAND : C_BLUE;
    char buf[84];
    snprintf(buf, sizeof(buf), "Manual req #%d (%s) submitted", req.id, ts);
    feedPush(buf, tc);
}

static int gridValueAt(const int grid[10][9], Vector2 pos) {
    float cellW = (float)SCREEN_W / 9.0f;
    float cellH = (float)SCREEN_H / 10.0f;
    int col = (int)(pos.x / cellW);
    int row = (int)(pos.y / cellH);
    if (row < 0 || row >= 10 || col < 0 || col >= 9) return 0;
    return grid[row][col];
}

static int gridChoiceByIndex(const int grid[10][9], int index) {
    int values[16];
    int count = 0;
    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < 9; c++) {
            int v = grid[r][c];
            if (v <= 0) continue;
            int exists = 0;
            for (int i = 0; i < count; i++) {
                if (values[i] == v) { exists = 1; break; }
            }
            if (!exists && count < (int)(sizeof(values) / sizeof(values[0])))
                values[count++] = v;
        }
    }
    if (index < 0 || index >= count) return 0;
    return values[index];
}

// ── Intro screen ─────────────────────────────────────────────────────────────
static void drawIntroScreen(Texture2D tex, bool loaded) {
    if (loaded) {
        DrawTexturePro(tex,
            (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
            (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        ClearBackground((Color){ 255, 212, 0, 255 });
        const char *title = "RIDEOS";
        int tw = MeasureText(title, 120);
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
        DrawText("Press S to continue",
             SCREEN_W / 2 - MeasureText("Press S to continue", 14) / 2,
             SCREEN_H - 55, 14, C_DIM);
    }
}

// ── Config screen ─────────────────────────────────────────────────────────────
static void drawConfigScreen(Texture2D tex, bool loaded) {
    if (loaded) {
        DrawTexturePro(tex,
            (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
            (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
        return;
    }
    ClearBackground((Color){ 255, 212, 0, 255 });
    const char *hdr = "Select Fleet Size";
    DrawText(hdr, SCREEN_W / 2 - MeasureText(hdr, 36) / 2, 210, 36, (Color){ 18, 18, 22, 255 });
    const char *sub = "Choose the number of drivers for this simulation run";
    DrawText(sub, SCREEN_W / 2 - MeasureText(sub, 15) / 2, 258, 15, (Color){ 40, 40, 50, 200 });
    const char *hint = "Click a driver count to continue";
    DrawText(hint, SCREEN_W / 2 - MeasureText(hint, 16) / 2, 430, 16, (Color){ 18, 18, 22, 255 });
    const char *esc = "Press ESC to go back";
    DrawText(esc, SCREEN_W / 2 - MeasureText(esc, 13) / 2, SCREEN_H - 50, 13, (Color){ 40, 40, 50, 160 });
}

// ── Metrics screen ────────────────────────────────────────────────────────────
static void drawMetricsScreen(const MetricsSnapshot *m) {
    ClearBackground(C_BG);
    DrawRectangle(0, 0, SCREEN_W, 80, C_BRAND);
    const char *banner = "SIMULATION COMPLETE";
    DrawText(banner, SCREEN_W / 2 - MeasureText(banner, 34) / 2, 24, 34, C_DARK);
    const char *sub = "Final Performance Report";
    DrawText(sub, SCREEN_W / 2 - MeasureText(sub, 15) / 2, 96, 15, C_DIM);

    int lx = SCREEN_W / 2 - 310;
    int rx = SCREEN_W / 2 +  70;
    int y  = 150;
    int sp = 46;
    int fs = 20;

    #define STAT(label, value, col) \
        DrawText(label, lx, y, fs, C_DIM); \
        DrawText(value, rx, y, fs, col);   \
        y += sp;

    STAT("Total Requests Created:", TextFormat("%d",    m->totalCreated),           C_TEXT)
    STAT("Completed:",              TextFormat("%d",    m->totalCompleted),          C_GREEN)
    STAT("Cancelled:",              TextFormat("%d",    m->totalCancelled),          C_RED)
    STAT("Cancellation Rate:",      TextFormat("%.1f%%",m->cancellationRate * 100.0f), C_BRAND)
    DrawLine(lx, y, rx + 180, y, C_BORDER); y += 14;
    STAT("Avg Wait (Normal):",      TextFormat("%.1f s",m->avgWaitNormal),           C_TEXT)
    STAT("Avg Wait (VIP):",         TextFormat("%.1f s",m->avgWaitVip),              C_TEXT)
    STAT("Avg Wait (Emergency):",   TextFormat("%.1f s",m->avgWaitEmergency),        C_TEXT)
    DrawLine(lx, y, rx + 180, y, C_BORDER); y += 14;
    STAT("Driver Utilization:",     TextFormat("%.1f%%",m->driverUtilization * 100.0f), C_BRAND)
    STAT("Surge Triggered:",        m->surgeActive ? "YES" : "NO",
                                    m->surgeActive ? C_RED : C_GREEN)
    #undef STAT

    const char *hint = "Press ESC or close the window to exit";
    DrawText(hint, SCREEN_W / 2 - MeasureText(hint, 14) / 2, SCREEN_H - 44, 14, C_DIM);
}

// ── Dashboard ─────────────────────────────────────────────────────────────────
static void drawDashboard(const SharedState *s, int manualType, int pendingMaxRows) {
    ClearBackground(C_BG);

    // ════════════════════════════════════════════════════
    //  SIDEBAR
    // ════════════════════════════════════════════════════

    // Dark header block (top ~88 px)
    DrawRectangle(0, 0, SIDEBAR_W, 88, C_SIDE_HDR);
    // Thin brand stripe on far-left edge
    DrawRectangle(0, 0, 4, SCREEN_H, C_BRAND);
    // Separator below header
    DrawRectangle(0, 88, SIDEBAR_W, 2, C_BRAND);
    // Rest of sidebar background
    DrawRectangle(4, 90, SIDEBAR_W - 4, SCREEN_H - 90, C_SIDE);
    // Right border
    DrawRectangle(SIDEBAR_W - 1, 0, 1, SCREEN_H, C_BORDER);

    // Brand wordmark
    DrawText("RideOS", PAD + 8, 16, 30, RAYWHITE);
    DrawText("DISPATCH", PAD + 8,  54, 11, (Color){ 160, 210, 225, 255 });
    DrawText("DASHBOARD",
             PAD + 8 + MeasureText("DISPATCH", 11) + 6, 56, 10,
             (Color){ 220, 190, 50, 200 });

    int sy = 100;   // running y cursor inside sidebar

    // ── Surge badge (conditional) ─────────────────────────────────────────────
    if (s->metrics.surgeActive) {
        DrawRectangle(PAD + 4, sy, SIDEBAR_W - PAD * 2 + 2, 30, C_RED);
        DrawRectangle(PAD + 4, sy, 4, 30, C_AMBER);
        const char *sm = "!! SURGE PRICING ACTIVE !!";
        DrawText(sm,
                 PAD + 4 + (SIDEBAR_W - PAD * 2 + 2 - MeasureText(sm, 11)) / 2,
                 sy + 10, 11, RAYWHITE);
        sy += 38;
    }

    // ── System panel ─────────────────────────────────────────────────────────
    int panelY = sidePanel(sy, 60, C_BRAND, "SYSTEM");
    DrawText("TICK",         PAD + 14, panelY + 8,  10, C_DIM);
    DrawText(TextFormat("%d", s->tick),
                             PAD + 52, panelY + 6,  16, C_TEXT);
    DrawCircle(PAD + 14, panelY + 42, 5,
               s->activeRides > 0 ? C_GREEN : C_DIM);
    DrawText(TextFormat("%d active ride%s", s->activeRides,
                        s->activeRides == 1 ? "" : "s"),
             PAD + 24, panelY + 36, 13, C_TEXT);
    sy += 14 + 60 + 10;

    // ── Performance panel ─────────────────────────────────────────────────────
    panelY = sidePanel(sy, 112, C_BRAND, "PERFORMANCE");
    {
        float util = s->metrics.driverUtilization;
        int   bw   = SIDEBAR_W - PAD * 2 - 22;
        Color utilCol = (util > 0.8f) ? C_RED
                      : (util > 0.5f) ? C_ORANGE
                      :                  C_GREEN;
        DrawText("UTILIZATION",        PAD + 14, panelY + 8,  10, C_DIM);
        // Right-aligned percentage
        const char *pct = TextFormat("%.0f%%", util * 100.0f);
        DrawText(pct, PAD + 4 + SIDEBAR_W - PAD * 2 - MeasureText(pct, 16),
                 panelY + 6, 16, utilCol);
        drawBar(PAD + 14, panelY + 30, bw, 6, util, C_BORDER, utilCol);

        DrawLine(PAD + 14, panelY + 46, PAD + 14 + bw, panelY + 46, C_BORDER);

        // Completed / Cancelled in two columns
        DrawText("DONE",   PAD + 14,        panelY + 54, 10, C_DIM);
        DrawText("CANCL",  PAD + 14 + bw/2, panelY + 54, 10, C_DIM);
        DrawText(TextFormat("%d", s->metrics.totalCompleted),
                 PAD + 14,        panelY + 66, 18, C_GREEN);
        DrawText(TextFormat("%d", s->metrics.totalCancelled),
                 PAD + 14 + bw/2, panelY + 66, 18, C_RED);

        // Cancel-rate sub-label
        DrawText(TextFormat("%.0f%% cancel rate",
                            s->metrics.cancellationRate * 100.0f),
                 PAD + 14, panelY + 92, 10, C_DIM);
    }
    sy += 14 + 112 + 10;

    // ── Avg wait panel ───────────────────────────────────────────────────────
    panelY = sidePanel(sy, 72, C_BLUE, "AVG WAIT TIMES");
    {
        int bw = SIDEBAR_W - PAD * 2 - 22;
        const char *wLabels[] = { "NORM", "VIP",  "EMER" };
        Color       wColors[] = { C_DIM,  C_BRAND, C_RED };
        float       wTimes[]  = {
            s->metrics.avgWaitNormal,
            s->metrics.avgWaitVip,
            s->metrics.avgWaitEmergency
        };
        int colW = bw / 3;
        for (int i = 0; i < 3; i++) {
            int wx = PAD + 14 + i * colW;
            DrawText(wLabels[i], wx, panelY + 7,  10, C_DIM);
            DrawText(TextFormat("%.0fs", wTimes[i]),
                     wx, panelY + 21, 15, wColors[i]);
            float frac = wTimes[i] / 60.0f;
            if (frac > 1.0f) frac = 1.0f;
            drawBar(wx, panelY + 46, colW - 4, 4, frac, C_BORDER, wColors[i]);
        }
    }
    sy += 14 + 72 + 10;

    // ── Queue depth panel ────────────────────────────────────────────────────
    panelY = sidePanel(sy, 42, 
                       (s->pendingCount >= 6) ? C_RED
                     : (s->pendingCount >= 3) ? C_ORANGE : C_GREEN,
                       "QUEUE DEPTH");
    {
        Color qc = (s->pendingCount >= 6) ? C_RED
                 : (s->pendingCount >= 3) ? C_ORANGE : C_GREEN;
        int bw = SIDEBAR_W - PAD * 2 - 22;
        const char *qs = TextFormat("%d", s->pendingCount);
        DrawText(qs, PAD + 14, panelY + 6, 20, qc);
        int qnw = MeasureText(qs, 20);
        DrawText("pending", PAD + 14 + qnw + 6, panelY + 12, 11, C_DIM);
        // Fill bar (visual max = 10 slots)
        float frac = (float)s->pendingCount / 10.0f;
        if (frac > 1.0f) frac = 1.0f;
        drawBar(PAD + 14, panelY + 33, bw, 4, frac, C_BORDER, qc);
    }
    sy += 14 + 42 + 10;

    // ── Manual request section (anchored near bottom) ─────────────────────────
    DrawLine(PAD + 4, SCREEN_H - 164, SIDEBAR_W - PAD, SCREEN_H - 164, C_BORDER);
    secLabel("MANUAL REQUEST", PAD + 6, SCREEN_H - 158);

    const char *typeLabel[] = { "NORMAL", "VIP", "EMERGENCY" };
    Color       typeColor[] = { C_TEXT, C_BRAND, C_RED };
    Color       typeBg[]    = {
        C_PANEL2,
        (Color){  8, 50, 70, 255 },
        (Color){ 55, 10,  8, 255 }
    };
    // Type toggle pill with < > arrow hints
    Rectangle typeBtn = { PAD + 4, SCREEN_H - 144, SIDEBAR_W - PAD * 2 + 2, 30 };
    DrawRectangleRounded(typeBtn, 0.25f, 6, typeBg[manualType]);
    DrawRectangle((int)typeBtn.x, (int)typeBtn.y, 3, (int)typeBtn.height,
                  typeColor[manualType]);
    DrawText("<", PAD + 10, SCREEN_H - 136, 13,
             alphaBlend(typeColor[manualType], 0.45f));
    {
        int tw = MeasureText(typeLabel[manualType], 13);
        DrawText(typeLabel[manualType],
                 PAD + 4 + (int)((typeBtn.width - tw) / 2),
                 SCREEN_H - 136, 13, typeColor[manualType]);
    }
    DrawText(">",
             PAD + (int)typeBtn.width - 10, SCREEN_H - 136, 13,
             alphaBlend(typeColor[manualType], 0.45f));

    // Submit button — brand primary style
    DrawRectangleRounded(
        (Rectangle){ PAD + 4, SCREEN_H - 106, SIDEBAR_W - PAD * 2 + 2, 34 },
        0.22f, 6, C_BRAND);
    {
        const char *bt = "SUBMIT  [R]";
        DrawText(bt,
                 PAD + 4 + (SIDEBAR_W - PAD * 2 + 2 - MeasureText(bt, 13)) / 2,
                 SCREEN_H - 96, 13, C_DARK);
    }

    // End simulation button
    DrawRectangleRounded(END_BTN, 0.20f, 6, (Color){ 160, 24, 24, 255 });
    DrawRectangle((int)END_BTN.x, (int)END_BTN.y,
                  3, (int)END_BTN.height, (Color){ 255, 70, 50, 255 });
    {
        const char *et = "END SIMULATION  [E]";
        DrawText(et,
                 PAD + 6 + (SIDEBAR_W - PAD * 2 - MeasureText(et, 12)) / 2,
                 (int)(END_BTN.y + 12), 12, RAYWHITE);
    }

    // ════════════════════════════════════════════════════
    //  MAIN CONTENT AREA
    // ════════════════════════════════════════════════════

    int mx = SIDEBAR_W + PAD;
    int mw = SCREEN_W - SIDEBAR_W - PAD * 2;

    // ── Top header bar ────────────────────────────────────────────────────────
    DrawRectangle(SIDEBAR_W, 0, SCREEN_W - SIDEBAR_W, 36, C_DARK);
    DrawRectangle(SIDEBAR_W, 34, SCREEN_W - SIDEBAR_W, 2, C_BRAND);
    DrawText("FLEET STATUS", mx + 4, 10, 13, (Color){ 150, 200, 215, 255 });
    // Right-aligned live counters
    {
        const char *hdr = TextFormat("TICK %d   |   %d DRIVERS   |   %d ACTIVE",
                                     s->tick, s->numDrivers, s->activeRides);
        DrawText(hdr, SCREEN_W - PAD - MeasureText(hdr, 12), 11, 12,
                 (Color){ 90, 150, 175, 255 });
    }

    // ── Driver card grid ─────────────────────────────────────────────────────
    int gridTop = 44;
    for (int i = 0; i < s->numDrivers; i++) {
        int col = i % DRIVER_COLS;
        int row = i / DRIVER_COLS;
        int cx  = mx + col * (CARD_W + CARD_GAP);
        int cy  = gridTop + row * (CARD_H + CARD_GAP);

        int busy   = (s->drivers[i].status == DRIVER_BUSY);
        int isPlus = (s->drivers[i].category == DRIVER_PLUS);

        // Card background — slightly tinted when busy
        Color cardBg = busy ? (Color){ 185, 225, 232, 255 } : C_PANEL;
        if (driverFlash[i] > 0.0f) {
            Color tint = busy ? (Color){ 155, 215, 226, 255 }
                              : (Color){ 200, 238, 244, 255 };
            cardBg = lerpColor(cardBg, tint, driverFlash[i]);
        }
        DrawRectangleRounded((Rectangle){ cx, cy, CARD_W, CARD_H },
                             0.14f, 6, cardBg);

        // Left accent stripe — PLUS = orange, STD = border grey
        Color accentCol = isPlus ? C_ORANGE : C_BORDER;
        DrawRectangle(cx, cy, 3, CARD_H, accentCol);

        // Status indicator dot (top-right)
        DrawCircle(cx + CARD_W - 11, cy + 12, 5, busy ? C_RED : C_GREEN);

        // Driver ID
        DrawText(TextFormat("#%02d", s->drivers[i].id),
                 cx + 10, cy + 7, 18, C_DARK);

        // Category badge
        if (isPlus) {
            DrawRectangleRounded((Rectangle){ cx + 10, cy + 30, 36, 14 },
                                 0.4f, 4, C_ORANGE);
            DrawText("PLUS", cx + 13, cy + 33, 10, RAYWHITE);
        } else {
            DrawText("STD", cx + 10, cy + 33, 11, C_DIM);
        }

        // Request info or idle label
        if (busy) {
            DrawText(TextFormat("Req #%d", s->drivers[i].currentRequestId),
                     cx + 10, cy + 54, 13, C_BRAND);
            // Green bottom progress bar to indicate active trip
            DrawRectangle(cx + 1, cy + CARD_H - 4, CARD_W - 2, 3, C_GREEN);
        } else {
            DrawText("IDLE", cx + 10, cy + 56, 12, C_DIM);
        }

        // Flash outline (fades out over time)
        if (driverFlash[i] > 0.0f) {
            Color outline = busy ? C_GREEN : C_BLUE;
            outline.a = (unsigned char)(220 * driverFlash[i]);
            DrawRectangleLinesEx((Rectangle){ cx, cy, CARD_W, CARD_H }, 2, outline);
        }
    }

    // ── Stats strip ──────────────────────────────────────────────────────────
    int driverRows = (s->numDrivers + DRIVER_COLS - 1) / DRIVER_COLS;
    int statsY = gridTop + driverRows * (CARD_H + CARD_GAP) + 6;

    struct {
        const char *label;
        char        val[32];
        Color       col;
    } stats[4] = {
        { "COMPLETED",  "", C_GREEN  },
        { "CANCELLED",  "", C_RED    },
        { "PENDING",    "", C_ORANGE },
        { "UTILIZATION","", C_BRAND  },
    };
    snprintf(stats[0].val, 32, "%d",    s->metrics.totalCompleted);
    snprintf(stats[1].val, 32, "%d",    s->metrics.totalCancelled);
    snprintf(stats[2].val, 32, "%d",    s->pendingCount);
    snprintf(stats[3].val, 32, "%.0f%%",s->metrics.driverUtilization * 100.0f);

    int statW = (mw - PAD * 3) / 4;
    for (int i = 0; i < 4; i++) {
        int sx2 = mx + i * (statW + PAD);
        DrawRectangle(sx2, statsY,     statW, 38, C_PANEL);
        DrawRectangle(sx2, statsY,     statW,  3, stats[i].col);  // top accent
        DrawText(stats[i].label, sx2 + 8, statsY + 8,  10, C_DIM);
        DrawText(stats[i].val,   sx2 + 8, statsY + 20, 14, stats[i].col);
    }

    int divY = statsY + 50;
    DrawLine(mx, divY, SCREEN_W - PAD, divY, C_BORDER);

    // ── Bottom split: queue | feed ────────────────────────────────────────────
    int botY = divY + 8;
    int botH = SCREEN_H - botY - PAD;
    int qW   = mw / 2 - PAD / 2;
    int fX   = mx + qW + PAD;
    int fW   = mw - qW - PAD;

    // ── Pending queue ─────────────────────────────────────────────────────────
    // Section label + count badge
    secLabel("PENDING QUEUE", mx, botY);
    if (s->pendingCount > 0) {
        int badgeX = mx + MeasureText("PENDING QUEUE", 11) + 6;
        Color bc = (s->pendingCount >= 6) ? C_RED
                 : (s->pendingCount >= 3) ? C_ORANGE : C_BRAND;
        DrawRectangleRounded((Rectangle){ (float)badgeX, (float)(botY - 1), 20, 13 },
                             0.4f, 4, bc);
        const char *bnum = TextFormat("%d", s->pendingCount);
        DrawText(bnum,
                 badgeX + (20 - MeasureText(bnum, 9)) / 2,
                 botY + 1, 9, RAYWHITE);
    }

    DrawRectangle(mx, botY + 14, qW, botH - 14, C_PANEL);

    if (s->pendingCount == 0) {
        const char *em = "Queue empty";
        DrawText(em,
                 mx + (qW - MeasureText(em, 13)) / 2,
                 botY + 14 + (botH - 14) / 2 - 7, 13, C_DIM);
    } else {
        int qy = botY + 20;
        // Column header row
        DrawRectangle(mx, qy - 2, qW, 18, C_PANEL2);
        DrawText("ID",   mx + 12,  qy, 11, C_BRAND);
        DrawText("TYPE", mx + 62,  qy, 11, C_BRAND);
        DrawText("FARE", mx + 155, qy, 11, C_BRAND);
        DrawText("WAIT", mx + 220, qy, 11, C_BRAND);
        DrawLine(mx, qy + 16, mx + qW, qy + 16, C_BORDER);

        int rowH = 22;
        for (int i = 0; i < s->pendingCount && i < pendingMaxRows; i++) {
            int ry = botY + 40 + i * rowH;

            // Alternating row tint
            if (i % 2 == 0)
                DrawRectangle(mx, ry - 1, qW, rowH, C_PANEL2);

            // Priority left-edge accent
            Color priCol =
                (s->pendingRequests[i].type == EMERGENCY) ? C_RED   :
                (s->pendingRequests[i].type == VIP)       ? C_BRAND : C_BLUE;
            DrawRectangle(mx, ry - 1, 3, rowH, priCol);

            // Type badge pill
            const char *ts =
                (s->pendingRequests[i].type == EMERGENCY) ? "EMER" :
                (s->pendingRequests[i].type == VIP)       ? "VIP"  : "NORM";
            DrawText(TextFormat("#%d", s->pendingRequests[i].id),
                     mx + 12, ry + 3, 13, C_TEXT);
            drawPill((Rectangle){ (float)(mx + 57), (float)(ry + 1),
                                  44, 16 }, ts, priCol, RAYWHITE, 10);
            DrawText(TextFormat("$%d", s->pendingRequests[i].fare),
                     mx + 155, ry + 3, 13, C_TEXT);

            // Wait time — colour-coded
            int   wt    = s->pendingRequests[i].waitTicks;
            Color wtCol = (wt > 20) ? C_RED
                        : (wt > 10) ? C_ORANGE : C_DIM;
            DrawText(TextFormat("%dt", wt), mx + 220, ry + 3, 13, wtCol);
        }
        if (s->pendingCount > pendingMaxRows)
            DrawText(TextFormat("  +%d more...", s->pendingCount - pendingMaxRows),
                     mx + 10,
                     botY + 40 + pendingMaxRows * rowH + 2, 11, C_DIM);
    }

    // ── Live activity feed ────────────────────────────────────────────────────
    secLabel("LIVE ACTIVITY", fX, botY);
    DrawRectangle(fX, botY + 14, fW, botH - 14, C_PANEL);

    if (feedCount == 0) {
        const char *em = "Awaiting events...";
        DrawText(em,
                 fX + (fW - MeasureText(em, 13)) / 2,
                 botY + 14 + (botH - 14) / 2 - 7, 13, C_DIM);
    } else {
        int rowH    = 20;
        int maxRows = (botH - 26) / rowH;
        for (int i = 0; i < feedCount && i < maxRows; i++) {
            float fade = 1.0f - (feed[i].age / 25.0f);
            if (fade < 0.15f) fade = 0.15f;
            int fy = botY + 18 + i * rowH;

            // Alternating row tint
            if (i % 2 == 0)
                DrawRectangle(fX, fy - 1, fW, rowH,
                              (Color){ 200, 228, 234, 70 });

            // Newest-entry highlight + left accent bar
            if (i == 0) {
                DrawRectangle(fX, fy - 1, fW, rowH,
                              (Color){ C_BRAND.r, C_BRAND.g, C_BRAND.b, 22 });
                DrawRectangle(fX, fy - 1, 3, rowH, C_BRAND);
            }

            DrawText(feed[i].text, fX + 10, fy + 3, 12,
                     alphaBlend(feed[i].col, fade));
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────
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
    int       manualOnly      = 0;

    bool            generatorStarted = false;
    bool            simulationEnded  = false;
    MetricsSnapshot finalMetrics     = {0};
    pthread_t       generatorTid;

    SharedState state = {0};
    int mainPid = -1;
    resetDashboard();

    #define START_SIM() do {                                                     \
        if (!generatorStarted) {                                                  \
            if (pthread_create(&generatorTid, NULL, generatorLoop, NULL) == 0) { \
                generatorStarted = true;                                          \
                pthread_detach(generatorTid);                                     \
                printf("GUI --- Generator started (%d drivers).\n",              \
                       selectedDrivers);                                           \
            } else {                                                              \
                perror("GUI --- Failed to start generator thread");               \
            }                                                                     \
        }                                                                         \
        resetDashboard();                                                         \
        screen = GUI_DASHBOARD;                                                   \
    } while (0)

    while (!WindowShouldClose()) {
        float   dt      = GetFrameTime();
        Vector2 mouse   = GetMousePosition();
        bool    clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        switch (screen) {

            case GUI_INTRO:
                if (IsKeyPressed(KEY_S))
                    screen = GUI_CONFIG;
                if (clicked && gridValueAt(bg_grid, mouse) == 1)
                    screen = GUI_CONFIG;
                break;

            case GUI_CONFIG:
                if (IsKeyPressed(KEY_M))
                    manualOnly = 1;
                {
                    int keyChoice = 0;
                    if (IsKeyPressed(KEY_ONE))   keyChoice = gridChoiceByIndex(num_drivers_grid, 0);
                    if (IsKeyPressed(KEY_TWO))   keyChoice = gridChoiceByIndex(num_drivers_grid, 1);
                    if (IsKeyPressed(KEY_THREE)) keyChoice = gridChoiceByIndex(num_drivers_grid, 2);
                    if (IsKeyPressed(KEY_FOUR))  keyChoice = gridChoiceByIndex(num_drivers_grid, 3);

                    if (clicked) {
                        int clickChoice = gridValueAt(num_drivers_grid, mouse);
                        if (clickChoice > 0) keyChoice = clickChoice;
                    }

                    if (keyChoice > 0) {
                        selectedDrivers = keyChoice;
                        PipeRequest cfg = {0};
                        cfg.msgType = PIPE_MSG_CONFIG;
                        cfg.configDrivers = selectedDrivers;
                        writePipeRequest(&cfg);

                        if (manualOnly) {
                            resetDashboard();
                            screen = GUI_DASHBOARD;
                        } else {
                            START_SIM();
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
                                     "Driver #%d (%s) -> Req #%d",
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

                for (int i = 0; i < feedCount;   i++) feed[i].age    += dt;
                for (int i = 0; i < MAX_DRIVERS; i++)
                    if (driverFlash[i] > 0.0f) driverFlash[i] -= dt;

                if (clicked) {
                    Rectangle typeBtn = { PAD + 4, SCREEN_H - 144,
                                          SIDEBAR_W - PAD * 2 + 2, 30 };
                    if (CheckCollisionPointRec(mouse, typeBtn))
                        manualType = (manualType + 1) % 3;

                    Rectangle submitBtn = { PAD + 4, SCREEN_H - 106,
                                            SIDEBAR_W - PAD * 2 + 2, 34 };
                    if (CheckCollisionPointRec(mouse, submitBtn))
                        submitManualRequest(&nextManualId, manualType);

                    if (CheckCollisionPointRec(mouse, END_BTN) && !simulationEnded) {
                        simulationEnded = true;
                        finalMetrics    = state.metrics;
                        generatorStop();
                        screen = GUI_METRICS;
                    }
                }

                if (IsKeyPressed(KEY_TAB))
                    manualType = (manualType + 1) % 3;
                if (IsKeyPressed(KEY_R))
                    submitManualRequest(&nextManualId, manualType);
                if (IsKeyPressed(KEY_E) && !simulationEnded) {
                    simulationEnded = true;
                    finalMetrics    = state.metrics;
                    generatorStop();
                    screen = GUI_METRICS;
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
                drawConfigScreen(configTex, configLoaded);
                break;
            case GUI_DASHBOARD:
                if (readSharedState(&state) != 0) {
                    ClearBackground(C_BG);
                    DrawRectangle(0, 0, SCREEN_W, 36, C_DARK);
                    DrawRectangle(0, 34, SCREEN_W, 2, C_BRAND);
                    const char *msg = "CONNECTING TO MAIN SERVER...";
                    DrawText(msg,
                             SCREEN_W / 2 - MeasureText(msg, 18) / 2,
                             SCREEN_H / 2 - 9, 18, C_DIM);
                } else {
                    int pmr = (SCREEN_H - 16 - 26 - 300) / 22;
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
    if (mainPid > 0) kill(mainPid, SIGINT);
    generatorStop();
    if (introLoaded)  UnloadTexture(introTex);
    if (configLoaded) UnloadTexture(configTex);
    CloseWindow();
}