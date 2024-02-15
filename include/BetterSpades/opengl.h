#ifndef OPENGL_H
#define OPENGL_H

#ifndef OPENGL_ES
    #define GLEW_STATIC
    #include <GL/glew.h>
#else
    #ifdef USE_SDL
        #include <SDL2/SDL_opengles.h>
    #endif

    #define glColor3f(r, g, b)  glColor4f(r, g, b, 1.0F)
    #define glColor3ub(r, g, b) glColor4ub(r, g, b, 255)
    #define glDepthRange(a, b)  glDepthRangef(a, b)
    #define glClearDepth(a)     glClearDepthf(a)
#endif

#endif