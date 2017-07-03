#include "LOGL.h"
#include "LOGLPlatform.h"
#include "OpenGL.h"

#define DQN_PLATFORM_HEADER
#include "dqn.h"

#include <math.h>
enum ShaderTypeInternal
{
	ShaderTypeInternal_Invalid,
	ShaderTypeInternal_Vertex,
	ShaderTypeInternal_Fragment,
};

FILE_SCOPE bool OpenGL_CompileShader(char *srcCode, u32 *const shaderId, enum ShaderTypeInternal shaderType)
{
	if (!DQN_ASSERT_MSG(shaderType != ShaderTypeInternal_Invalid, "shaderType: Is the invalid enum value") ||
	    !DQN_ASSERT_MSG(srcCode && shaderId, "Null argument(s): srcCode: %p, shaderId: %p", shaderId, srcCode))
	{
		return false;
	}

	if (shaderType == ShaderTypeInternal_Vertex)
		*shaderId = glCreateShader(GL_VERTEX_SHADER);
	else
		*shaderId = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(*shaderId, 1, &srcCode, NULL);
	glCompileShader(*shaderId);

	i32 success;
	glGetShaderiv(*shaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char infoLog[512] = {};
		glGetShaderInfoLog(*shaderId, DQN_ARRAY_COUNT(infoLog), NULL, infoLog);
		glDeleteShader(*shaderId);
		DQN_ASSERT_MSG(DQN_INVALID_CODE_PATH, "glCompileShader() failed: %s", infoLog);
	}

	return success;
}

void LOGL_Update(struct PlatformInput *const input, struct PlatformMemory *const memory)
{
	if (!memory->state)
	{
		memory->state = (LOGLState *)memory->stack.Push(sizeof(*memory->state));
		if (!memory->state) return;

		LOGLState *state = memory->state;

		// Compile Shaders
		u32 vertexShader;
		u32 fragmentShader;
		{
			char *vertexShaderSrc = R"DQN(
			#version 330 core
			layout(location = 0) in vec3 aPos;
			layout(location = 1) in vec3 aColor;

			out vec3 vertexColor;
			void main()
			{
			    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0f);
				vertexColor = aColor;
			}
			)DQN";

			char *fragmentShaderSrc = R"DQN(
			#version 330 core
			out vec4 fragColor;
			in  vec3 vertexColor;

			void main()
			{
			    fragColor = vec4(vertexColor, 1.0);
			}
			)DQN";

			bool success = OpenGL_CompileShader(vertexShaderSrc,   &vertexShader,   ShaderTypeInternal_Vertex);
			success     |= OpenGL_CompileShader(fragmentShaderSrc, &fragmentShader, ShaderTypeInternal_Fragment);
			if (!DQN_ASSERT_MSG(success, "OpenGL_CompileShader() failed.")) return;
		}

		// Link shaders
		{
			state->glShaderProgram = glCreateProgram();
			glAttachShader(state->glShaderProgram, vertexShader);
			glAttachShader(state->glShaderProgram, fragmentShader);
			glLinkProgram(state->glShaderProgram);

			i32 success;
			char infoLog[512] = {};
			glGetProgramiv(state->glShaderProgram, GL_LINK_STATUS, &success);
			if (!success)
			{
				glGetProgramInfoLog(state->glShaderProgram, DQN_ARRAY_COUNT(infoLog), NULL, infoLog);
				if (!DQN_ASSERT_MSG(DQN_INVALID_CODE_PATH, "glLinkProgram() failed: %s", infoLog))
				{
					return;
				}
			}

			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
		}

		// Init geometry
		{
			glGenVertexArrays(1, &state->vao);
			glBindVertexArray(state->vao);

			f32 vertices[] = {
			    // positions        // colors
			     0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom right
			    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, // bottom left
			     0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f  // top
			};

			// Copy vertices into a vertex buffer and upload to GPU
			glGenBuffers(1, &state->vbo);
			glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			// Describe the vertex layout for OpenGL
			{
				const u32 numPosComponents   = 3;
				const u32 numColorComponents = 3;
				const u32 stride       = sizeof(f32) * (numPosComponents + numColorComponents);
				GLboolean isNormalised = GL_FALSE;

				// Set the vertex attributes pointer for pos
				{
					const u32 vertexShaderPosIn = 0;
					glVertexAttribPointer(vertexShaderPosIn, numPosComponents, GL_FLOAT, isNormalised, stride, NULL);
					glEnableVertexAttribArray(vertexShaderPosIn);
				}

				// Set the vertex attributes pointer for color
				{
					const u32 vertexShaderColorIn = 1;
					void *vertexShaderColorOffset = (void *)(numPosComponents * sizeof(f32));
					glVertexAttribPointer(vertexShaderColorIn, numColorComponents, GL_FLOAT, isNormalised, stride, vertexShaderColorOffset);
					glEnableVertexAttribArray(vertexShaderColorIn);
				}
			}
		}
	}
	LOGLState *state = memory->state;

	glClearColor(0.5f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUseProgram(state->glShaderProgram);
	glBindVertexArray(state->vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	(void)input;
}
