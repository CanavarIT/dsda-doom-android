/*
 * Android build: DSDA-Doom's OpenGL renderer uses desktop (fixed-function)
 * OpenGL, which does not exist on Android. The game always runs in the
 * Software renderer (frames are presented through SDL's GLES2 renderer),
 * so every gld_* entry point is a harmless no-op.
 */
#include "config.h"
#include <stddef.h>
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_state.h"
#include "p_spec.h"
#include "gl_struct.h"
#include "dsda/gl/render_scale.h"

/* render_scale.c globals */
int gl_statusbar_height = 0;
int gl_scene_width = 0;
int gl_scene_height = 0;
float gl_scale_x = 1.0f;
float gl_scale_y = 1.0f;
int gl_letterbox_clear_required = 0;

void dsda_GLSetRenderViewportParams(void) {}
void dsda_GLSetRenderViewport(void) {}
void dsda_GLSetRenderViewportScissor(void) {}
void dsda_GLSetRenderSceneScissor(void) {}
void dsda_GLSetScreenSpaceScissor(int x, int y, int w, int h) {}
void dsda_GLUpdateStatusBarVisible(void) {}
void dsda_GLLetterboxClear(void) {}
void dsda_GLStartMeltRenderTexture(void) {}
void dsda_GLEndMeltRenderTexture(void) {}
void dsda_GLFullscreenOrtho2D(void) {}

/* gl_main.c / gl_light.c / gl_sky.c / gl_opengl.c globals */
TAnimItemParam *anim_flats = NULL;
TAnimItemParam *anim_textures = NULL;
const int gl_colorbuffer_bits = 32;
const int gl_depthbuffer_bits = 24;
int gl_drawskys = 0;
dboolean gl_ui_lightmode_indexed = false;
dboolean gl_automap_lightmode_indexed = false;
dboolean gl_menu_lightmode_indexed = false;
dboolean gl_use_stencil = false;

/* fake floor / bleed-through helpers only fed by the GL preprocessor */
sector_t* GetBestFake(sector_t *sector, int ceiling, int validcount) { return NULL; }
sector_t* GetBestBleedSector(sector_t* source, enum bleedtype type) { return NULL; }

void gld_FlushTextures(void) {}
void gld_InitVertexData() {}
void gld_CleanVertexData() {}
void gld_UpdateSplitData(sector_t *sector) {}
void gld_Init(int width, int height) {}
void gld_InitCommandLine(void) {}
void gld_BeginUIDraw(void) {}
void gld_EndUIDraw(void) {}
void gld_BeginAutomapDraw(void) {}
void gld_EndAutomapDraw(void) {}
void gld_BeginMenuDraw(void) {}
void gld_EndMenuDraw(void) {}
void gld_DrawNumPatch(int x, int y, int lump, dboolean center, int cm, enum patch_translation_e flags) {}
void gld_DrawNumPatch_f(float x, float y, int lump, dboolean center, int cm, enum patch_translation_e flags) {}
void gld_FillRaw(int lump, int x, int y, int src_width, int src_height, int dst_width, int dst_height, enum patch_translation_e flags) {}
void gld_FillPatch(int lump, int x, int y, int width, int height, enum patch_translation_e flags) {}
void gld_DrawLine(int x0, int y0, int x1, int y1, int BaseColor) {}
void gld_DrawLine_f(float x0, float y0, float x1, float y1, int BaseColor) {}
void gld_DrawWeapon(int weaponlump, vissprite_t *vis, int lightlevel) {}
void gld_FillBlock(int x, int y, int width, int height, int col) {}
void gld_DrawShaded(int x, int y, int width, int height, int shade) {}
void gld_SetPalette(int palette) {}
unsigned char *gld_ReadScreen(void) { return NULL; }
void gld_CleanMemory(void) {}
void gld_CleanStaticMemory(void) {}
void gld_PreprocessLevel(void) {}
void gld_Set2DMode() {}
void gld_InitDrawScene(void) {}
void gld_StartDrawScene(void) {}
void gld_AddPlane(int subsectornum, visplane_t *floor, visplane_t *ceiling) {}
void gld_AddWall(seg_t *seg) {}
void gld_ProjectSprite(mobj_t* thing, int lightlevel) {}
void gld_DrawScene(player_t *player) {}
void gld_EndDrawScene(void) {}
void gld_Finish() {}
int gld_wipe_doMelt(int ticks, int *y_lookup) { return 1; }
int gld_wipe_exitMelt(int ticks) { return 0; }
int gld_wipe_StartScreen(void) { return 0; }
int gld_wipe_EndScreen(void) { return 0; }
dboolean gld_clipper_SafeCheckRange(angle_t startAngle, angle_t endAngle) { return true; }
void gld_clipper_SafeAddClipRange(angle_t startangle, angle_t endangle) {}
void gld_FrustumSetup(void) {}
dboolean gld_SphereInFrustum(float x, float y, float z, float radius) { return true; }
void gld_DrawMapLines(void) {}
void gld_MultisamplingInit(void) {}
void gld_MultisamplingSet(void) {}
void gld_ProcessTexturedMap(void) {}
void gld_ResetTexturedAutomap(void) {}
void gld_ResetAutomapTransparency(void) {}
void gld_MapDrawSubsectors(player_t *plr, int fx, int fy, fixed_t mx, fixed_t my, int fw, int fh, fixed_t scale) {}
void gld_Init8InGLMode(void) {}
void gld_Draw8InGL(void) {}
void gld_InitMapPics(void) {}
void gld_AddNiceThing(int type, float x, float y, float radius, float angle,
                      unsigned char r, unsigned char g, unsigned char b, unsigned char a) {}
void gld_DrawNiceThings(int fx, int fy, int fw, int fh) {}
void gld_ClearNiceThings(void) {}

/* ENDOOM text screen (txt_sdl.c) only calls these in OpenGL mode; they are renamed at
 * compile time (see CMakeLists.txt) so no GL symbols are exported from libmain.so. */
void dsda_stub_glBegin() {}
void dsda_stub_glBindTexture() {}
void dsda_stub_glClear() {}
void dsda_stub_glClearColor() {}
void dsda_stub_glDeleteTextures() {}
void dsda_stub_glDisable() {}
void dsda_stub_glEnable() {}
void dsda_stub_glEnd() {}
void dsda_stub_glGenTextures() {}
void dsda_stub_glLoadIdentity() {}
void dsda_stub_glMatrixMode() {}
void dsda_stub_glOrtho() {}
void dsda_stub_glScalef() {}
void dsda_stub_glTexCoord2f() {}
void dsda_stub_glTexImage2D() {}
void dsda_stub_glTexParameteri() {}
void dsda_stub_glTexSubImage2D() {}
void dsda_stub_glTranslatef() {}
void dsda_stub_glVertex2f() {}
void dsda_stub_glViewport() {}
