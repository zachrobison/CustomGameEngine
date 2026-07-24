#pragma once
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include <glm/glm.hpp>

class Shader {
public:
    GLuint id = 0;
    Shader(const char* vertPath, const char* fragPath);
    ~Shader();

    void use();
    void setMat4 (const char* name, const glm::mat4& m);
    void setVec3 (const char* name, const glm::vec3& v);
    void setFloat(const char* name, float v);
};
