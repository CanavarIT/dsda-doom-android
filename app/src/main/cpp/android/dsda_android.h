#ifndef DSDA_ANDROID_H
#define DSDA_ANDROID_H

#include <SDL.h>
#include "doomtype.h"

/* Called first thing in main(): SDL hints, working directory, environment. */
void dsda_android_PreInit(void);
/* Called right after the config file was loaded (applies first-run defaults). */
void dsda_android_ApplyDefaults(void);
/* Directory with demos ("/sdcard/DSDA DEMO" or an app-private fallback). */
const char *dsda_android_BaseDir(void);
/* Directory for config / saves / dsda_doom_data. */
const char *dsda_android_DataDir(void);

/* Event hook for I_GetEvent. Returns true when the event was consumed. */
dboolean dsda_android_HandleEvent(const SDL_Event *ev);
/* Per-frame hook (I_StartTic): emits look / stick / key events to the game. */
void dsda_android_Frame(void);
/* Draws the touch controls on top of the finished frame. */
void dsda_android_DrawOverlay(SDL_Renderer *renderer);

/* Finger position (0..1) used by the menu's mouse code. */
void dsda_android_GetPointer(float *nx, float *ny);

void dsda_android_Log(int level, const char *msg);
void dsda_android_ShowError(const char *msg);

#endif
