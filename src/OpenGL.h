#ifndef OPENGL_H
#define OPENGL_H
////////////////////////////////////////////////////////////////////////////////
// Usage
////////////////////////////////////////////////////////////////////////////////
// Copy the GlobalGLFunctions section to your platform layer, removing the
// extern and define the function pointers at runtime.

////////////////////////////////////////////////////////////////////////////////
// #TOC Table Of Contents
////////////////////////////////////////////////////////////////////////////////
// #WGL               WindowsGL Extension Definitions
// #OGL               OpenGL Extension Definitions
// #GlobalGLFunctions Exposed public function pointers

////////////////////////////////////////////////////////////////////////////////
// #WGL Windows GL Extension
////////////////////////////////////////////////////////////////////////////////
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <gl/gl.h>

#define WGL_WGLEXT_VERSION 20170613
#ifndef WGL_ARB_create_context_profile
#define WGL_ARB_create_context_profile 1
	#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126
	#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
	#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
	#define ERROR_INVALID_PROFILE_ARB                 0x2096
#endif /* WGL_ARB_create_context_profile */

#ifndef WGL_ARB_pixel_format
#define WGL_ARB_pixel_format 1
	#define WGL_DRAW_TO_WINDOW_ARB            0x2001
	#define WGL_ACCELERATION_ARB              0x2003
	#define WGL_SUPPORT_OPENGL_ARB            0x2010
	#define WGL_DOUBLE_BUFFER_ARB             0x2011
	#define WGL_PIXEL_TYPE_ARB                0x2013
	#define WGL_COLOR_BITS_ARB                0x2014
	#define WGL_ALPHA_BITS_ARB                0x201B
	#define WGL_DEPTH_BITS_ARB                0x2022
	#define WGL_STENCIL_BITS_ARB              0x2023
	#define WGL_TYPE_RGBA_ARB                 0x202B
	#define WGL_FULL_ACCELERATION_ARB         0x2027
	typedef BOOL wglChoosePixelFormatARBProc(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
#endif /* WGL_ARB_pixel_format */

#ifndef WGL_ARB_multisample
#define WGL_ARB_multisample 1
	#define WGL_SAMPLE_BUFFERS_ARB            0x2041
	#define WGL_SAMPLES_ARB                   0x2042
#endif /* WGL_ARB_multisample */

#ifndef WGL_ARB_create_context
#define WGL_ARB_create_context 1
	#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
	#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
	typedef HGLRC wglCreateContextAttribsARBProc(HDC hDC, HGLRC hShareContext, const int *attribList);
#endif /* WGL_ARGB_create_context */

////////////////////////////////////////////////////////////////////////////////
// #OGL OpenGL Extension
////////////////////////////////////////////////////////////////////////////////
typedef void glClearColorProc(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void glDrawArraysProc(GLenum mode, GLint first, GLsizei count);

#ifndef GL_VERSION_1_3
#define GL_VERSION_1_3 1
    #define GL_TEXTURE0                       0x84C0
    #define GL_TEXTURE1                       0x84C1
    #define GL_TEXTURE2                       0x84C2
    #define GL_TEXTURE3                       0x84C3
    #define GL_TEXTURE4                       0x84C4
    #define GL_TEXTURE5                       0x84C5
    #define GL_TEXTURE6                       0x84C6
    #define GL_TEXTURE7                       0x84C7
    #define GL_TEXTURE8                       0x84C8
    #define GL_TEXTURE9                       0x84C9
    #define GL_TEXTURE10                      0x84CA
    #define GL_TEXTURE11                      0x84CB
    #define GL_TEXTURE12                      0x84CC
    #define GL_TEXTURE13                      0x84CD
    #define GL_TEXTURE14                      0x84CE
    #define GL_TEXTURE15                      0x84CF
    #define GL_TEXTURE16                      0x84D0
    #define GL_TEXTURE17                      0x84D1
    #define GL_TEXTURE18                      0x84D2
    #define GL_TEXTURE19                      0x84D3
    #define GL_TEXTURE20                      0x84D4
    #define GL_TEXTURE21                      0x84D5
    #define GL_TEXTURE22                      0x84D6
    #define GL_TEXTURE23                      0x84D7
    #define GL_TEXTURE24                      0x84D8
    #define GL_TEXTURE25                      0x84D9
    #define GL_TEXTURE26                      0x84DA
    #define GL_TEXTURE27                      0x84DB
    #define GL_TEXTURE28                      0x84DC
    #define GL_TEXTURE29                      0x84DD
    #define GL_TEXTURE30                      0x84DE
    #define GL_TEXTURE31                      0x84DF

    typedef void glActiveTextureProc(GLenum texture);
#endif /* GL_VERSION_1_3 */

#ifndef GL_VERSION_1_5
#define GL_VERSION_1_5 1
	typedef ptrdiff_t GLsizeiptr;
	typedef ptrdiff_t GLintptr;

	#define GL_STREAM_DRAW                    0x88E0
	#define GL_STATIC_DRAW                    0x88E4
	#define GL_DYNAMIC_DRAW                   0x88E8
	#define GL_ARRAY_BUFFER                   0x8892
	#define GL_ELEMENT_ARRAY_BUFFER           0x8893

	typedef void glGenBuffersProc(GLsizei n, GLuint *buffers);
	typedef void glBindBufferProc(GLenum target, GLuint buffer);
	typedef void glBufferDataProc(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
#endif /* GL_VERSION_1_5 */

#ifndef GL_VERSION_2_0
#define GL_VERSION_2_0 1
	typedef char GLchar;

	#define GL_FRAGMENT_SHADER                0x8B30
	#define GL_VERTEX_SHADER                  0x8B31
	#define GL_COMPILE_STATUS                 0x8B81
	#define GL_LINK_STATUS                    0x8B82

	typedef GLuint glCreateShaderProc      (GLenum type);
	typedef void   glShaderSourceProc      (GLuint shader, GLsizei count, GLchar **string, const GLint *length);
	typedef void   glCompileShaderProc     (GLuint shader);
	typedef void   glGetShaderivProc       (GLuint shader, GLenum pname, GLint *params);
	typedef void   glGetShaderInfoLogProc  (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
	typedef GLuint glCreateProgramProc     (void);
	typedef void   glAttachShaderProc      (GLuint program, GLuint shader);
	typedef void   glLinkProgramProc       (GLuint program);
	typedef void   glUseProgramProc        (GLuint program);
	typedef void   glDeleteShaderProc      (GLuint shader);
	typedef void   glGetProgramInfoLogProc (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
	typedef void   glGetProgramivProc      (GLuint program, GLenum pname, GLint *params);

	typedef GLint  glGetUniformLocationProc(GLuint program, const GLchar *name);
	typedef void   glUniform4fProc         (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
	typedef void   glUniform1iProc         (GLint location, GLint v0);
	typedef void   glUniformMatrix4fvProc  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

    typedef void glEnableVertexAttribArrayProc (GLuint index);
	typedef void glDisableVertexAttribArrayProc(GLuint index);
	typedef void glVertexAttribPointerProc     (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
#endif /* GL_VERSION_2_0 */

#ifndef GL_VERSION_3_0
#define GL_VERSION_3_0 1
	typedef unsigned short GLhalf;
	typedef void glGenVertexArraysProc(GLsizei n, GLuint *arrays);
	typedef void glBindVertexArrayProc(GLuint array);
	typedef void glGenerateMipmapProc (GLenum target);
#endif /* GL_VERSION_3_0 */

////////////////////////////////////////////////////////////////////////////////
// #GlobalGLFunctions
////////////////////////////////////////////////////////////////////////////////
// Copy the following definitions to the platform layer and link to your OpenGL
// functions at runtime.

// WinGL
extern wglChoosePixelFormatARBProc    *wglChoosePixelFormatARB;
extern wglCreateContextAttribsARBProc *wglCreateContextAttribsARB;

// GL 1.3
extern glActiveTextureProc *glActiveTexture;

// GL 1.5
extern glGenBuffersProc *glGenBuffers;
extern glBindBufferProc *glBindBuffer;
extern glBufferDataProc *glBufferData;

// GL 2.0
extern glCreateShaderProc             *glCreateShader;
extern glShaderSourceProc             *glShaderSource;
extern glCompileShaderProc            *glCompileShader;
extern glGetShaderivProc              *glGetShaderiv;
extern glGetShaderInfoLogProc         *glGetShaderInfoLog;
extern glCreateProgramProc            *glCreateProgram;
extern glAttachShaderProc             *glAttachShader;
extern glLinkProgramProc              *glLinkProgram;
extern glUseProgramProc               *glUseProgram;
extern glDeleteShaderProc             *glDeleteShader;
extern glGetProgramInfoLogProc        *glGetProgramInfoLog;
extern glGetProgramivProc             *glGetProgramiv;

extern glGetUniformLocationProc       *glGetUniformLocation;
extern glUniform4fProc                *glUniform4f;
extern glUniform1iProc                *glUniform1i;
extern glUniformMatrix4fvProc         *glUniformMatrix4fv;

extern glEnableVertexAttribArrayProc  *glEnableVertexAttribArray;
extern glDisableVertexAttribArrayProc *glDisableVertexAttribArray;
extern glVertexAttribPointerProc      *glVertexAttribPointer;

// GL 3.0
extern glGenVertexArraysProc *glGenVertexArrays;
extern glBindVertexArrayProc *glBindVertexArray;
extern glGenerateMipmapProc  *glGenerateMipmap;

#endif // OPENGL_H
