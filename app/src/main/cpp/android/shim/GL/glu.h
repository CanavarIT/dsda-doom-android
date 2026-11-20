/* Android shim: minimal GLU declarations (functions are stubbed). */
#ifndef DSDA_ANDROID_GLU_H
#define DSDA_ANDROID_GLU_H
#include <SDL_opengl.h>
#ifdef __cplusplus
extern "C" {
#endif
void gluOrtho2D(GLdouble l, GLdouble r, GLdouble b, GLdouble t);
void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
#ifdef __cplusplus
}
#endif
#endif
