#ifndef GUI_H
#define GUI_H
#include <signal.h>
extern volatile sig_atomic_t g_knownMainPid;
extern volatile sig_atomic_t g_sigintReceived;
/**
 * runGui - Starts the Raylib window and enters the main rendering loop.
 * This function blocks until the window is closed.
 */
void runGui();

#endif
