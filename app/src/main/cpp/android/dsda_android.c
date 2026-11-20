/*
 * DSDA-Doom for Android: touch controls, paths, logging.
 *
 * Controls (landscape):
 *   - left side touch      analog stick appears under the thumb and stays there
 *   - drag anywhere else   look; dragging on the fire disc also looks while shooting
 *   - big orange disc      fire (hold)
 *   - small ring           use
 *   - < >                  previous / next weapon
 *   - three bars (top)     menu            - small square: automap
 * Menus: no D-pad. Tap an item to activate it, drag a finger up / down to scroll
 * (sliders are dragged in place). Only a Back button is shown; Y / N appear when the
 * game asks a yes/no question, and the soft keyboard opens by itself whenever the game
 * wants text (save name, console, string / number option).
 */
#include "config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "g_game.h"
#include "i_video.h"
#include "i_system.h"
#include "i_main.h"
#include "am_map.h"
#include "lprintf.h"
#include "m_file.h"
#include "dsda/configuration.h"
#include "dsda/input.h"
#include "dsda/excmd.h"
#include "dsda/settings.h"
#include "m_menu.h"
#include "m_misc.h"

#include "dsda_android.h"

/* ------------------------------------------------------------------ paths */

static char base_dir[512];
static char data_dir[512];

static int dir_writable(const char *path)
{
  char probe[640];
  FILE *f;
  struct stat st;

  if (stat(path, &st) != 0)
  {
    if (mkdir(path, 0777) != 0 && errno != EEXIST)
      return 0;
  }
  snprintf(probe, sizeof(probe), "%s/.dsda_probe", path);
  f = fopen(probe, "wb");
  if (!f)
    return 0;
  fclose(f);
  remove(probe);
  return 1;
}

static void init_paths(void)
{
  const char *env;

  if (base_dir[0])
    return;

  env = getenv("DSDA_BASE_DIR");           /* set by the Java launcher */
  if (env && *env && dir_writable(env))
  {
    snprintf(base_dir, sizeof(base_dir), "%s", env);
  }
  else
  {
#ifdef __ANDROID__
    const char *fallback = SDL_AndroidGetExternalStoragePath();
    if (!fallback)
      fallback = SDL_AndroidGetInternalStoragePath();
#else
    const char *fallback = "/tmp/dsda";
#endif
    snprintf(base_dir, sizeof(base_dir), "%s", fallback ? fallback : "/data/local/tmp");
    mkdir(base_dir, 0777);
  }

  snprintf(data_dir, sizeof(data_dir), "%s/data", base_dir);
  mkdir(data_dir, 0777);
}

const char *dsda_android_BaseDir(void) { init_paths(); return base_dir; }
const char *dsda_android_DataDir(void) { init_paths(); return data_dir; }

/* ---------------------------------------------------------------- logging */

void dsda_android_Log(int level, const char *msg)
{
#ifdef __ANDROID__
  int prio = (level & LO_ERROR) ? ANDROID_LOG_ERROR
           : (level & LO_WARN)  ? ANDROID_LOG_WARN
           : (level & LO_DEBUG) ? ANDROID_LOG_DEBUG : ANDROID_LOG_INFO;
  __android_log_write(prio, "DSDA", msg);
#else
  (void) level; (void) msg;
#endif
}

void dsda_android_ShowError(const char *msg)
{
  char path[640];
  FILE *f;

  init_paths();
  snprintf(path, sizeof(path), "%s/dsda_error.log", data_dir);
  f = fopen(path, "ab");
  if (f)
  {
    fprintf(f, "%s\n", msg);
    fclose(f);
  }
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DSDA-Doom", msg, NULL);
}

/* ------------------------------------------------------- init / defaults */

/* First launch: write phone-friendly defaults into dsda-doom.cfg *before* the game
 * loads it (changing options after start-up would trigger video re-initialisation). */
static void seed_config(void)
{
  char path[640];
  struct stat st;
  FILE *f;

  snprintf(path, sizeof(path), "%s/dsda-doom.cfg", data_dir);
  if (stat(path, &st) == 0)
    return;
  f = fopen(path, "wb");
  if (!f)
    return;
  fputs("# Seeded by DSDA-Doom Android\n"
        "use_fullscreen            1\n"
        "use_game_controller       1\n"
        "render_vsync              1\n"
        "videomode                 \"Software\"\n"
        "snd_midiplayer            \"opl\"\n", f);
  fclose(f);
}

static void parse_layout(const char *str);
static float touch_sens = 1.0f;
static int   show_ui = 1;

void dsda_android_PreInit(void)
{
  const char *e;

  init_paths();
  if (chdir(data_dir) != 0)
    chdir(base_dir);

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  seed_config();

  e = getenv("DSDA_TOUCH_SENS");
  if (e && *e)
  {
    touch_sens = (float) atof(e);
    if (touch_sens < 0.1f) touch_sens = 0.1f;
    if (touch_sens > 5.0f) touch_sens = 5.0f;
  }
  parse_layout(getenv("DSDA_TOUCH_LAYOUT"));
  e = getenv("DSDA_TOUCH_UI");
  if (e && *e == '0')
    show_ui = 0;
}

#define KEY_NEXT_WEAPON KEYD_KEYPAD6
#define KEY_PREV_WEAPON KEYD_KEYPAD4

void dsda_android_ApplyDefaults(void)
{
  /* extra key bindings used by the touch buttons (weapon cycle) */
  if (!dsda_InputMatchKey(dsda_input_nextweapon, KEY_NEXT_WEAPON))
    dsda_InputAddKey(dsda_input_nextweapon, KEY_NEXT_WEAPON);
  if (!dsda_InputMatchKey(dsda_input_prevweapon, KEY_PREV_WEAPON))
    dsda_InputAddKey(dsda_input_prevweapon, KEY_PREV_WEAPON);

  /* touch menus go through the engine's mouse-navigation code */
  if (!dsda_IntConfig(dsda_config_mouse_menu_navigation))
    dsda_UpdateIntConfig(dsda_config_mouse_menu_navigation, 1, true);
}

/* ------------------------------------------------------------- UI state */

enum
{
  B_FIRE, B_USE, B_NEXT, B_PREV, B_MENU, B_MAP, B_ZOOMIN, B_ZOOMOUT,
  B_UP, B_DOWN, B_LEFT, B_RIGHT, B_OK, B_BACK, B_YES, B_NO, B_KBD,
  B_COUNT
};

typedef struct
{
  float x, y, r;
  int visible;
  int down;          /* number of fingers holding it */
  int sent;          /* key down has been delivered to the game */
  Uint32 sent_at;    /* SDL ticks when delivered */
  Uint32 last_rep;
  unsigned frame_sent;
} button_t;

enum { MODE_GAME, MODE_MENU, MODE_SCREEN };
enum { R_NONE, R_STICK, R_LOOK, R_BTN, R_TAP, R_MENU };

#define MAX_FINGERS 10

typedef struct
{
  int active;
  SDL_FingerID id;
  int role;
  int btn;
  float x, y;
  float x0, y0;      /* where the touch started (menus) */
  float acc;         /* vertical drag not yet turned into scroll steps */
  int moved;         /* menu touch became a scroll gesture */
  int slider;        /* menu touch is dragging a slider */
  int consumed;      /* long press already acted: no click / scroll on release */
  Uint32 t0;         /* touch start time */
} finger_t;

static button_t btn[B_COUNT];
static finger_t fingers[MAX_FINGERS];
static int cur_mode = MODE_GAME;
static unsigned frame_no;

static float out_w = 1920, out_h = 1080, unit = 10.8f;

/* stick */
static int   stick_finger = -1;
static float stick_cx, stick_cy, stick_px, stick_py;
static int   stick_was_active;
static float stick_ox, stick_oy;

/* look */
static float look_dx, look_dy, look_cx, look_cy;

/* menu state, refreshed by layout() */
static int  menu_confirm;          /* game asks Y/N */
static int  menu_text;             /* game wants text input */
static char menu_text_buf[160];
static int  text_active;           /* we started SDL text input */

/* pointer (normalised 0..1) used by the engine's mouse-menu code */
static float ptr_nx = 0.5f, ptr_ny = 0.5f;
static int   mouse_down;
static unsigned mouse_down_frame;

void dsda_android_GetPointer(float *nx, float *ny)
{
  *nx = ptr_nx;
  *ny = ptr_ny;
}

/* ---- user layout (set in the launcher's "Edit Touch") ----
 * DSDA_TOUCH_LAYOUT = "FIRE=nx,ny,scale;USE=...;STICK=..." ; nx / ny are fractions of the
 * screen, scale multiplies the default size. Buttons that are not listed keep defaults. */
typedef struct { int has; float nx, ny, scale; } ov_t;
static ov_t ov[B_COUNT];
static ov_t ov_stick;

static void parse_layout(const char *str)
{
  static const struct { const char *name; int b; } names[] = {
    { "FIRE", B_FIRE }, { "USE", B_USE }, { "NEXT", B_NEXT }, { "PREV", B_PREV },
    { "MENU", B_MENU }, { "MAP", B_MAP }, { "STICK", -1 }
  };
  char buf[512];
  char *tok, *save = NULL;
  size_t i;

  if (!str || !*str)
    return;
  snprintf(buf, sizeof(buf), "%s", str);
  for (tok = strtok_r(buf, ";", &save); tok; tok = strtok_r(NULL, ";", &save))
  {
    char *eq = strchr(tok, '=');
    float nx, ny, sc;
    if (!eq)
      continue;
    *eq = 0;
    if (sscanf(eq + 1, "%f,%f,%f", &nx, &ny, &sc) != 3)
      continue;
    if (nx < 0) nx = 0; if (nx > 1) nx = 1;
    if (ny < 0) ny = 0; if (ny > 1) ny = 1;
    if (sc < 0.4f) sc = 0.4f; if (sc > 2.5f) sc = 2.5f;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      if (!strcmp(tok, names[i].name))
      {
        ov_t *o = names[i].b < 0 ? &ov_stick : &ov[names[i].b];
        o->has = 1; o->nx = nx; o->ny = ny; o->scale = sc;
      }
  }
}

static button_t place(int b, float x, float y, float r)
{
  button_t t;
  if (ov[b].has)
  {
    x = ov[b].nx * out_w;
    y = ov[b].ny * out_h;
    r *= ov[b].scale;
  }
  memset(&t, 0, sizeof(t));
  t.x = x; t.y = y; t.r = r; t.visible = 1;
  return t;
}

static float stick_radius(void)
{
  return 13.0f * unit * (ov_stick.has ? ov_stick.scale : 1.0f);
}
#define STICK_R() stick_radius()

static void stick_home(float *cx, float *cy)
{
  if (ov_stick.has)
  {
    *cx = ov_stick.nx * out_w;
    *cy = ov_stick.ny * out_h;
    return;
  }
  *cx = 17.0f * unit;
  *cy = out_h - 23.0f * unit;
}

static int mode_now(void)
{
  if (menuactive != mnact_inactive)
    return MODE_MENU;
  if (gamestate == GS_LEVEL)
    return MODE_GAME;
  return MODE_SCREEN;
}

static void post_key(int key, dboolean down)
{
  event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = down ? ev_keydown : ev_keyup;
  ev.data1.i = key;
  D_PostEvent(&ev);
}

static void post_mouse_buttons(int buttons)
{
  event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ev_mouse;
  ev.data1.i = buttons;
  D_PostEvent(&ev);
}

static void post_mouse_motion(void)
{
  event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ev_mousemotion;
  D_PostEvent(&ev);
}

static void set_pointer(float x, float y)
{
  ptr_nx = out_w > 0 ? x / out_w : 0.5f;
  ptr_ny = out_h > 0 ? y / out_h : 0.5f;
}

static void mouse_release(void)
{
  if (mouse_down)
  {
    post_mouse_buttons(0);
    mouse_down = 0;
  }
}

static void mouse_press(void)
{
  mouse_release();
  post_mouse_buttons(1);
  mouse_down = 1;
  mouse_down_frame = frame_no;
}

static void wheel_step(int up)
{
  int k = up ? KEYD_MWHEELUP : KEYD_MWHEELDOWN;
  post_key(k, true);
  post_key(k, false);
}

static int bound_key(int id, int fallback)
{
  dsda_input_t *in = dsda_Input(id);
  if (in && in->key && in->num_keys > 0 && in->key[0] > 0)
    return in->key[0];
  return fallback;
}

static int button_key(int b)
{
  switch (b)
  {
    case B_FIRE:    return bound_key(dsda_input_fire, KEYD_RCTRL);
    case B_USE:     return bound_key(dsda_input_use, ' ');
    case B_NEXT:    return KEY_NEXT_WEAPON;
    case B_PREV:    return KEY_PREV_WEAPON;
    case B_MENU:    return KEYD_ESCAPE;
    case B_MAP:     return bound_key(dsda_input_map, KEYD_TAB);
    case B_ZOOMIN:  return bound_key(dsda_input_map_zoomin, '=');
    case B_ZOOMOUT: return bound_key(dsda_input_map_zoomout, '-');
    case B_UP:      return bound_key(dsda_input_menu_up, KEYD_UPARROW);
    case B_DOWN:    return bound_key(dsda_input_menu_down, KEYD_DOWNARROW);
    case B_LEFT:    return bound_key(dsda_input_menu_left, KEYD_LEFTARROW);
    case B_RIGHT:   return bound_key(dsda_input_menu_right, KEYD_RIGHTARROW);
    case B_OK:      return KEYD_ENTER;
    case B_BACK:    return KEYD_ESCAPE;
    case B_YES:     return 'y';
    case B_NO:      return 'n';
    default:        return 0;
  }
}

static int button_repeats(int b)
{
  return b == B_UP || b == B_DOWN || b == B_LEFT || b == B_RIGHT;
}

static void layout(void)
{
  int i;
  float w = out_w, h = out_h, u = unit;
  for (i = 0; i < B_COUNT; i++)
    btn[i].visible = 0;

  if (cur_mode == MODE_MENU)
  {
    /* menus are touched directly; only what cannot be done by touch is shown */
    menu_confirm = M_AndroidNeedsConfirmation() ? 1 : 0;
    menu_text = M_AndroidTextInput(menu_text_buf, sizeof(menu_text_buf)) ? 1 : 0;
  }
  else
  {
    menu_confirm = menu_text = 0;
  }

  if (!show_ui && cur_mode != MODE_MENU)
    return;

  if (cur_mode == MODE_GAME)
  {
    btn[B_FIRE]  = place(B_FIRE, w - 17 * u, h - 22 * u, 9.5f * u);
    btn[B_USE]   = place(B_USE,  w - 35 * u, h - 13 * u, 6.0f * u);
    btn[B_NEXT]  = place(B_NEXT, w - 11 * u, h - 44 * u, 4.6f * u);
    btn[B_PREV]  = place(B_PREV, w - 27 * u, h - 40 * u, 4.6f * u);
    btn[B_MENU]  = place(B_MENU, w - 6.5f * u, 6.5f * u, 4.4f * u);
    btn[B_MAP]   = place(B_MAP,  w - 18 * u, 6.5f * u, 4.4f * u);
    if (automap_on)   /* zoom buttons sit next to the automap button wherever it is */
    {
      btn[B_ZOOMIN]  = (button_t){ btn[B_MAP].x - 12 * u, btn[B_MAP].y, 4.4f * u, 1 };
      btn[B_ZOOMOUT] = (button_t){ btn[B_MAP].x - 24 * u, btn[B_MAP].y, 4.4f * u, 1 };
    }
  }
  else if (cur_mode == MODE_MENU)
  {
    btn[B_BACK] = (button_t){ w - 6.5f * u, 6.5f * u, 4.4f * u, 1 };
    if (menu_text)      /* bring the keyboard back if it was hidden */
      btn[B_KBD] = (button_t){ w - 18 * u, 6.5f * u, 4.4f * u, 1 };
    if (menu_confirm)
    {
      btn[B_NO]  = (button_t){ w * 0.5f - 14 * u, h - 17 * u, 7.5f * u, 1 };
      btn[B_YES] = (button_t){ w * 0.5f + 14 * u, h - 17 * u, 7.5f * u, 1 };
    }
  }
  else
  {
    btn[B_MENU]  = place(B_MENU, w - 6.5f * u, 6.5f * u, 4.4f * u);
  }
}

/* keep the dynamic fields while re-laying out */
static void layout_keep(void)
{
  button_t old[B_COUNT];
  int i;
  memcpy(old, btn, sizeof(btn));
  layout();
  for (i = 0; i < B_COUNT; i++)
  {
    btn[i].down = old[i].down;
    btn[i].sent = old[i].sent;
    btn[i].sent_at = old[i].sent_at;
    btn[i].last_rep = old[i].last_rep;
    btn[i].frame_sent = old[i].frame_sent;
  }
}

static void release_everything(void)
{
  int i;

  for (i = 0; i < MAX_FINGERS; i++)
    fingers[i].active = 0;
  for (i = 0; i < B_COUNT; i++)
    btn[i].down = 0;           /* Frame() delivers the key-ups */
  stick_finger = -1;
  look_dx = look_dy = 0;
}

static int hit_button(float x, float y)
{
  int i, best = -1;
  float bestd = 1e9f;

  for (i = 0; i < B_COUNT; i++)
  {
    float dx, dy, d, lim;
    if (!btn[i].visible)
      continue;
    dx = x - btn[i].x;
    dy = y - btn[i].y;
    d = sqrtf(dx * dx + dy * dy);
    lim = btn[i].r * 1.3f;
    if (d <= lim && d < bestd)
    {
      bestd = d;
      best = i;
    }
  }
  return best;
}

static finger_t *find_finger(SDL_FingerID id)
{
  int i;
  for (i = 0; i < MAX_FINGERS; i++)
    if (fingers[i].active && fingers[i].id == id)
      return &fingers[i];
  return NULL;
}

static finger_t *new_finger(SDL_FingerID id)
{
  int i;
  for (i = 0; i < MAX_FINGERS; i++)
    if (!fingers[i].active)
    {
      memset(&fingers[i], 0, sizeof(fingers[i]));
      fingers[i].active = 1;
      fingers[i].id = id;
      return &fingers[i];
    }
  return NULL;
}

static void tap_key(int key)
{
  post_key(key, true);
  post_key(key, false);
}

/* Intermission / finale: the game only looks at the USE/FIRE state of the current
 * tic, so a tap must be held for a couple of tics (70 ms) before it is released. */
static int    hold_key;
static Uint32 hold_until;
static unsigned hold_frame;

static void screen_tap(void)
{
  if (hold_key)
    return;                               /* previous tap is still being delivered */
  hold_key = button_key(B_USE);
  post_key(hold_key, true);
  tap_key(KEYD_ENTER);                    /* text screens advance on any key */
  hold_until = SDL_GetTicks() + 70;
  hold_frame = frame_no;
}

static void update_output_size(void)
{
  SDL_Renderer *r = (SDL_Renderer *) I_GetSDLRenderer();
  int w = 0, h = 0;
  if (r && SDL_GetRendererOutputSize(r, &w, &h) == 0 && w > 0 && h > 0)
  {
    out_w = (float) w;
    out_h = (float) h;
    unit = out_h / 100.0f;
  }
}

static void finger_down(const SDL_TouchFingerEvent *tf)
{
  finger_t *f;
  float x, y;
  int b;

  update_output_size();
  x = tf->x * out_w;
  y = tf->y * out_h;

  f = new_finger(tf->fingerId);
  if (!f)
    return;
  f->x = x;
  f->y = y;

  cur_mode = mode_now();
  layout_keep();

  b = hit_button(x, y);
  if (b >= 0)
  {
    f->role = R_BTN;
    f->btn = b;
    btn[b].down++;
    if (b == B_KBD)
    {
      /* re-open the keyboard (it can be hidden with the system back gesture) */
      SDL_StopTextInput();
      SDL_StartTextInput();
      text_active = 1;
    }
    return;
  }

  if (cur_mode == MODE_MENU)
  {
    f->role = R_MENU;
    f->x0 = x;
    f->y0 = y;
    f->acc = 0;
    f->moved = 0;
    f->slider = 0;
    f->consumed = 0;
    f->t0 = SDL_GetTicks();
    if (!menu_confirm)           /* Y/N questions are answered with the buttons only */
    {
      set_pointer(x, y);
      if (M_AndroidSliderAtPointer())
      {
        f->slider = 1;           /* drag the slider in place */
        mouse_press();
      }
      else
      {
        post_mouse_motion();     /* hover: highlights the item under the finger */
      }
    }
    return;
  }

  if (cur_mode == MODE_GAME && show_ui)
  {
    /* left part of the screen: the stick appears where the thumb lands and then
       stays put (the knob moves, the centre does not) */
    float hx, hy;
    int near_home = 0;
    stick_home(&hx, &hy);
    if (ov_stick.has)    /* stick moved by the user: also accept touches around its new spot */
      near_home = hypotf(x - hx, y - hy) < STICK_R() * 1.6f;
    if (stick_finger < 0 && (x < out_w * 0.45f || near_home))
    {
      float R = STICK_R(), cx = x, cy = y;
      if (cx < R) cx = R;
      if (cy < R) cy = R;
      if (cy > out_h - R) cy = out_h - R;
      f->role = R_STICK;
      stick_finger = (int) (f - fingers);
      stick_cx = cx;
      stick_cy = cy;
      stick_ox = cx - x;     /* centre was nudged away from screen edges */
      stick_oy = cy - y;
      stick_px = cx;
      stick_py = cy;
      return;
    }
    f->role = R_LOOK;
    return;
  }
  if (cur_mode == MODE_GAME)
  {
    f->role = R_LOOK;
    return;
  }

  /* non-gameplay screens: any tap continues / opens the menu */
  if (cur_mode == MODE_SCREEN)
  {
    f->role = R_TAP;
    if (gamestate == GS_DEMOSCREEN)
      tap_key(KEYD_ESCAPE);
    else
      screen_tap();
  }
}

static void finger_motion(const SDL_TouchFingerEvent *tf)
{
  finger_t *f = find_finger(tf->fingerId);
  float x, y;

  if (!f)
    return;
  x = tf->x * out_w;
  y = tf->y * out_h;

  /* a finger holding FIRE can also drag to aim while shooting */
  if (f->role == R_LOOK || (f->role == R_BTN && f->btn == B_FIRE && cur_mode == MODE_GAME))
  {
    look_dx += tf->dx * out_w;
    look_dy += tf->dy * out_h;
  }
  else if (f->role == R_STICK)
  {
    stick_px = x + stick_ox;
    stick_py = y + stick_oy;
  }
  else if (f->role == R_MENU && !menu_confirm)
  {
    if (f->slider)
    {
      set_pointer(x, y);
      post_mouse_motion();
    }
    else if (!f->consumed)
    {
      float dx0 = x - f->x0, dy0 = y - f->y0;
      float step = M_AndroidSetupActive() ? 4.6f * unit : 7.5f * unit;

      if (!f->moved && dx0 * dx0 + dy0 * dy0 > (2.4f * unit) * (2.4f * unit))
      {
        f->moved = 1;            /* it is a scroll, not a tap */
        f->acc = 0;
      }
      else if (f->moved)
      {
        f->acc += y - f->y;
        /* the highlight follows the finger: drag down -> goes down, drag up -> goes up */
        while (f->acc >= step)  { wheel_step(0); f->acc -= step; }
        while (f->acc <= -step) { wheel_step(1); f->acc += step; }
      }
    }
  }
  f->x = x;
  f->y = y;
}

static void finger_up(const SDL_TouchFingerEvent *tf)
{
  finger_t *f = find_finger(tf->fingerId);

  if (!f)
    return;
  if (f->role == R_BTN)
  {
    if (btn[f->btn].down > 0)
      btn[f->btn].down--;
  }
  else if (f->role == R_STICK)
  {
    stick_finger = -1;
  }
  else if (f->role == R_MENU && !menu_confirm)
  {
    if (f->slider)
    {
      set_pointer(tf->x * out_w, tf->y * out_h);
      post_mouse_motion();
      mouse_release();
    }
    else if (!f->moved && !f->consumed)
    {
      set_pointer(f->x0, f->y0);   /* a tap: click where the finger landed */
      mouse_press();
    }
  }
  f->active = 0;
}

/* ----------------------------------------------------------- event hook */

dboolean dsda_android_HandleEvent(const SDL_Event *ev)
{
  switch (ev->type)
  {
    case SDL_FINGERDOWN:   finger_down(&ev->tfinger);   return true;
    case SDL_FINGERMOTION: finger_motion(&ev->tfinger); return true;
    case SDL_FINGERUP:     finger_up(&ev->tfinger);     return true;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
      /* hardware / gesture Back button acts as Escape */
      if (ev->key.keysym.sym == SDLK_AC_BACK)
      {
        if (ev->type == SDL_KEYDOWN && !ev->key.repeat)
          tap_key(KEYD_ESCAPE);
        return true;
      }
      return false;

    case SDL_APP_WILLENTERBACKGROUND:
      release_everything();
      mouse_release();
      M_SaveDefaults();       /* options survive the app being killed in the background */
      /* pause: open the menu if we are in the middle of a level */
      if (gamestate == GS_LEVEL && menuactive == mnact_inactive && !demoplayback)
        tap_key(KEYD_ESCAPE);
      return true;

    case SDL_APP_DIDENTERFOREGROUND:
      return true;

    case SDL_QUIT:
      /* Activity is being destroyed: leave through the normal exit path so
         a demo that is being recorded gets written to disk. */
      I_SafeExit(0);
      return true;

    default:
      return false;
  }
}

/* ------------------------------------------------------------ per frame */

void dsda_android_Frame(void)
{
  int i;
  Uint32 now = SDL_GetTicks();
  int m = mode_now();

  frame_no++;

  if (m != cur_mode)
  {
    cur_mode = m;
    release_everything();
  }
  layout_keep();

  /* the soft keyboard follows the game: open while text is wanted, closed otherwise */
  if (cur_mode == MODE_MENU && menu_text)
  {
    if (!text_active)
    {
      SDL_StartTextInput();
      text_active = 1;
    }
  }
  else if (text_active)
  {
    SDL_StopTextInput();
    text_active = 0;
  }

  if (mouse_down && frame_no > mouse_down_frame)
    mouse_release();

  /* long press on a save slot -> delete it (the game then asks Y / N) */
  if (cur_mode == MODE_MENU && !menu_confirm && M_AndroidInSaveLoadMenu())
  {
    for (i = 0; i < MAX_FINGERS; i++)
    {
      finger_t *f = &fingers[i];
      if (f->active && f->role == R_MENU && !f->moved && !f->slider && !f->consumed &&
          now - f->t0 > 650)
      {
        f->consumed = 1;
        tap_key(bound_key(dsda_input_menu_clear, KEYD_DEL));
      }
    }
  }

  if (hold_key && now >= hold_until && frame_no > hold_frame)
  {
    post_key(hold_key, false);
    hold_key = 0;
  }

  /* buttons: deliver key down / up, with a one-frame minimum hold */
  for (i = 0; i < B_COUNT; i++)
  {
    int key = button_key(i);

    if (!key)
      continue;

    if (btn[i].down && !btn[i].sent)
    {
      post_key(key, true);
      btn[i].sent = 1;
      btn[i].sent_at = btn[i].last_rep = now;
      btn[i].frame_sent = frame_no;
    }
    else if (btn[i].down && btn[i].sent && button_repeats(i))
    {
      if (now - btn[i].sent_at > 380 && now - btn[i].last_rep > 90)
      {
        post_key(key, true);
        btn[i].last_rep = now;
      }
    }
    else if (!btn[i].down && btn[i].sent && frame_no > btn[i].frame_sent)
    {
      post_key(key, false);
      btn[i].sent = 0;
    }
  }

  if (cur_mode != MODE_GAME)
  {
    look_dx = look_dy = 0;
    return;
  }

  /* stick -> analog movement */
  if (stick_finger >= 0 && fingers[stick_finger].active)
  {
    float R = STICK_R();
    float dx, dy, len;
    float nx = 0, ny = 0, mag;
    event_t ev;

    /* fixed stick: the centre never moves, the knob is clamped to the ring */
    dx = stick_px - stick_cx;
    dy = stick_py - stick_cy;
    len = sqrtf(dx * dx + dy * dy);
    if (len > R)
    {
      dx *= R / len;
      dy *= R / len;
      len = R;
    }

    mag = len / R;
    if (mag > 0.10f)
    {
      float s = (mag - 0.10f) / 0.90f;
      if (s > 1.0f) s = 1.0f;
      nx = dx / len * s;
      ny = dy / len * s;
      /* axis snapping makes straight runs easy (useful for demos) */
      if (fabsf(nx) < 0.22f * fabsf(ny)) nx = 0;
      if (fabsf(ny) < 0.22f * fabsf(nx)) ny = 0;
    }

    memset(&ev, 0, sizeof(ev));
    ev.type = ev_move_analog;
    ev.data1.f = nx * 50.0f;     /* strafe right */
    ev.data2.f = -ny * 50.0f;    /* forward */
    if (ev.data1.f != 0 || ev.data2.f != 0)
      D_PostEvent(&ev);
    stick_was_active = 1;
  }
  else
  {
    stick_was_active = 0;
  }

  /* look -> mouse motion */
  if (look_dx != 0 || look_dy != 0)
  {
    static float cx, cy;
    event_t ev;
    int ix, iy;

    cx += look_dx * (3600.0f / out_w) * touch_sens;
    look_dx = 0;
    if (dsda_FreeAim())
    {
      cy += look_dy * (2000.0f / out_h) * touch_sens;
    }
    look_dy = 0;

    ix = (int) cx;
    iy = (int) cy;
    cx -= ix;
    cy -= iy;
    if (ix || iy)
    {
      memset(&ev, 0, sizeof(ev));
      ev.type = ev_mousemotion;
      ev.data1.i = ix;
      ev.data2.i = iy;
      D_PostEvent(&ev);
    }
  }
}

/* -------------------------------------------------------------- drawing */

#define SEG 40

static void set_blend(SDL_Renderer *r)
{
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}

static void geom(SDL_Renderer *r, const SDL_Vertex *v, int nv, const int *idx, int ni)
{
  SDL_RenderGeometry(r, NULL, v, nv, idx, ni);
}

static void disc(SDL_Renderer *r, float cx, float cy, float rad, SDL_Color c)
{
  SDL_Vertex v[SEG + 2];
  int idx[SEG * 3];
  int i;

  v[0].position.x = cx; v[0].position.y = cy; v[0].color = c;
  v[0].tex_coord.x = v[0].tex_coord.y = 0;
  for (i = 0; i <= SEG; i++)
  {
    float a = (float) i / SEG * 6.2831853f;
    v[i + 1].position.x = cx + cosf(a) * rad;
    v[i + 1].position.y = cy + sinf(a) * rad;
    v[i + 1].color = c;
    v[i + 1].tex_coord.x = v[i + 1].tex_coord.y = 0;
  }
  for (i = 0; i < SEG; i++)
  {
    idx[i * 3] = 0;
    idx[i * 3 + 1] = i + 1;
    idx[i * 3 + 2] = i + 2;
  }
  geom(r, v, SEG + 2, idx, SEG * 3);
}

static void ring(SDL_Renderer *r, float cx, float cy, float rad, float th, SDL_Color c)
{
  SDL_Vertex v[(SEG + 1) * 2];
  int idx[SEG * 6];
  int i;

  for (i = 0; i <= SEG; i++)
  {
    float a = (float) i / SEG * 6.2831853f;
    float ca = cosf(a), sa = sinf(a);
    v[i * 2].position.x = cx + ca * rad;
    v[i * 2].position.y = cy + sa * rad;
    v[i * 2 + 1].position.x = cx + ca * (rad - th);
    v[i * 2 + 1].position.y = cy + sa * (rad - th);
    v[i * 2].color = v[i * 2 + 1].color = c;
    v[i * 2].tex_coord.x = v[i * 2].tex_coord.y = 0;
    v[i * 2 + 1].tex_coord.x = v[i * 2 + 1].tex_coord.y = 0;
  }
  for (i = 0; i < SEG; i++)
  {
    int a = i * 2;
    idx[i * 6 + 0] = a;     idx[i * 6 + 1] = a + 1; idx[i * 6 + 2] = a + 2;
    idx[i * 6 + 3] = a + 1; idx[i * 6 + 4] = a + 3; idx[i * 6 + 5] = a + 2;
  }
  geom(r, v, (SEG + 1) * 2, idx, SEG * 6);
}

static void line(SDL_Renderer *r, float x0, float y0, float x1, float y1, float th, SDL_Color c)
{
  SDL_Vertex v[4];
  int idx[6] = { 0, 1, 2, 1, 3, 2 };
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy), nx, ny;
  int i;

  if (len < 0.001f)
    return;
  nx = -dy / len * th * 0.5f;
  ny = dx / len * th * 0.5f;
  v[0].position.x = x0 + nx; v[0].position.y = y0 + ny;
  v[1].position.x = x0 - nx; v[1].position.y = y0 - ny;
  v[2].position.x = x1 + nx; v[2].position.y = y1 + ny;
  v[3].position.x = x1 - nx; v[3].position.y = y1 - ny;
  for (i = 0; i < 4; i++)
  {
    v[i].color = c;
    v[i].tex_coord.x = v[i].tex_coord.y = 0;
  }
  geom(r, v, 4, idx, 6);
}

static void triangle(SDL_Renderer *r, float x0, float y0, float x1, float y1, float x2, float y2, SDL_Color c)
{
  SDL_Vertex v[3];
  int idx[3] = { 0, 1, 2 };
  int i;
  v[0].position.x = x0; v[0].position.y = y0;
  v[1].position.x = x1; v[1].position.y = y1;
  v[2].position.x = x2; v[2].position.y = y2;
  for (i = 0; i < 3; i++)
  {
    v[i].color = c;
    v[i].tex_coord.x = v[i].tex_coord.y = 0;
  }
  geom(r, v, 3, idx, 3);
}

static SDL_Color col(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
  SDL_Color c = { r, g, b, a };
  return c;
}

/* glyphs, k = half-size of the glyph */
static void glyph(SDL_Renderer *r, int b, float cx, float cy, float k, float th, SDL_Color c)
{
  switch (b)
  {
    case B_UP:    triangle(r, cx, cy - k, cx - k, cy + k * 0.7f, cx + k, cy + k * 0.7f, c); break;
    case B_DOWN:  triangle(r, cx, cy + k, cx - k, cy - k * 0.7f, cx + k, cy - k * 0.7f, c); break;
    case B_LEFT:  triangle(r, cx - k, cy, cx + k * 0.7f, cy - k, cx + k * 0.7f, cy + k, c); break;
    case B_RIGHT: triangle(r, cx + k, cy, cx - k * 0.7f, cy - k, cx - k * 0.7f, cy + k, c); break;
    case B_NEXT:
      line(r, cx - k * 0.4f, cy - k, cx + k * 0.5f, cy, th, c);
      line(r, cx + k * 0.5f, cy, cx - k * 0.4f, cy + k, th, c);
      break;
    case B_PREV:
      line(r, cx + k * 0.4f, cy - k, cx - k * 0.5f, cy, th, c);
      line(r, cx - k * 0.5f, cy, cx + k * 0.4f, cy + k, th, c);
      break;
    case B_MENU:
      line(r, cx - k, cy - k * 0.6f, cx + k, cy - k * 0.6f, th, c);
      line(r, cx - k, cy, cx + k, cy, th, c);
      line(r, cx - k, cy + k * 0.6f, cx + k, cy + k * 0.6f, th, c);
      break;
    case B_MAP:
      line(r, cx, cy - k, cx + k, cy, th, c);
      line(r, cx + k, cy, cx, cy + k, th, c);
      line(r, cx, cy + k, cx - k, cy, th, c);
      line(r, cx - k, cy, cx, cy - k, th, c);
      break;
    case B_ZOOMIN:
      line(r, cx - k, cy, cx + k, cy, th, c);
      line(r, cx, cy - k, cx, cy + k, th, c);
      break;
    case B_ZOOMOUT:
      line(r, cx - k, cy, cx + k, cy, th, c);
      break;
    case B_USE:
      ring(r, cx, cy, k * 0.9f, th, c);
      disc(r, cx, cy, k * 0.28f, c);
      break;
    case B_FIRE:
      disc(r, cx, cy, k * 0.34f, c);
      break;
    case B_OK:
      line(r, cx - k, cy + k * 0.05f, cx - k * 0.3f, cy + k * 0.7f, th, c);
      line(r, cx - k * 0.3f, cy + k * 0.7f, cx + k, cy - k * 0.7f, th, c);
      break;
    case B_BACK:
      line(r, cx - k * 0.8f, cy - k * 0.8f, cx + k * 0.8f, cy + k * 0.8f, th, c);
      line(r, cx - k * 0.8f, cy + k * 0.8f, cx + k * 0.8f, cy - k * 0.8f, th, c);
      break;
    case B_YES:  /* Y */
      line(r, cx - k * 0.8f, cy - k, cx, cy, th, c);
      line(r, cx + k * 0.8f, cy - k, cx, cy, th, c);
      line(r, cx, cy, cx, cy + k, th, c);
      break;
    case B_NO:   /* N */
      line(r, cx - k * 0.7f, cy + k, cx - k * 0.7f, cy - k, th, c);
      line(r, cx - k * 0.7f, cy - k, cx + k * 0.7f, cy + k, th, c);
      line(r, cx + k * 0.7f, cy + k, cx + k * 0.7f, cy - k, th, c);
      break;
    case B_KBD:
      line(r, cx - k, cy - k * 0.6f, cx + k, cy - k * 0.6f, th, c);
      line(r, cx + k, cy - k * 0.6f, cx + k, cy + k * 0.6f, th, c);
      line(r, cx + k, cy + k * 0.6f, cx - k, cy + k * 0.6f, th, c);
      line(r, cx - k, cy + k * 0.6f, cx - k, cy - k * 0.6f, th, c);
      line(r, cx - k * 0.5f, cy, cx + k * 0.5f, cy, th, c);
      break;
    default: break;
  }
}

/* ---- tiny 5x7 font for the "what you are typing" bar (the soft keyboard covers
 * half of the screen, so the text field of the game may be hidden behind it) ---- */
static const char font_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,-_:/'!?()+=*#";
static const unsigned char font_rows[][7] = {
  {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
  {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
  {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
  {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
  {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
  {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
  {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
  {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
  {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
  {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
  {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
  {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
  {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},{0x00,0x00,0x00,0x00,0x0C,0x04,0x08},
  {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
  {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},{0x01,0x01,0x02,0x04,0x08,0x10,0x10},
  {0x04,0x04,0x08,0x00,0x00,0x00,0x00},{0x04,0x04,0x04,0x04,0x04,0x00,0x04},
  {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},{0x02,0x04,0x08,0x08,0x08,0x04,0x02},
  {0x08,0x04,0x02,0x02,0x02,0x04,0x08},{0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
  {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},{0x00,0x15,0x0E,0x1F,0x0E,0x15,0x00},
  {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}
};
static const unsigned char font_box[7] = {0x1F,0x11,0x11,0x11,0x11,0x11,0x1F};

static void fill_rect(SDL_Renderer *r, float x, float y, float w, float h, SDL_Color c)
{
  SDL_Rect rc;
  rc.x = (int) (x + 0.5f);
  rc.y = (int) (y + 0.5f);
  rc.w = (int) (w + 0.5f);
  rc.h = (int) (h + 0.5f);
  if (rc.w < 1) rc.w = 1;
  if (rc.h < 1) rc.h = 1;
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rc);
}

static void draw_char(SDL_Renderer *r, int ch, float x, float y, float px, SDL_Color c)
{
  const unsigned char *rows = font_box;
  const char *p;
  int row, col;

  if (ch >= 'a' && ch <= 'z')
    ch -= 32;
  if (ch == ' ')
    return;
  p = ch ? strchr(font_chars, ch) : NULL;
  if (p)
    rows = font_rows[p - font_chars];
  for (row = 0; row < 7; row++)
    for (col = 0; col < 5; col++)
      if (rows[row] & (0x10 >> col))
        fill_rect(r, x + col * px, y + row * px, px, px, c);
}

/* bar at the top: the text being typed (tail of it) + blinking cursor */
static void draw_text_bar(SDL_Renderer *r)
{
  float px = unit * 0.85f, adv = px * 6.0f;
  float bx = out_w * 0.10f, bw = out_w * 0.66f;
  float by = unit * 2.0f, bh = px * 7 + unit * 3.0f;
  int len = (int) strlen(menu_text_buf);
  int maxc = (int) ((bw - unit * 3.0f) / adv) - 1;
  int start = len > maxc ? len - maxc : 0;
  float x = bx + unit * 1.5f;
  int i;

  fill_rect(r, bx, by, bw, bh, col(12, 14, 20, 215));
  fill_rect(r, bx, by, bw, unit * 0.3f, col(255, 150, 38, 200));
  for (i = start; i < len; i++, x += adv)
    draw_char(r, (unsigned char) menu_text_buf[i], x, by + unit * 1.5f, px, col(245, 247, 252, 255));
  if ((SDL_GetTicks() / 450) & 1)
    fill_rect(r, x, by + unit * 1.5f + px * 6.0f, px * 4, px, col(255, 170, 70, 255));
}

void dsda_android_DrawOverlay(SDL_Renderer *r)
{
  int i;
  float th;

  if (!r)
    return;
  if (!show_ui && mode_now() != MODE_MENU)
    return;

  /* draw in physical pixels, ignoring the game's logical resolution */
  SDL_RenderSetLogicalSize(r, 0, 0);
  SDL_RenderSetScale(r, 1.0f, 1.0f);
  SDL_RenderSetViewport(r, NULL);
  SDL_RenderSetClipRect(r, NULL);
  update_output_size();
  cur_mode = mode_now();
  layout_keep();
  set_blend(r);

  th = unit * 0.38f;

  /* movement stick */
  if (cur_mode == MODE_GAME && show_ui)
  {
    float R = STICK_R();
    int active = stick_finger >= 0 && fingers[stick_finger].active;
    float cx, cy;
    float kx, ky;
    if (active) { cx = stick_cx; cy = stick_cy; }
    else stick_home(&cx, &cy);       /* faint hint ring while idle */
    kx = cx;
    ky = cy;

    if (active)
    {
      float dx = stick_px - stick_cx, dy = stick_py - stick_cy;
      float len = sqrtf(dx * dx + dy * dy);
      if (len > R) { dx *= R / len; dy *= R / len; }
      kx = cx + dx;
      ky = cy + dy;
    }
    ring(r, cx, cy, R, th, col(235, 240, 250, active ? 120 : 62));
    disc(r, cx, cy, R, col(235, 240, 250, active ? 26 : 12));
    disc(r, kx, ky, 5.4f * unit, col(240, 244, 252, active ? 150 : 70));
  }

  for (i = 0; i < B_COUNT; i++)
  {
    button_t *b = &btn[i];
    int pressed;
    SDL_Color edge, fill, ink;

    if (!b->visible)
      continue;
    pressed = b->down > 0;

    if (i == B_YES || i == B_NO)
    {
      SDL_Color tone = (i == B_YES) ? col(90, 220, 120, 255) : col(240, 90, 80, 255);
      edge = tone; edge.a = pressed ? 240 : 190;
      fill = tone; fill.a = pressed ? 120 : 55;
      ink  = tone; ink.a  = 255;
    }
    else if (i == B_FIRE)
    {
      edge = col(255, 150, 38, pressed ? 235 : 150);
      fill = col(255, 140, 30, pressed ? 110 : 42);
      ink  = col(255, 170, 70, pressed ? 240 : 150);
    }
    else
    {
      edge = col(235, 240, 250, pressed ? 220 : 108);
      fill = col(235, 240, 250, pressed ? 70 : 20);
      ink  = col(240, 244, 252, pressed ? 240 : 150);
    }
    disc(r, b->x, b->y, b->r, fill);
    ring(r, b->x, b->y, b->r, th, edge);
    glyph(r, i, b->x, b->y, b->r * ((i == B_YES || i == B_NO) ? 0.5f : 0.42f),
          th * ((i == B_YES || i == B_NO) ? 1.3f : 0.9f), ink);
  }

  if (cur_mode == MODE_MENU && menu_text)
    draw_text_bar(r);

  /* hide stray 1-2 px colour fringes at the left / right screen edges */
  fill_rect(r, 0, 0, 2, out_h, col(0, 0, 0, 255));
  fill_rect(r, out_w - 2, 0, 2, out_h, col(0, 0, 0, 255));

  /* restore blend state; i_video.c restores the logical size */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
