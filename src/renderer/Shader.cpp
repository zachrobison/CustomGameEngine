#include "Shader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

static GLuint compileStage(GLenum type, const char* path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error(std::string("Shader not found: ") + path);
    std::stringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();
    const char* c = src.c_str();

    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {};
        glGetShaderInfoLog(s, 512, nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(std::string("Shader compile error (") + path + "): " + log);
    }
    return s;
}

Shader::Shader(const char* vertPath, const char* fragPath) {
    GLuint vert = compileStage(GL_VERTEX_SHADER, vertPath);
    GLuint frag = compileStage(GL_FRAGMENT_SHADER, fragPath);

    id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);

    GLint ok = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512] = {};
        glGetProgramInfoLog(id, 512, nullptr, log);
        throw std::runtime_error(std::string("Shader link error: ") + log);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() { if (id) glDeleteProgram(id); }

void Shader::use()                               { glUseProgram(id); }
void Shader::setMat4(const char* n, const glm::mat4& m) { glUniformMatrix4fv(glGetUniformLocation(id, n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::setVec3(const char* n, const glm::vec3& v) { glUniform3fv(glGetUniformLocation(id, n), 1, glm::value_ptr(v)); }
void Shader::setFloat(const char* n, float v)           { glUniform1f(glGetUniformLocation(id, n), v); }
