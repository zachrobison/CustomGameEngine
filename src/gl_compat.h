#pragma once
// Portable OpenGL header. macOS ships a full GL 4.1 core header and needs no
// loader; Windows/Linux need a loader (GLEW) to get modern GL entry points.
#if defined(__APPLE__)
  #ifndef GL_SILENCE_DEPRECATION
  #define GL_SILENCE_DEPRECATION
  #endif
  #include <OpenGL/gl3.h>
#else
  #include <GL/glew.h>
#endif
