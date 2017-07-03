#include "LOGL.h"
#include "LOGLPlatform.h"
#include "OpenGL.h"

#define DQN_PLATFORM_HEADER
#include "dqn.h"

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

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
	DqnMemStack *const mainStack = &memory->mainStack;
	DqnMemStack *const tempStack = &memory->tempStack;
	auto tempRegion              = tempStack->TempRegionGuard();

	if (!memory->state)
	{
		memory->state = (LOGLState *)memory->mainStack.Push(sizeof(*memory->state));
		if (!memory->state) return;

		LOGLState *const state = memory->state;

		// Compile Shaders
		u32 vertexShader;
		u32 fragmentShader;
		{
			char *vertexShaderSrc = R"DQN(
			#version 330 core
			layout(location = 0) in vec3 aPos;
			layout(location = 1) in vec3 aColor;
			layout(location = 2) in vec2 aTexCoord;

			uniform mat4 transform;

			out vec3 vertexColor;
			out vec2 texCoord;
			void main()
			{
			    gl_Position = transform * vec4(aPos, 1.0);
			    vertexColor = aColor;
			    texCoord    = vec2(aTexCoord.x, aTexCoord.y);
			}
			)DQN";

			char *fragmentShaderSrc = R"DQN(
			#version 330 core
			out vec4 fragColor;

			in  vec3 vertexColor;
			in  vec2 texCoord;

			uniform sampler2D inTexture1;
			uniform sampler2D inTexture2;

			void main()
			{
			    fragColor = mix(texture(inTexture1, texCoord), texture(inTexture2, texCoord), 0.2);
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

			// Setup shader uniforms
			{
				glUseProgram(state->glShaderProgram);
				u32 tex1Loc = glGetUniformLocation(state->glShaderProgram, "inTexture1");
				u32 tex2Loc = glGetUniformLocation(state->glShaderProgram, "inTexture2");
				DQN_ASSERT_MSG(tex1Loc != -1 && tex2Loc != -1, "tex1Loc: %d, tex2Loc: %d", tex1Loc,
				               tex2Loc);

				glUniform1i(tex1Loc, 0);
				glUniform1i(tex2Loc, 1);

				state->shaderTransformLoc = glGetUniformLocation(state->glShaderProgram, "transform");
				DQN_ASSERT(state->shaderTransformLoc != -1);
			}
		}

		// Init geometry
		{
			glGenVertexArrays(1, &state->vao);
			glBindVertexArray(state->vao);

			f32 vertices[] = {
			    // positions         // colors         // texture coords
			     0.5f,   0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right
			     0.5f,  -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right
			    -0.5f,  -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left
			    -0.5f,   0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f  // top left
			};

			u32 indices[] = {
			    0, 1, 3, // first triangle
			    1, 2, 3  // second triangle
			};

			// Copy vertices into a vertex buffer and upload to GPU
			glGenBuffers(1, &state->vbo);
			glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			// Copy indices into vertex buffer and upload to GPU
			glGenBuffers(1, &state->ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

			// Describe the vertex layout for OpenGL
			{
				const u32 numPosComponents   = 3;
				const u32 numColorComponents = 3;
				const u32 numTexComponents   = 2;
				const u32 stride =
				    sizeof(f32) * (numPosComponents + numColorComponents + numTexComponents);
				GLboolean isNormalised = GL_FALSE;

				// Set the vertex attributes pointer for pos
				{
					const u32 shaderInLoc = 0;
					glVertexAttribPointer(shaderInLoc, numPosComponents, GL_FLOAT, isNormalised, stride, NULL);
					glEnableVertexAttribArray(shaderInLoc);
				}

				// Set the vertex attributes pointer for color
				{
					const u32 shaderInLoc = 1;
					void *vertexOffset = (void *)(numPosComponents * sizeof(f32));
					glVertexAttribPointer(shaderInLoc, numColorComponents, GL_FLOAT, isNormalised, stride, vertexOffset);
					glEnableVertexAttribArray(shaderInLoc);
				}

				// Set the vertex attributes pointer for textures
				{
					const u32 shaderInLoc = 2;
					void *vertexOffset = (void *)((numColorComponents + numPosComponents) * sizeof(f32));
					glVertexAttribPointer(shaderInLoc, numTexComponents, GL_FLOAT, isNormalised, stride, vertexOffset);
					glEnableVertexAttribArray(shaderInLoc);
				}
			}
		}

		// Load assets
		{
			stbi_set_flip_vertically_on_load(true);
			auto regionGuard   = mainStack->TempRegionGuard();
			LOGLBitmap *bitmap = (LOGLBitmap *)mainStack->Push(sizeof(LOGLBitmap));
			if (LOGL_LoadBitmap(mainStack, tempStack, bitmap, "container.jpg"))
			{
				glGenTextures(1, &state->glTexIdContainer);
				glBindTexture(GL_TEXTURE_2D, state->glTexIdContainer);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bitmap->dim.w, bitmap->dim.h, 0, GL_RGB,
				             GL_UNSIGNED_BYTE, bitmap->memory);
				glGenerateMipmap(GL_TEXTURE_2D);
			}

			*bitmap = {};
			if (LOGL_LoadBitmap(mainStack, tempStack, bitmap, "awesomeface.png"))
			{
				glGenTextures(1, &state->glTexIdFace);
				glBindTexture(GL_TEXTURE_2D, state->glTexIdFace);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bitmap->dim.w, bitmap->dim.h, 0, GL_RGBA,
				             GL_UNSIGNED_BYTE, bitmap->memory);
				glGenerateMipmap(GL_TEXTURE_2D);

			}
		}
	}
	LOGLState *const state = memory->state;

	glClearColor(0.5f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->glTexIdContainer);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, state->glTexIdFace);

	// Make transform matrix
	{
		LOCAL_PERSIST f32 rotation = 0;
		rotation += (input->deltaForFrame * 5.0f);

		DqnMat4 translateMatrix = DqnMat4_Translate(0.5f, -0.5f, 0.0f);
		DqnMat4 rotateMatrix = DqnMat4_Rotate(DQN_DEGREES_TO_RADIANS(rotation), 0, 0, 1.0f);
		DqnMat4 transform   = DqnMat4_Mul(translateMatrix, rotateMatrix);
		glUniformMatrix4fv(state->shaderTransformLoc, 1, GL_FALSE, (f32 *)transform.e);
	}

	glUseProgram(state->glShaderProgram);
	glBindVertexArray(state->vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	(void)input;
}

////////////////////////////////////////////////////////////////////////////////
// Bitmap Loading Code
////////////////////////////////////////////////////////////////////////////////
bool LOGL_LoadBitmap(DqnMemStack *const memStack, DqnMemStack *const tempStack, LOGLBitmap *const bitmap,
                     const char *const path)
{
	if (!bitmap || !memStack || !tempStack) return false;

	// Get file size necessary
	size_t fileSize;
	u8 *rawBytes                            = NULL;
	DqnMemStackTempRegionGuard tmpMemRegion = tempStack->TempRegionGuard();
	{
		if (!DQN_ASSERT(DqnFile_GetFileSize(path, &fileSize))) return false;

		// Load raw file bytes
		rawBytes = (u8 *)tempStack->Push(fileSize);

		size_t bytesRead;
		if (!DqnFile_ReadEntireFile(path, rawBytes, fileSize, &bytesRead))
			return false;

		if (!DQN_ASSERT_MSG(bytesRead == fileSize, "bytesRead: %d, fileSize: %d", bytesRead, fileSize))
			return false;
	}

	// Use stb to interpret bytes as pixels
	{
		LOGLBitmap tmp = {};
		tmp.memory     = stbi_load_from_memory(rawBytes, (i32)fileSize, &tmp.dim.w, &tmp.dim.h,
		                                   &tmp.bytesPerPixel, 0);
		if (!tmp.memory)
		{
			const char *failReason = stbi_failure_reason();
			DQN_ASSERT_MSG(DQN_INVALID_CODE_PATH, "stbi_load_from_memory() failed: %s", failReason);
			return false;
		}

		// NOTE: We do this since stbi does alot of mallocing itself, we just let it use malloc and
		// whatnot, then do our own allocation
		*bitmap           = tmp;
		size_t bitmapSize = bitmap->dim.w * bitmap->dim.h * bitmap->bytesPerPixel;
		bitmap->memory    = (u8 *)memStack->Push(bitmapSize);
		if (!bitmap->memory) return false;

		for (u32 i = 0; i < bitmapSize; i++)
			bitmap->memory[i] = tmp.memory[i];

		free(tmp.memory);
	}

	return true;
}
