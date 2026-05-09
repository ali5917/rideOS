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

// layout 
#define SCREEN_W 1280
#define SCREEN_H  800
#define SIDEBAR_W 295
#define PAD 16
#define CARD_W 148
#define CARD_H 78
#define CARD_GAP 8
#define DRIVER_COLS 6

// GUI states
typedef enum { 
    GUI_INTRO, 
    GUI_CONFIG, 
    GUI_DASHBOARD, 
    GUI_METRICS,
    GUI_CONTRIBUTORS 
} GuiScreen;

int numDriversGrid[10][9] = {
    {0, 0,  0,  0,  0,  0,  0, 0, 0},
    {0, 0,  0,  0,  0,  0,  0, 0, 0},
    {0, 0,  0,  0,  0,  0,  0, 0, 0},
    {0, 0,  5,  5,  0, 10, 10, 0, 0},
    {0, 0,  5,  5,  0, 10, 10, 0, 0},
    {0, 0,  0,  0,  0,  0,  0, 0, 0},
    {0, 0,  0,  0,  0,  0,  0, 0, 0},
    {0, 0, 15, 15,  0, 20, 20, 0, 0},
    {0, 0, 15, 15,  0, 20, 20, 0, 0},
    {0, 0,  0,  0,  0,  0,  0, 0, 0},
};

// 10 rows, 9 columns
int bgGrid[10][9] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 2, 2, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// brand palette (matching bg.png)
static const Color C_BRAND = {6, 92, 120, 255};
static const Color C_BG = {232, 248, 250, 255};
static const Color C_SIDE = {214, 238, 242, 255};
static const Color C_PANEL = {204, 231, 237, 255};
static const Color C_PANEL2 = {194, 223, 230, 255};
static const Color C_BORDER = {150, 192, 201, 255};
static const Color C_GREEN = {0, 160, 120, 255};
static const Color C_RED = {220, 70, 60, 255};
static const Color C_BLUE = {40, 120, 190, 255};
static const Color C_ORANGE = {230, 145, 60, 255};
static const Color C_TEXT = {14, 50, 68, 255};
static const Color C_DIM = {38, 82, 100, 255};
static const Color C_DARK = {10, 40, 55, 255};
static const Color C_SIDE_HDR = {4, 58, 76, 255};   
static const Color C_SIDE_MID = {8, 72, 96, 255};   
static const Color C_PANEL3 = {188, 218, 226, 255}; 
static const Color C_AMBER = {230, 175, 20, 255};  
static const Color C_RED_TEXT    = {170, 28, 18, 255};  // emergency text
static const Color C_BRAND_TEXT  = {4,   65, 88, 255};  // VIP text
static const Color C_BLUE_TEXT   = {22,  88, 152, 255}; // normal text

// asset paths
#define INTRO_PNG_PATH        "assets/bg.png"
#define CONFIG_PNG_PATH       "assets/selectDrivers.png"
#define CONTRIBUTORS_PNG_PATH "assets/contributors.png"

// end simulation button (sidebar — fixed position from bottom)
static const Rectangle END_BTN = {PAD, SCREEN_H - 56, SIDEBAR_W - PAD * 2, 36};

volatile sig_atomic_t g_knownMainPid = -1;
volatile sig_atomic_t g_sigintReceived = 0;

// activity feed
#define FEED_CAP 22
typedef struct { 
    char text[84]; 
    Color col; 
    float age; 
} FeedEntry;

static FeedEntry feed[FEED_CAP];
static int feedCount = 0;

static void feedPush(const char *msg, Color col) {
    if (feedCount < FEED_CAP) feedCount++;
    for (int i = feedCount - 1; i > 0; i--){
        feed[i] = feed[i - 1];  
    } 

    strncpy(feed[0].text, msg, 83);
    feed[0].text[83] = '\0';
    feed[0].col = col;
    feed[0].age = 0.0f;
}

// Per-driver diffing
typedef struct { 
    int status; 
    int currentRequestId; 
} DriverSnap;

static DriverSnap prevSnap[MAX_DRIVERS];
static int prevDriverCount = 0;
static int prevCancelled   = 0;
static float driverFlash[MAX_DRIVERS];

// driver names mapping
static char driverNames[MAX_DRIVERS][32];
static bool namesLoaded = false;

static void loadDriverNames(void) {
    FILE *f = fopen("request_server/assets/drivers.txt", "r");
    if (!f) {
        for (int i = 0; i < MAX_DRIVERS; i++) {
            snprintf(driverNames[i], 32, "Driver #%d", i + 1);
        }
        return;
    }
    char line[64];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < MAX_DRIVERS) {
        line[strcspn(line, "\r\n")] = 0; // trim newline
        if (strlen(line) > 0) {
            strncpy(driverNames[count], line, 31);
            driverNames[count][31] = '\0';
            count++;
        }
    }
    fclose(f);
    for (int i = count; i < MAX_DRIVERS; i++) {
        snprintf(driverNames[i], 32, "Driver #%d", i + 1);
    }
    namesLoaded = true;
}

static const char* getDriverName(int id) {
    if (id < 1 || id > MAX_DRIVERS) return "Unknown";
    return driverNames[id - 1];
}

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

// filled rounded rectangle with centred text
static void drawPill(Rectangle r, const char *text, Color bg, Color fg, int fs) {
    DrawRectangleRounded(r, 0.35f, 6, bg);
    int tw = MeasureText(text, fs);
    DrawText(text, (int)(r.x + (r.width  - tw) / 2), (int)(r.y + (r.height - fs) / 2), fs, fg);
}

// small caps section label in brand colour
static void secLabel(const char *t, int x, int y) {
    DrawText(t, x, y, 11, C_BRAND);
}

// horizontal progress bar with rounded end-cap on the fill
static void drawBar(int x, int y, int w, int h, float frac, Color track, Color fill) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    DrawRectangle(x, y, w, h, track);
    int fw = (int)(w * frac);
    if (fw > 0) {
        DrawRectangle(x, y, fw, h, fill);
        // small rounded cap at fill edge
        if (fw >= h)
            DrawCircle(x + fw, y + h / 2, h / 2.0f, fill);
    }
}

// sidebar panel block 
// draws a labelled panel card with a coloured left accent, returns new y.
static int sidePanel(int sy, int h, Color accent, const char *label) {
    secLabel(label, PAD + 6, sy);
    sy += 14;
    DrawRectangle(PAD + 4, sy, SIDEBAR_W - PAD * 2 + 2, h, C_PANEL);
    DrawRectangle(PAD + 4, sy, 3, h, accent);
    return sy;
}

static void resetDashboard(void) {
    feedCount = 0;
    memset(feed, 0, sizeof(feed));
    memset(driverFlash, 0, sizeof(driverFlash));
    memset(prevSnap, 0, sizeof(prevSnap));
    prevDriverCount = 0;
    prevCancelled = 0;
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

// intro screen
static void drawIntroScreen(Texture2D tex, bool loaded) {
    if (loaded) {
        DrawTexturePro(tex,
            (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
            (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
}

// config screen 
static void drawConfigScreen(Texture2D tex, bool loaded) {
    if (loaded) {
        DrawTexturePro(tex,
            (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
            (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
}

static void drawMetricsScreen(const MetricsSnapshot *m) {
    ClearBackground(C_BG);

    // header band 
    DrawRectangle(0, 0, SCREEN_W, 72, C_DARK);
    DrawRectangle(0, 70, SCREEN_W, 3, C_BRAND);

    const char *banner = "SIMULATION COMPLETE";
    DrawText(banner, SCREEN_W / 2 - MeasureText(banner, 30) / 2, 20, 30, RAYWHITE);
    const char *sub = "Final Performance Report";
    DrawText(sub, SCREEN_W / 2 - MeasureText(sub, 13) / 2, 90, 13, C_DIM);

    // layout constants 
    // three equal columns centred on screen
    int totalW = 860;
    int gap = 20;
    int colW   = (totalW - gap * 2) / 3; 
    int col1x  = SCREEN_W / 2 - totalW / 2;
    int col2x  = col1x + colW + 16;
    int col3x  = col2x + colW + 16;
    int cardY  = 118;
    int cardH  = 204;

    // draw a section card (rounded rect + top accent + label) 
    #define CARD(cx, label, accentCol) \
        DrawRectangleRounded((Rectangle){ cx, cardY, colW, cardH }, \
                             0.12f, 6, C_PANEL); \
        DrawRectangle(cx, cardY, colW, 3, accentCol); \
        secLabel(label, cx + 10, cardY + 10);

    // column 1 - Overall volumes 
    CARD(col1x, "OVERALL VOLUME", C_BRAND)
    {
        int vy = cardY + 26;
        int vsp = 34;
        int vfs = 22;

        DrawText("CREATED",    col1x + 10, vy,      10, C_DIM);
        DrawText(TextFormat("%d", m->totalCreated),
                              col1x + 10, vy + 12,  vfs, C_TEXT);
        vy += vsp;

        DrawLine(col1x + 8, vy + 2, col1x + colW - 8, vy + 2, C_BORDER);
        vy += 10;

        DrawText("COMPLETED",  col1x + 10, vy,      10, C_DIM);
        DrawText(TextFormat("%d", m->totalCompleted),
                              col1x + 10, vy + 12,  vfs, C_GREEN);
        vy += vsp;

        DrawText("CANCELLED",  col1x + 10, vy,      10, C_DIM);
        DrawText(TextFormat("%d", m->totalCancelled),
                              col1x + 10, vy + 12,  vfs, C_RED);
        vy += vsp;

        // cancel-rate bar
        DrawText("CANCEL RATE", col1x + 10, vy,     10, C_DIM);
        float cr = m->cancellationRate;
        Color crCol = (cr > 0.35f) ? C_RED
                    : (cr > 0.15f) ? C_ORANGE : C_GREEN;
        DrawText(TextFormat("%.1f%%", cr * 100.0f),
                              col1x + 10, vy + 12, 18, crCol);
        int bw = colW - 20;
        DrawRectangle(col1x + 10, vy + 34, bw, 4, C_BORDER);
        DrawRectangle(col1x + 10, vy + 34, (int)(bw * cr), 4, crCol);
    }

    // Column 2: Per-type breakdown
    CARD(col2x, "REQUEST BREAKDOWN", C_BLUE)
    {
        // Three sub-rows: NORMAL / VIP / EMERGENCY
        const char *typeNames[] = { "NORMAL",    "VIP",    "EMERGENCY" };
        Color       typeCols[]  = { C_TEXT,      C_BRAND,  C_RED       };
        int         typeCnt[]   = { m->numRequestsNormal,
                                    m->numRequestsVip,
                                    m->numRequestsEmergency };
        float       typeWait[]  = { m->avgWaitNormal,
                                    m->avgWaitVip,
                                    m->avgWaitEmergency };

        int total = m->numRequestsNormal + m->numRequestsVip
                  + m->numRequestsEmergency;
        int bw = colW - 20;
        int ry = cardY + 26;

        for (int i = 0; i < 3; i++) {
            // Type label + count
            DrawText(typeNames[i], col2x + 10, ry,      10, C_DIM);
            DrawText(TextFormat("%d", typeCnt[i]),
                                col2x + 10, ry + 12, 18, typeCols[i]);

            // Avg wait (right-aligned inside the card)
            const char *ws = TextFormat("%.0fs avg", typeWait[i]);
            DrawText(ws, col2x + colW - MeasureText(ws, 11) - 10,
                     ry + 16, 11, C_DIM);

            // Proportional fill bar
            float frac = (total > 0) ? (float)typeCnt[i] / (float)total : 0.0f;
            int ry2 = ry + 34;
            DrawRectangle(col2x + 10, ry2, bw,               4, C_BORDER);
            DrawRectangle(col2x + 10, ry2, (int)(bw * frac), 4, typeCols[i]);

            ry += 52;
            if (i < 2)
                DrawLine(col2x + 8, ry - 4,
                         col2x + colW - 8, ry - 4, C_BORDER);
        }
    }

    // column 3 - performance 
    CARD(col3x, "PERFORMANCE", C_GREEN)
    {
        int py = cardY + 26;

        // Utilization
        float util = m->driverUtilization;
        Color utilCol = (util > 0.8f) ? C_RED
                      : (util > 0.5f) ? C_ORANGE : C_GREEN;
        DrawText("DRIVER UTILIZATION", col3x + 10, py,      10, C_DIM);
        DrawText(TextFormat("%.1f%%", util * 100.0f),
                             col3x + 10, py + 12, 22, utilCol);
        int bw = colW - 20;
        DrawRectangle(col3x + 10, py + 38, bw,              5, C_BORDER);
        DrawRectangle(col3x + 10, py + 38, (int)(bw * util),5, utilCol);
        py += 54;

        DrawLine(col3x + 8, py, col3x + colW - 8, py, C_BORDER);
        py += 10;

        // Avg wait times (all three in a compact grid)
        DrawText("AVG WAIT TIMES", col3x + 10, py, 10, C_DIM);
        py += 14;
        const char *wl[] = { "NORM", "VIP", "EMER" };
        float       wv[] = { m->avgWaitNormal, m->avgWaitVip, m->avgWaitEmergency };
        Color       wc[] = { C_TEXT, C_BRAND, C_RED };
        int         cw3  = (colW - 20) / 3;
        for (int i = 0; i < 3; i++) {
            int wx = col3x + 10 + i * cw3;
            DrawText(wl[i],                     wx, py,      10, C_DIM);
            DrawText(TextFormat("%.0fs", wv[i]), wx, py + 12, 16, wc[i]);
        }
        py += 34;

        DrawLine(col3x + 8, py, col3x + colW - 8, py, C_BORDER);
        py += 10;

        // surge
        DrawText("SURGE PRICING", col3x + 10, py, 10, C_DIM);
        if (m->surgeActive) {
            DrawRectangleRounded(
                (Rectangle){ col3x + 10, (float)(py + 12), 72, 20 },
                0.3f, 4, C_RED);
            DrawText("ACTIVE", col3x + 17, py + 16, 11, RAYWHITE);
            DrawText(TextFormat("x%.1f", m->surgeMultiplier),
                     col3x + 90, py + 14, 14, C_RED);
        } else {
            DrawText("OFF", col3x + 10, py + 12, 16, C_GREEN);
        }
    }

    #undef CARD

    // divider + footer hint 
    DrawLine(SCREEN_W / 2 - totalW / 2, cardY + cardH + 16, 
             SCREEN_W / 2 + totalW / 2, cardY + cardH + 16, C_BORDER);

    const char *hint = "Press ESC or close the window to exit";
    DrawText(hint,SCREEN_W / 2 - MeasureText(hint, 13) / 2, SCREEN_H - 40, 13, C_DIM);
}

// dashboard 
static void drawDashboard(const SharedState *s, int manualType, int pendingMaxRows) {
    ClearBackground(C_BG);
    // sidebar
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
    // 4 px brand stripe on left edge
    DrawRectangle(0, 0, 4, SCREEN_H, C_BRAND);
    // Separator below header
    DrawRectangle(0, 88, SIDEBAR_W, 2, C_BRAND);
    // Rest of sidebar background
    DrawRectangle(4, 90, SIDEBAR_W - 4, SCREEN_H - 90, C_SIDE);
    // Right border
    DrawRectangle(SIDEBAR_W - 1, 0, 1, SCREEN_H, C_BORDER);

    // Brand Heading
    DrawText("RideOS", PAD + 8, 16, 30, RAYWHITE);
    DrawText("DISPATCH", PAD + 8,  54, 11, (Color){ 160, 210, 225, 255 });
    DrawText("DASHBOARD", PAD + 8 + MeasureText("DISPATCH", 11) + 6, 56, 10, (Color){ 220, 190, 50, 200 });

    int sy = 100; // running y cursor inside sidebar

    // surge badge 
    if (s->metrics.surgeActive) {
        DrawRectangle(PAD + 4, sy, SIDEBAR_W - PAD * 2 + 2, 30, C_RED);
        DrawRectangle(PAD + 4, sy, 4, 30, C_AMBER);
        const char *sm = "!! SURGE PRICING ACTIVE !!";
        DrawText(sm,
                 PAD + 4 + (SIDEBAR_W - PAD * 2 + 2 - MeasureText(sm, 11)) / 2,
                 sy + 10, 11, RAYWHITE);
        sy += 38;
    }

    // system panel 
    int panelY = sidePanel(sy, 60, C_BRAND, "SYSTEM");
    DrawText("TICK", PAD + 14, panelY + 8,  10, C_DIM);
    DrawText(TextFormat("%d", s->tick), PAD + 52, panelY + 6,  16, C_TEXT);
    DrawCircle(PAD + 14, panelY + 42, 5, s->activeRides > 0 ? C_GREEN : C_DIM);
    DrawText(TextFormat("%d active ride%s", s->activeRides, s->activeRides == 1 ? "" : "s"),
             PAD + 24, panelY + 36, 13, C_TEXT);
    sy += 14 + 60 + 10;

    // performance panel
    panelY = sidePanel(sy, 112, C_BRAND, "PERFORMANCE");
    {
        float util = s->metrics.driverUtilization;
        int bw = SIDEBAR_W - PAD * 2 - 22;
        Color utilCol = (util > 0.8f) ? C_RED : (util > 0.5f) ? C_ORANGE : C_GREEN;
        DrawText("UTILIZATION", PAD + 14, panelY + 8,  10, C_DIM);
        // Right-aligned percentage
        const char *pct = TextFormat("%.0f%%", util * 100.0f);
        DrawText(pct, PAD + 4 + SIDEBAR_W - PAD * 2 - MeasureText(pct, 16), panelY + 6, 16, utilCol);
        drawBar(PAD + 14, panelY + 30, bw, 6, util, C_BORDER, utilCol);

        DrawLine(PAD + 14, panelY + 46, PAD + 14 + bw, panelY + 46, C_BORDER);

        // Completed / Cancelled in two columns
        DrawText("DONE", PAD + 14, panelY + 54, 10, C_DIM);
        DrawText("CANCL", PAD + 14 + bw/2, panelY + 54, 10, C_DIM);
        DrawText(TextFormat("%d", s->metrics.totalCompleted), PAD + 14, panelY + 66, 18, C_GREEN);
        DrawText(TextFormat("%d", s->metrics.totalCancelled), PAD + 14 + bw/2, panelY + 66, 18, C_RED);

        // Cancel-rate sub-label
        DrawText(TextFormat("%.0f%% cancel rate", s->metrics.cancellationRate * 100.0f), 
                 PAD + 14, panelY + 92, 10, C_DIM);
    }
    sy += 14 + 112 + 10;

    // avg wait panel 
    panelY = sidePanel(sy, 72, C_BLUE, "AVG WAIT TIMES");
    {
        int bw = SIDEBAR_W - PAD * 2 - 22;
        const char *wLabels[] = { "NORM", "VIP",  "EMER" };
        Color wColors[] = { C_TEXT, C_BRAND_TEXT, C_RED_TEXT };
        float wTimes[]  = {
            s->metrics.avgWaitNormal,
            s->metrics.avgWaitVip,
            s->metrics.avgWaitEmergency
        };
        int colW = bw / 3;
        for (int i = 0; i < 3; i++) {
            int wx = PAD + 14 + i * colW;
            DrawText(wLabels[i], wx, panelY + 7,  10, C_DIM);
            DrawText(TextFormat("%.0fs", wTimes[i]), wx, panelY + 21, 15, wColors[i]);
            float frac = wTimes[i] / 60.0f;
            if (frac > 1.0f) frac = 1.0f;
            drawBar(wx, panelY + 46, colW - 4, 4, frac, C_BORDER, wColors[i]);
        }
    }
    sy += 14 + 72 + 10;

    // queue depth panel 
    panelY = sidePanel(sy, 42, (s->pendingCount >= 6) ? C_RED : (s->pendingCount >= 3) ? C_ORANGE 
                     : C_GREEN, "QUEUE DEPTH");
    {
        Color qc = (s->pendingCount >= 6) ? C_RED : (s->pendingCount >= 3) ? C_ORANGE : C_GREEN;
        int bw = SIDEBAR_W - PAD * 2 - 22;
        const char *qs = TextFormat("%d", s->pendingCount);
        DrawText(qs, PAD + 14, panelY + 6, 20, qc);
        int qnw = MeasureText(qs, 20);
        DrawText("pending", PAD + 14 + qnw + 6, panelY + 12, 11, C_DIM);
        // fill bar (visual max = 10 slots)
        float frac = (float)s->pendingCount / 10.0f;
        if (frac > 1.0f) frac = 1.0f;
        drawBar(PAD + 14, panelY + 33, bw, 4, frac, C_BORDER, qc);
    }
    sy += 14 + 42 + 10;

    // manual request section (anchored near bottom)
    DrawLine(PAD + 4, SCREEN_H - 164, SIDEBAR_W - PAD, SCREEN_H - 164, C_BORDER);
    secLabel("MANUAL REQUEST", PAD + 6, SCREEN_H - 158);

    const char *typeLabel[] = { "NORMAL", "VIP", "EMERGENCY" };
    Color typeColor[] = {C_TEXT, C_BG, C_RED};
    Color typeBg[]    = {
        C_PANEL2,
        (Color){  8, 50, 70, 255 },
        (Color){ 55, 10,  8, 255 }
    };
    // type toggle pill with < > arrow hints
    Rectangle typeBtn = {PAD + 4, SCREEN_H - 144, SIDEBAR_W - PAD * 2 + 2, 30};
    DrawRectangleRounded(typeBtn, 0.25f, 6, typeBg[manualType]);
    DrawRectangle((int)typeBtn.x, (int)typeBtn.y, 3, (int)typeBtn.height, typeColor[manualType]);
    DrawText("<", PAD + 10, SCREEN_H - 136, 13, alphaBlend(typeColor[manualType], 0.45f));
    {
        int tw = MeasureText(typeLabel[manualType], 13);
        DrawText(typeLabel[manualType], PAD + 4 + (int)((typeBtn.width - tw) / 2),
                 SCREEN_H - 136, 13, typeColor[manualType]);
    }
    DrawText(">", PAD + (int)typeBtn.width - 10, SCREEN_H - 136, 13, alphaBlend(typeColor[manualType], 0.45f));

    // submit button
    DrawRectangleRounded((Rectangle){ PAD + 4, SCREEN_H - 106, SIDEBAR_W - PAD * 2 + 2, 34 },
        0.22f, 6, C_BRAND);
    {
        const char *bt = "SUBMIT  [R]";
        DrawText(bt, PAD + 4 + (SIDEBAR_W - PAD * 2 + 2 - MeasureText(bt, 13)) / 2,
                 SCREEN_H - 96, 13, C_BG);
    }

    // end simulation button
    DrawRectangleRounded(END_BTN, 0.20f, 6, (Color){ 160, 24, 24, 255 });
    DrawRectangle((int)END_BTN.x, (int)END_BTN.y,
                  3, (int)END_BTN.height, (Color){ 255, 70, 50, 255 });
    {
        const char *et = "END SIMULATION  [E]";
        DrawText(et,
         (int)(END_BTN.x + (END_BTN.width - MeasureText(et, 12)) / 2),
         (int)(END_BTN.y + 12), 12, RAYWHITE);
    }

    //  main content area
    int mx = SIDEBAR_W + PAD;
    int mw = SCREEN_W - SIDEBAR_W - PAD * 2;

    // top header bar
    DrawRectangle(SIDEBAR_W, 0, SCREEN_W - SIDEBAR_W, 36, C_DARK);
    DrawRectangle(SIDEBAR_W, 34, SCREEN_W - SIDEBAR_W, 2, C_BRAND);
    DrawText("DRIVERS FLEET STATUS", mx + 4, 10, 13, (Color){ 150, 200, 215, 255 });
    // Right-aligned live counters
    {
        const char *hdr = TextFormat("TICK %d   |   %d DRIVERS   |   %d ACTIVE", 
                                     s->tick, s->numDrivers, s->activeRides);
        DrawText(hdr, SCREEN_W - PAD - MeasureText(hdr, 12), 11, 12, (Color){ 90, 150, 175, 255 });
    }

    // driver card grid
    int gridTop = 44;
    for (int i = 0; i < s->numDrivers; i++) {
        int col = i % DRIVER_COLS;
        int row = i / DRIVER_COLS;
        int cx = mx + col * (CARD_W + CARD_GAP);
        int cy = gridTop + row * (CARD_H + CARD_GAP);

        int busy   = (s->drivers[i].status == DRIVER_BUSY);
        int isPlus = (s->drivers[i].category == DRIVER_PLUS);

        // card background — slightly tinted when busy
        Color cardBg = busy ? (Color){ 185, 225, 232, 255 } : C_PANEL;
        if (driverFlash[i] > 0.0f) {
            Color tint = busy ? (Color){ 155, 215, 226, 255 } : (Color){ 200, 238, 244, 255 };
            cardBg = lerpColor(cardBg, tint, driverFlash[i]);
        }
        DrawRectangleRounded((Rectangle){ cx, cy, CARD_W, CARD_H }, 0.14f, 6, cardBg);

        // left accent stripe — PLUS = orange, STD = border grey
        Color accentCol = isPlus ? C_ORANGE : C_BORDER;
        DrawRectangle(cx, cy, 3, CARD_H, accentCol);

        // status indicator dot (top-right)
        DrawCircle(cx + CARD_W - 11, cy + 12, 5, busy ? C_RED : C_GREEN);

        // driver name (or ID if not found)
        const char *dname = getDriverName(s->drivers[i].id);
        DrawText(dname, cx + 10, cy + 7, 16, C_DARK);

        // category badge
        if (isPlus) {
            DrawRectangleRounded((Rectangle){ cx + 10, cy + 30, 36, 14 }, 0.4f, 4, C_ORANGE);
            DrawText("PLUS", cx + 13, cy + 33, 10, RAYWHITE);
        } else {
            DrawText("STD", cx + 10, cy + 33, 11, C_TEXT);
        }

        // request info or idle label
        if (busy) {
            DrawText(TextFormat("Req #%d", s->drivers[i].currentRequestId), cx + 10, cy + 54, 13, C_BRAND);
            // green bottom progress bar to indicate active trip
            DrawRectangle(cx + 1, cy + CARD_H - 4, CARD_W - 2, 3, C_GREEN);
        } else {
            DrawText("IDLE", cx + 10, cy + 56, 12, C_TEXT);
        }

        // flash outline (fades out over time)
        if (driverFlash[i] > 0.0f) {
            Color outline = busy ? C_GREEN : C_BLUE;
            outline.a = (unsigned char)(220 * driverFlash[i]);
            DrawRectangleLinesEx((Rectangle){ cx, cy, CARD_W, CARD_H }, 2, outline);
        }
    }

    // Stats strip 
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
    snprintf(stats[0].val, 32, "%d", s->metrics.totalCompleted);
    snprintf(stats[1].val, 32, "%d", s->metrics.totalCancelled);
    snprintf(stats[2].val, 32, "%d", s->pendingCount);
    snprintf(stats[3].val, 32, "%.0f%%", s->metrics.driverUtilization * 100.0f);

    int statW = (mw - PAD * 3) / 4;
    for (int i = 0; i < 4; i++) {
        int sx2 = mx + i * (statW + PAD);
        DrawRectangle(sx2, statsY, statW, 38, C_PANEL);
        DrawRectangle(sx2, statsY, statW,  3, stats[i].col);  // top accent
        DrawText(stats[i].label, sx2 + 8, statsY + 8,  10, C_TEXT);
        DrawText(stats[i].val, sx2 + 8, statsY + 20, 14, stats[i].col);
    }

    int divY = statsY + 50;
    DrawLine(mx, divY, SCREEN_W - PAD, divY, C_BORDER);

    // bottom split: queue | feed 
    int botY = divY + 8;
    int botH = SCREEN_H - botY - PAD;
    int qW   = mw / 2 - PAD / 2;
    int fX   = mx + qW + PAD;
    int fW   = mw - qW - PAD;

    // pending queue 
    // section label + count badge
    secLabel("PENDING QUEUE", mx, botY);
    if (s->pendingCount > 0) {
        int badgeX = mx + MeasureText("PENDING QUEUE", 11) + 6;
        Color bc = (s->pendingCount >= 6) ? C_RED : (s->pendingCount >= 3) ? C_ORANGE : C_BRAND;
        DrawRectangleRounded((Rectangle){ (float)badgeX, (float)(botY - 1), 20, 13 }, 0.4f, 4, bc);
        const char *bnum = TextFormat("%d", s->pendingCount);
        DrawText(bnum, badgeX + (20 - MeasureText(bnum, 9)) / 2, botY + 1, 9, RAYWHITE);
    }

    DrawRectangle(mx, botY + 14, qW, botH - 14, C_PANEL);

    if (s->pendingCount == 0) {
        const char *em = "Queue empty";
        DrawText(em, mx + (qW - MeasureText(em, 13)) / 2, botY + 14 + (botH - 14) / 2 - 7, 13, C_DIM);
    } else {
        int qy = botY + 20;
        // column header row
        DrawRectangle(mx, qy - 2, qW, 18, C_PANEL2);
        DrawText("ID",   mx + 12,  qy, 11, C_BRAND);
        DrawText("TYPE", mx + 62,  qy, 11, C_BRAND);
        DrawText("FARE", mx + 155, qy, 11, C_BRAND);
        DrawText("WAIT", mx + 220, qy, 11, C_BRAND);
        DrawLine(mx, qy + 16, mx + qW, qy + 16, C_BORDER);

        int rowH = 22;
        for (int i = 0; i < s->pendingCount && i < pendingMaxRows; i++) {
            int ry = botY + 40 + i * rowH;

            // alternating row tint
            if (i % 2 == 0)
                DrawRectangle(mx, ry - 1, qW, rowH, C_PANEL2);

            // priority left-edge accent
            Color priCol =
                (s->pendingRequests[i].type == EMERGENCY) ? C_RED   :
                (s->pendingRequests[i].type == VIP) ? C_BRAND : C_BLUE;
            DrawRectangle(mx, ry - 1, 3, rowH, priCol);

            // type badge pill
            const char *ts =
                (s->pendingRequests[i].type == EMERGENCY) ? "EMER" :
                (s->pendingRequests[i].type == VIP) ? "VIP"  : "NORM";
            DrawText(TextFormat("#%d", s->pendingRequests[i].id), mx + 12, ry + 3, 13, C_TEXT);
            drawPill((Rectangle){ (float)(mx + 57), (float)(ry + 1), 44, 16 }, ts, priCol, RAYWHITE, 10);
            DrawText(TextFormat("$%d", s->pendingRequests[i].fare), mx + 155, ry + 3, 13, C_TEXT);

            // wait time — colour-coded
            int wt = s->pendingRequests[i].waitTicks;
            Color wtCol = (wt > 20) ? C_RED_TEXT : (wt > 10) ? C_ORANGE : C_TEXT;
            DrawText(TextFormat("%dt", wt), mx + 220, ry + 3, 13, wtCol);
        }
        if (s->pendingCount > pendingMaxRows)
            DrawText(TextFormat("  +%d more...", s->pendingCount - pendingMaxRows), 
                mx + 10, botY + 40 + pendingMaxRows * rowH + 2, 11, C_DIM);
    }

    // live activity feed
    secLabel("LIVE ACTIVITY", fX, botY);
    DrawRectangle(fX, botY + 14, fW, botH - 14, C_PANEL);

    if (feedCount == 0) {
        const char *em = "Awaiting events...";
        DrawText(em, fX + (fW - MeasureText(em, 13)) / 2, botY + 14 + (botH - 14) / 2 - 7, 13, C_DIM);
    } else {
        int rowH    = 20;
        int maxRows = (botH - 26) / rowH;
        for (int i = 0; i < feedCount && i < maxRows; i++) {
            float fade = 1.0f - (feed[i].age / 25.0f);
            if (fade < 0.15f) fade = 0.15f;
            int fy = botY + 18 + i * rowH;

            // alternating row tint
            if (i % 2 == 0)
                DrawRectangle(fX, fy - 1, fW, rowH,
                              (Color){ 200, 228, 234, 70 });

            // newest-entry highlight + left accent bar
            if (i == 0) {
                DrawRectangle(fX, fy - 1, fW, rowH, (Color){ C_BRAND.r, C_BRAND.g, C_BRAND.b, 22 });
                DrawRectangle(fX, fy - 1, 3, rowH, C_BRAND);
            }

            DrawText(feed[i].text, fX + 10, fy + 3, 12, alphaBlend(feed[i].col, fade));
        }
    }
}

// main gui loop 
void runGui() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "RideOS | Dispatch Dashboard");
    SetTargetFPS(60);

    Texture2D introTex = {0};
    Texture2D configTex = {0};
    Texture2D contribTex = {0};
    bool introLoaded = false;
    bool configLoaded = false;
    bool contribLoaded = false;

    if (FileExists(INTRO_PNG_PATH)) {
        introTex = LoadTexture(INTRO_PNG_PATH);
        introLoaded = true;
    } else {
        printf("GUI --- intro PNG not found at '%s', using fallback.\n", INTRO_PNG_PATH);
    }
    if (FileExists(CONFIG_PNG_PATH)) {
        configTex = LoadTexture(CONFIG_PNG_PATH);
        configLoaded = true;
    } else {
        printf("GUI --- config PNG not found at '%s', using fallback.\n", CONFIG_PNG_PATH);
    }

    if (FileExists(CONTRIBUTORS_PNG_PATH)) {
        contribTex = LoadTexture(CONTRIBUTORS_PNG_PATH);
        contribLoaded = true;
    }

    loadDriverNames();

    GuiScreen screen = GUI_INTRO;
    int selectedDrivers = 10;
    int nextManualId = 9000;
    int manualType = 0;
    int manualOnly = 0;

    bool generatorStarted = false;
    bool simulationEnded = false;
    MetricsSnapshot finalMetrics = {0};
    pthread_t generatorTid;

    SharedState state = {0};
    int mainPid = -1;
    resetDashboard();

    #define START_SIM() do {                                                      \
        if (!generatorStarted) {                                                  \
            if (pthread_create(&generatorTid, NULL, generatorLoop, NULL) == 0) {  \
                generatorStarted = true;                                          \
                pthread_detach(generatorTid);                                     \
                printf("GUI --- Generator started (%d drivers).\n",               \
                       selectedDrivers);                                          \
            } else {                                                              \
                perror("GUI --- Failed to start generator thread");               \
            }                                                                     \
        }                                                                         \
        resetDashboard();                                                         \
        screen = GUI_DASHBOARD;                                                   \
    } while (0)

    while (!WindowShouldClose()) {
        if (g_sigintReceived) goto cleanup;
        float dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        switch (screen) {
            case GUI_INTRO:
                if (IsKeyPressed(KEY_S))
                    screen = GUI_CONFIG;
                if (clicked) {
                    int val = gridValueAt(bgGrid, mouse);
                    if (val == 1) screen = GUI_CONFIG;
                    if (val == 2) {
                        screen = GUI_CONTRIBUTORS;
                    }
                }
                break;

            case GUI_CONFIG:
                if (IsKeyPressed(KEY_M))
                    manualOnly = 1;
                {
                    int keyChoice = 0;
                    if (IsKeyPressed(KEY_ONE)) keyChoice = gridChoiceByIndex(numDriversGrid, 0);
                    if (IsKeyPressed(KEY_TWO)) keyChoice = gridChoiceByIndex(numDriversGrid, 1);
                    if (IsKeyPressed(KEY_THREE)) keyChoice = gridChoiceByIndex(numDriversGrid, 2);
                    if (IsKeyPressed(KEY_FOUR)) keyChoice = gridChoiceByIndex(numDriversGrid, 3);

                    if (clicked) {
                        int clickChoice = gridValueAt(numDriversGrid, mouse);
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
                if (IsKeyPressed(KEY_ESCAPE)) screen = GUI_INTRO;
                break;

            case GUI_DASHBOARD: {
                // check shutdown flag from main server
                SharedState tmp;
                if (readSharedState(&tmp) == 0 && tmp.shutdownFlag && !simulationEnded) {
                    if (tmp.mainPid > 0) {
                        mainPid = tmp.mainPid;
                        g_knownMainPid = tmp.mainPid;
                    }
                    simulationEnded = true;
                    finalMetrics = tmp.metrics;
                    generatorStop();
                    screen = GUI_METRICS;
                    break;
                }

                // state diffing → feed events
                if (readSharedState(&state) == 0) {
                    if (state.mainPid > 0) {
                        mainPid = state.mainPid;
                        g_knownMainPid = tmp.mainPid;
                    }
                    for (int i = prevDriverCount; i < state.numDrivers; i++) {
                        prevSnap[i].status           = DRIVER_ONLINE;
                        prevSnap[i].currentRequestId = -1;
                    }
                    for (int i = 0; i < state.numDrivers && i < MAX_DRIVERS; i++) {
                        int wasBusy = (prevSnap[i].status == DRIVER_BUSY);
                        int isBusy  = (state.drivers[i].status == DRIVER_BUSY);

                        if (!wasBusy && isBusy) {
                            const char *cat = (state.drivers[i].category == DRIVER_PLUS) ? "PLUS" : "STD";
                            char buf[84];
                            snprintf(buf, sizeof(buf), "%s (%s) -> Req #%d", 
                                    getDriverName(state.drivers[i].id), cat, state.drivers[i].currentRequestId);
                            feedPush(buf, C_GREEN);
                            driverFlash[i] = 1.0f;
                        } else if (wasBusy && !isBusy) {
                            char buf[84];
                            snprintf(buf, sizeof(buf), "%s completed ride", getDriverName(state.drivers[i].id));
                            feedPush(buf, C_BLUE);
                            driverFlash[i] = 0.6f;
                        }
                        prevSnap[i].status = state.drivers[i].status;
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

                for (int i = 0; i < feedCount; i++) {
                    feed[i].age    += dt;
                }

                for (int i = 0; i < MAX_DRIVERS; i++) {
                    if (driverFlash[i] > 0.0f) driverFlash[i] -= dt;
                }

                if (clicked) {
                    Rectangle typeBtn = { PAD + 4, SCREEN_H - 144,
                                          SIDEBAR_W - PAD * 2 + 2, 30 };
                    if (CheckCollisionPointRec(mouse, typeBtn)) manualType = (manualType + 1) % 3;

                    Rectangle submitBtn = { PAD + 4, SCREEN_H - 106, SIDEBAR_W - PAD * 2 + 2, 34 };
                    if (CheckCollisionPointRec(mouse, submitBtn)) {
                        submitManualRequest(&nextManualId, manualType);
                    }

                    if (CheckCollisionPointRec(mouse, END_BTN) && !simulationEnded) {
                        simulationEnded = true;
                        finalMetrics    = state.metrics;
                        generatorStop();
                        screen = GUI_METRICS;
                    }
                }

                if (IsKeyPressed(KEY_TAB)) manualType = (manualType + 1) % 3;
                if (IsKeyPressed(KEY_R)) submitManualRequest(&nextManualId, manualType);
                if (IsKeyPressed(KEY_E) && !simulationEnded) {
                    simulationEnded = true;
                    finalMetrics    = state.metrics;
                    generatorStop();
                    screen = GUI_METRICS;
                }
                break;
            }
            case GUI_CONTRIBUTORS:
                if (clicked || IsKeyPressed(KEY_B))
                    screen = GUI_INTRO;
                break;

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
            case GUI_CONTRIBUTORS:
                if (contribLoaded) {
                    DrawTexturePro(contribTex,
                        (Rectangle){ 0, 0, (float)contribTex.width, (float)contribTex.height },
                        (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
                        (Vector2){ 0, 0 }, 0.0f, WHITE);
                } else {
                    ClearBackground(C_BRAND);
                    DrawText("CONTRIBUTORS", 100, 100, 40, RAYWHITE);
                    DrawText("Click anywhere to go back", 100, 160, 20, C_BG);
                }
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
    if (contribLoaded) UnloadTexture(contribTex);
    CloseWindow();
}