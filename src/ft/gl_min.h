// FRACTURE — minimal GL3.3 declaration set (no system GL headers required)
// Function pointers are resolved at runtime via ft_getproc (platform-provided).
#pragma once
#include <cstdint>
#include <cstddef>

namespace ft {

// ---- GL types ----
using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLsizei = int;
using GLuint = unsigned int;
using GLfloat = float;
using GLchar = char;
using GLsizeiptr = std::int64_t;
using GLubyte = unsigned char;
using GLvoid = void;
using GLintptr_off = std::ptrdiff_t;

// ---- constants (subset used by the renderer) ----
enum : GLenum {
    GL_FALSE_ = 0,
    GL_TRUE_ = 1,
    GL_DEPTH_BUFFER_BIT = 0x00000100,
    GL_COLOR_BUFFER_BIT = 0x00004000,
    GL_POINTS = 0x0000,
    GL_TRIANGLES = 0x0004,
    GL_TRIANGLE_STRIP = 0x0005,
    GL_DEPTH_TEST = 0x0B71,
    GL_BLEND = 0x0BE2,
    GL_CULL_FACE = 0x0B44,
    GL_SCISSOR_TEST = 0x0C11,
    GL_LEQUAL = 0x0203,
    GL_LESS = 0x0201,
    GL_ALWAYS = 0x0207,
    GL_SRC_ALPHA = 0x0302,
    GL_ONE_MINUS_SRC_ALPHA = 0x0303,
    GL_ONE = 1,
    GL_FRONT = 0x0404,
    GL_BACK = 0x0405,
    GL_CCW = 0x0901,
    GL_CW = 0x0900,
    GL_BYTE = 0x1400,
    GL_UNSIGNED_BYTE = 0x1401,
    GL_SHORT = 0x1402,
    GL_UNSIGNED_SHORT = 0x1403,
    GL_INT = 0x1404,
    GL_UNSIGNED_INT = 0x1405,
    GL_FLOAT = 0x1406,
    GL_RGB = 0x1907,
    GL_RGBA = 0x1908,
    GL_RED = 0x1903,
    GL_R8 = 0x8229,
    GL_RGBA8 = 0x8058,
    GL_RGB8 = 0x8051,
    GL_DEPTH_COMPONENT = 0x1902,
    GL_DEPTH_COMPONENT24 = 0x81A6,
    GL_TEXTURE_2D = 0x0DE1,
    GL_TEXTURE0 = 0x84C0,
    GL_TEXTURE1 = 0x84C1,
    GL_TEXTURE2 = 0x84C2,
    GL_TEXTURE_MAG_FILTER = 0x2800,
    GL_TEXTURE_MIN_FILTER = 0x2801,
    GL_TEXTURE_WRAP_S = 0x2802,
    GL_TEXTURE_WRAP_T = 0x2803,
    GL_NEAREST = 0x2600,
    GL_LINEAR = 0x2601,
    GL_LINEAR_MIPMAP_LINEAR = 0x2703,
    GL_REPEAT = 0x2901,
    GL_MIRRORED_REPEAT = 0x8370,
    GL_CLAMP_TO_EDGE = 0x812F,
    GL_ARRAY_BUFFER = 0x8892,
    GL_ELEMENT_ARRAY_BUFFER = 0x8893,
    GL_STATIC_DRAW = 0x88E4,
    GL_DYNAMIC_DRAW = 0x88E8,
    GL_STREAM_DRAW = 0x88E0,
    GL_FRAGMENT_SHADER = 0x8B30,
    GL_VERTEX_SHADER = 0x8B31,
    GL_COMPILE_STATUS = 0x8B81,
    GL_LINK_STATUS = 0x8B82,
    GL_INFO_LOG_LENGTH = 0x8B84,
    GL_FRAMEBUFFER = 0x8D40,
    GL_DEPTH_ATTACHMENT = 0x8D00,
    GL_FRAMEBUFFER_COMPLETE = 0x8CD5,
    GL_PACK_ALIGNMENT = 0x0D05,
    GL_UNPACK_ALIGNMENT = 0x0CF5,
    GL_VENDOR = 0x1F00,
    GL_RENDERER = 0x1F01,
    GL_VERSION = 0x1F02,
    GL_NO_ERROR = 0,
    GL_NONE = 0,
    GL_FLOAT_32_VEC2 = 0x8B50,
    GL_FLOAT_32_VEC3 = 0x8B51,
    GL_FLOAT_32_VEC4 = 0x8B52,
    GL_INT_32_VEC2 = 0x8B53,
    GL_INT_32_VEC4 = 0x8B57,
};

// ---- function pointer table ----
extern void (*glEnable)(GLenum);
extern void (*glDisable)(GLenum);
extern void (*glViewport)(GLint, GLint, GLsizei, GLsizei);
extern void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*glClear)(GLbitfield);
extern void (*glDepthFunc)(GLenum);
extern void (*glDepthMask)(GLboolean);
extern void (*glBlendFunc)(GLenum, GLenum);
extern void (*glCullFace)(GLenum);
extern void (*glFrontFace)(GLenum);
extern void (*glPixelStorei)(GLenum, GLint);
extern GLenum (*glGetError)();
extern const GLubyte* (*glGetString)(GLenum);

extern void (*glGenBuffers)(GLsizei, GLuint*);
extern void (*glBindBuffer)(GLenum, GLuint);
extern void (*glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (*glBufferSubData)(GLenum, GLintptr_off, GLsizeiptr, const void*);
extern void (*glDeleteBuffers)(GLsizei, const GLuint*);

extern void (*glGenVertexArrays)(GLsizei, GLuint*);
extern void (*glBindVertexArray)(GLuint);
extern void (*glDeleteVertexArrays)(GLsizei, const GLuint*);
extern void (*glEnableVertexAttribArray)(GLuint);
extern void (*glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void (*glVertexAttribDivisor)(GLuint, GLuint);

extern void (*glDrawArrays)(GLenum, GLint, GLsizei);
extern void (*glDrawElements)(GLenum, GLsizei, GLenum, const void*);

extern GLuint (*glCreateShader)(GLenum);
extern void (*glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void (*glCompileShader)(GLuint);
extern void (*glGetShaderiv)(GLuint, GLenum, GLint*);
extern void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*glDeleteShader)(GLuint);
extern GLuint (*glCreateProgram)();
extern void (*glAttachShader)(GLuint, GLuint);
extern void (*glLinkProgram)(GLuint);
extern void (*glGetProgramiv)(GLuint, GLenum, GLint*);
extern void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*glUseProgram)(GLuint);
extern void (*glDeleteProgram)(GLuint);
extern GLint (*glGetUniformLocation)(GLuint, const GLchar*);
extern void (*glUniform1i)(GLint, GLint);
extern void (*glUniform1f)(GLint, GLfloat);
extern void (*glUniform2f)(GLint, GLfloat, GLfloat);
extern void (*glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
extern void (*glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*glUniform3fv)(GLint, GLsizei, const GLfloat*);
extern void (*glUniform4fv)(GLint, GLsizei, const GLfloat*);
extern void (*glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

extern void (*glGenTextures)(GLsizei, GLuint*);
extern void (*glBindTexture)(GLenum, GLuint);
extern void (*glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
extern void (*glTexParameteri)(GLenum, GLenum, GLint);
extern void (*glDeleteTextures)(GLsizei, const GLuint*);
extern void (*glActiveTexture)(GLenum);
extern void (*glGenerateMipmap)(GLenum);

extern void (*glGenFramebuffers)(GLsizei, GLuint*);
extern void (*glBindFramebuffer)(GLenum, GLuint);
extern void (*glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
extern GLenum (*glCheckFramebufferStatus)(GLenum);
extern void (*glDeleteFramebuffers)(GLsizei, const GLuint*);
extern void (*glDrawBuffer)(GLenum);
extern void (*glReadBuffer)(GLenum);
extern void (*glFinish)();

// platform-provided proc resolver (set before GLBackend::init; defined in glbackend.cpp)
extern void* (*ft_getproc)(const char*);

} // namespace ft
