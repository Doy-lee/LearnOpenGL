#ifndef OPENGL_H
#define OPENGL_H

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <gl/gl.h>

// #TOC Table Of Contents
// #WGL WindowsGL Extension
// #OGL OpenGL Extension

////////////////////////////////////////////////////////////////////////////////
// #WGL Windows GL Extension
////////////////////////////////////////////////////////////////////////////////
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
	typedef BOOL WglChoosePixelFormatARBProc(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
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
	typedef HGLRC WglCreateContextAttribsARBProc(HDC hDC, HGLRC hShareContext, const int *attribList);
#endif /* WGL_ARGB_create_context */

////////////////////////////////////////////////////////////////////////////////
// #OGL OpenGL Extension
////////////////////////////////////////////////////////////////////////////////
#ifndef GL_VERSION_1_5
#define GL_VERSION_1_5 1
	typedef ptrdiff_t GLsizeiptr;
	typedef ptrdiff_t GLintptr;

	#define GL_STREAM_DRAW                    0x88E0
	#define GL_STATIC_DRAW                    0x88E4
	#define GL_DYNAMIC_DRAW                   0x88E8
	#define GL_ARRAY_BUFFER                   0x8892

	typedef void GLGenBuffersProc(GLsizei n, GLuint *buffers);
	typedef void GLBindBufferProc(GLenum target, GLuint buffer);
	typedef void GLBufferDataProc(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
#endif /* GL_VERSION_1_5 */

#ifndef GL_VERSION_2_0
#define GL_VERSION_2_0 1
	typedef char GLchar;

	#define GL_FRAGMENT_SHADER                0x8B30
	#define GL_VERTEX_SHADER                  0x8B31
	#define GL_COMPILE_STATUS                 0x8B81
	#define GL_LINK_STATUS                    0x8B82

	typedef GLuint GLCreateShaderProc     (GLenum type);
	typedef void   GLShaderSourceProc     (GLuint shader, GLsizei count, GLchar **string, const GLint *length);
	typedef void   GLCompileShaderProc    (GLuint shader);
	typedef void   GLGetShaderIVProc      (GLuint shader, GLenum pname, GLint *params);
	typedef void   GLGetShaderInfoLogProc (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
	typedef GLuint GLCreateProgramProc    (void);
	typedef void   GLAttachShaderProc     (GLuint program, GLuint shader);
	typedef void   GLLinkProgramProc      (GLuint program);
	typedef void   GLUseProgramProc       (GLuint program);
	typedef void   GLDeleteShaderProc     (GLuint shader);
	typedef void   GLGetProgramInfoLogProc(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
	typedef void   GLGetProgramIVProc     (GLuint program, GLenum pname, GLint *params);

	typedef void GLEnableVertexAttribArrayProc (GLuint index);
	typedef void GLDisableVertexAttribArrayProc(GLuint index);
	typedef void GLVertexAttribPointerProc     (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
#endif /* GL_VERSION_2_0 */

#ifndef GL_VERSION_3_0
#define GL_VERSION_3_0 1
	typedef unsigned short GLhalf;
	typedef void GLGenVertexArraysProc(GLsizei n, GLuint *arrays);
	typedef void GLBindVertexArrayProc(GLuint array);
#endif /* GL_VERSION_3_0 */
#endif // OPENGL_H
