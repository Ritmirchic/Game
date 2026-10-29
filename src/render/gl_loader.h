// FRACTURE — minimal OpenGL 3.3 function loader (Win32 WGL)
#pragma once
#ifdef _WIN32

#include <windows.h>
#include <GL/gl.h>

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_MULTISAMPLE_4X 0x0004
#define GL_FRAMEBUFFER_SRGB_CAPABLE 0x8DBA
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_RGBA8 0x8058
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FUNC_ADD 0x8006
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_TEXTURE_FILTER_ANISOTROPIC 0x84FE
#define GL_RED 0x1903
#define GL_R8 0x8229
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_MAX_SAMPLES 0x8D57
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_DEPTH_STENCIL 0x84F9
#define GL_MULTISAMPLE 0x809D
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_MAP_READ_BIT 0x0001
#define GL_BGRA 0x80E1
#define GL_DOUBLE 0x140A
#define GL_DEPTH_CLAMP 0x864F
#define GL_CLIP_DISTANCE0 0x3000
#define GL_POLYGON_OFFSET_FILL 0x8037

typedef void (APIENTRY* PFN_glGenBuffers)(GLsizei, GLuint*);
typedef void (APIENTRY* PFN_glBindBuffer)(GLenum, GLuint);
typedef void (APIENTRY* PFN_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (APIENTRY* PFN_glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
typedef void (APIENTRY* PFN_glDeleteBuffers)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFN_glGenVertexArrays)(GLsizei, GLuint*);
typedef void (APIENTRY* PFN_glBindVertexArray)(GLuint);
typedef void (APIENTRY* PFN_glDeleteVertexArrays)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFN_glEnableVertexAttribArray)(GLuint);
typedef void (APIENTRY* PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void (APIENTRY* PFN_glVertexAttribIPointer)(GLuint, GLint, GLenum, GLsizei, const void*);
typedef void (APIENTRY* PFN_glVertexAttribDivisor)(GLuint, GLuint);
typedef GLuint (APIENTRY* PFN_glCreateShader)(GLenum);
typedef void (APIENTRY* PFN_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void (APIENTRY* PFN_glCompileShader)(GLuint);
typedef void (APIENTRY* PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void (APIENTRY* PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (APIENTRY* PFN_glDeleteShader)(GLuint);
typedef GLuint (APIENTRY* PFN_glCreateProgram)(void);
typedef void (APIENTRY* PFN_glAttachShader)(GLuint, GLuint);
typedef void (APIENTRY* PFN_glLinkProgram)(GLuint);
typedef void (APIENTRY* PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
typedef void (APIENTRY* PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (APIENTRY* PFN_glUseProgram)(GLuint);
typedef void (APIENTRY* PFN_glDeleteProgram)(GLuint);
typedef GLint (APIENTRY* PFN_glGetUniformLocation)(GLuint, const GLchar*);
typedef void (APIENTRY* PFN_glUniform1i)(GLint, GLint);
typedef void (APIENTRY* PFN_glUniform1f)(GLint, GLfloat);
typedef void (APIENTRY* PFN_glUniform2f)(GLint, GLfloat, GLfloat);
typedef void (APIENTRY* PFN_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY* PFN_glUniform3fv)(GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* PFN_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY* PFN_glUniform1fv)(GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* PFN_glUniform4fv)(GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* PFN_glUniform2fv)(GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* PFN_glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (APIENTRY* PFN_glActiveTexture)(GLenum);
typedef void (APIENTRY* PFN_glGenerateMipmap)(GLenum);
typedef void (APIENTRY* PFN_glGenFramebuffers)(GLsizei, GLuint*);
typedef void (APIENTRY* PFN_glBindFramebuffer)(GLenum, GLuint);
typedef void (APIENTRY* PFN_glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (APIENTRY* PFN_glCheckFramebufferStatus)(GLenum);
typedef void (APIENTRY* PFN_glDeleteFramebuffers)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFN_glGenRenderbuffers)(GLsizei, GLuint*);
typedef void (APIENTRY* PFN_glBindRenderbuffer)(GLenum, GLuint);
typedef void (APIENTRY* PFN_glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY* PFN_glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
typedef void (APIENTRY* PFN_glDeleteRenderbuffers)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFN_glDrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei);
typedef void (APIENTRY* PFN_glDrawElementsInstanced)(GLenum, GLsizei, GLenum, const void*, GLsizei);
typedef void (APIENTRY* PFN_glBlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
typedef void (APIENTRY* PFN_glDrawBuffers)(GLsizei, const GLenum*);
typedef const GLubyte* (APIENTRY* PFN_glGetStringi)(GLenum, GLuint);

struct GLFuncs {
    PFN_glGenBuffers GenBuffers = nullptr;
    PFN_glBindBuffer BindBuffer = nullptr;
    PFN_glBufferData BufferData = nullptr;
    PFN_glBufferSubData BufferSubData = nullptr;
    PFN_glDeleteBuffers DeleteBuffers = nullptr;
    PFN_glGenVertexArrays GenVertexArrays = nullptr;
    PFN_glBindVertexArray BindVertexArray = nullptr;
    PFN_glDeleteVertexArrays DeleteVertexArrays = nullptr;
    PFN_glEnableVertexAttribArray EnableVertexAttribArray = nullptr;
    PFN_glVertexAttribPointer VertexAttribPointer = nullptr;
    PFN_glVertexAttribIPointer VertexAttribIPointer = nullptr;
    PFN_glVertexAttribDivisor VertexAttribDivisor = nullptr;
    PFN_glCreateShader CreateShader = nullptr;
    PFN_glShaderSource ShaderSource = nullptr;
    PFN_glCompileShader CompileShader = nullptr;
    PFN_glGetShaderiv GetShaderiv = nullptr;
    PFN_glGetShaderInfoLog GetShaderInfoLog = nullptr;
    PFN_glDeleteShader DeleteShader = nullptr;
    PFN_glCreateProgram CreateProgram = nullptr;
    PFN_glAttachShader AttachShader = nullptr;
    PFN_glLinkProgram LinkProgram = nullptr;
    PFN_glGetProgramiv GetProgramiv = nullptr;
    PFN_glGetProgramInfoLog GetProgramInfoLog = nullptr;
    PFN_glUseProgram UseProgram = nullptr;
    PFN_glDeleteProgram DeleteProgram = nullptr;
    PFN_glGetUniformLocation GetUniformLocation = nullptr;
    PFN_glUniform1i Uniform1i = nullptr;
    PFN_glUniform1f Uniform1f = nullptr;
    PFN_glUniform2f Uniform2f = nullptr;
    PFN_glUniform3f Uniform3f = nullptr;
    PFN_glUniform3fv Uniform3fv = nullptr;
    PFN_glUniform4f Uniform4f = nullptr;
    PFN_glUniform1fv Uniform1fv = nullptr;
    PFN_glUniform4fv Uniform4fv = nullptr;
    PFN_glUniform2fv Uniform2fv = nullptr;
    PFN_glUniformMatrix4fv UniformMatrix4fv = nullptr;
    PFN_glActiveTexture ActiveTexture = nullptr;
    PFN_glGenerateMipmap GenerateMipmap = nullptr;
    PFN_glGenFramebuffers GenFramebuffers = nullptr;
    PFN_glBindFramebuffer BindFramebuffer = nullptr;
    PFN_glFramebufferTexture2D FramebufferTexture2D = nullptr;
    PFN_glCheckFramebufferStatus CheckFramebufferStatus = nullptr;
    PFN_glDeleteFramebuffers DeleteFramebuffers = nullptr;
    PFN_glGenRenderbuffers GenRenderbuffers = nullptr;
    PFN_glBindRenderbuffer BindRenderbuffer = nullptr;
    PFN_glRenderbufferStorage RenderbufferStorage = nullptr;
    PFN_glFramebufferRenderbuffer FramebufferRenderbuffer = nullptr;
    PFN_glDeleteRenderbuffers DeleteRenderbuffers = nullptr;
    PFN_glDrawArraysInstanced DrawArraysInstanced = nullptr;
    PFN_glDrawElementsInstanced DrawElementsInstanced = nullptr;
    PFN_glBlitFramebuffer BlitFramebuffer = nullptr;
    bool ok = false;
};

GLFuncs& gl();
bool glLoad();
typedef void (APIENTRY* PFN_wglSwapIntervalEXT)(int);
typedef HGLRC (APIENTRY* PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef const char* (APIENTRY* PFN_wglGetExtensionsStringARB)(HDC);

#endif // _WIN32
