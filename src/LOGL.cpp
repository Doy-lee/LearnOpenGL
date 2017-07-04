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

FILE_SCOPE bool OpenGL_CompileShader(char *srcCode, u32 *const mainShaderId, enum ShaderTypeInternal shaderType)
{
	if (!DQN_ASSERT_MSG(shaderType != ShaderTypeInternal_Invalid, "shaderType: Is the invalid enum value") ||
	    !DQN_ASSERT_MSG(srcCode && mainShaderId, "Null argument(s): srcCode: %p, mainShaderId: %p", mainShaderId, srcCode))
	{
		return false;
	}

	if (shaderType == ShaderTypeInternal_Vertex)
		*mainShaderId = glCreateShader(GL_VERTEX_SHADER);
	else
		*mainShaderId = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(*mainShaderId, 1, &srcCode, NULL);
	glCompileShader(*mainShaderId);

	i32 success;
	glGetShaderiv(*mainShaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char infoLog[512] = {};
		glGetShaderInfoLog(*mainShaderId, DQN_ARRAY_COUNT(infoLog), NULL, infoLog);
		glDeleteShader(*mainShaderId);
		DQN_ASSERT_MSG(DQN_INVALID_CODE_PATH, "glCompileShader() failed: %s", infoLog);
	}

	return success;
}

bool OpenGL_LinkShaderProgram(u32 *const shaderId, const u32 vertexShader, const u32 fragmentShader)
{
	*shaderId = glCreateProgram();
	glAttachShader(*shaderId, vertexShader);
	glAttachShader(*shaderId, fragmentShader);
	glLinkProgram(*shaderId);

	i32 success;
	char infoLog[512] = {};
	glGetProgramiv(*shaderId, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(*shaderId, DQN_ARRAY_COUNT(infoLog), NULL, infoLog);
		DQN_ASSERT_MSG(DQN_INVALID_CODE_PATH, "glLinkProgram() failed: %s", infoLog);
		return false;
	}

	return true;
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

		LOGLState *const state       = memory->state;
		LOGLContext *const glContext = &state->glContext;

		// Compile Shaders
		u32 vertexShader;
		u32 fragmentShader;
		u32 lightFragmentShader;
		{
			char *vertexShaderSrc = R"DQN(
			#version 330 core
			layout(location = 0) in vec3 aPos;
			layout(location = 1) in vec3 aColor;
			layout(location = 2) in vec2 aTexCoord;

			uniform mat4 model;
			uniform mat4 view;
			uniform mat4 projection;

			out vec3 vertexColor;
			out vec2 texCoord;
			void main()
			{
			    gl_Position = projection * view * model * vec4(aPos, 1.0);
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
			uniform vec3 objectColor;
			uniform vec3 lightColor;

			void main()
			{
			    fragColor = mix(texture(inTexture1, texCoord), texture(inTexture2, texCoord), 0.2);
				fragColor = fragColor * vec4(lightColor * objectColor, 1.0);
			}
			)DQN";

			char *lightFragmentShaderSrc = R"DQN(
			#version 330 core
			out vec4 fragColor;

			void main()
			{
			    fragColor = vec4(1.0f);
			}
			)DQN";


			bool success = OpenGL_CompileShader(vertexShaderSrc,        &vertexShader,        ShaderTypeInternal_Vertex);
			success     |= OpenGL_CompileShader(fragmentShaderSrc,      &fragmentShader,      ShaderTypeInternal_Fragment);
			success     |= OpenGL_CompileShader(lightFragmentShaderSrc, &lightFragmentShader, ShaderTypeInternal_Fragment);
			if (!DQN_ASSERT_MSG(success, "OpenGL_CompileShader() failed.")) return;
		}

		// Link shaders
		{
			DQN_ASSERT_HARD(
			    OpenGL_LinkShaderProgram(&glContext->mainShaderId, vertexShader, fragmentShader));

			f32 fovDegrees     = 45.0f;
			f32 aspectRatio    = input->screenDim.w / input->screenDim.h;
			DqnMat4 projection = DqnMat4_Perspective(fovDegrees, aspectRatio, 0.1f, 100.0f);

			// Setup main shader uniforms
			{
				glUseProgram(glContext->mainShaderId);
				u32 tex1Loc = glGetUniformLocation(glContext->mainShaderId, "inTexture1");
				u32 tex2Loc = glGetUniformLocation(glContext->mainShaderId, "inTexture2");
				DQN_ASSERT_MSG(tex1Loc != -1 && tex2Loc != -1, "tex1Loc: %d, tex2Loc: %d", tex1Loc,
				               tex2Loc);

				glUniform1i(tex1Loc, 0);
				glUniform1i(tex2Loc, 1);

				glContext->uniformProjectionLoc = glGetUniformLocation(glContext->mainShaderId, "projection");
				glContext->uniformViewLoc       = glGetUniformLocation(glContext->mainShaderId, "view");
				glContext->uniformModelLoc      = glGetUniformLocation(glContext->mainShaderId, "model");
				DQN_ASSERT(glContext->uniformProjectionLoc != -1);
				DQN_ASSERT(glContext->uniformViewLoc != -1);
				DQN_ASSERT(glContext->uniformModelLoc != -1);

				glContext->uniformObjectColor = glGetUniformLocation(glContext->mainShaderId, "objectColor");
				glContext->uniformLightColor  = glGetUniformLocation(glContext->mainShaderId, "lightColor");
				DQN_ASSERT(glContext->uniformObjectColor != -1);
				DQN_ASSERT(glContext->uniformLightColor != -1);

				// Upload projection to GPU
				glUniformMatrix4fv(glContext->uniformProjectionLoc, 1, GL_FALSE, (f32 *)projection.e);
			}

			DQN_ASSERT_HARD(
			    OpenGL_LinkShaderProgram(&glContext->lightShaderId, vertexShader, lightFragmentShader));
			// Setup light shader uniforms
			{
				// Upload projection to GPU
				glUseProgram(glContext->lightShaderId);

				glContext->lightUniformProjectionLoc = glGetUniformLocation(glContext->lightShaderId, "projection");
				glContext->lightUniformViewLoc       = glGetUniformLocation(glContext->lightShaderId, "view");
				glContext->lightUniformModelLoc      = glGetUniformLocation(glContext->lightShaderId, "model");
				DQN_ASSERT(glContext->lightUniformProjectionLoc != -1);
				DQN_ASSERT(glContext->lightUniformViewLoc != -1);
				DQN_ASSERT(glContext->lightUniformModelLoc != -1);

				glUniformMatrix4fv(glContext->lightUniformProjectionLoc, 1, GL_FALSE, (f32 *)projection.e);
			}

			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
			glDeleteShader(lightFragmentShader);
		}

		// Init geometry
		{
			glGenVertexArrays(1, &glContext->vao);
			glBindVertexArray(glContext->vao);

			f32 vertices[] = {
			    // positions          // color  // tex coords
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 0.0f, //
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  1.0f, 0.0f, //
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f, //
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f, //
			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 0.0f, //

			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f, //
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 1.0f, //
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 1.0f, //
			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  0.0f, 1.0f, //
			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f, //

			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //
			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f, //
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //
			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f, //
			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //

			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f, //
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f, //
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //

			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f, //
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //
			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f, //
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //

			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f, //
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f, //
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f, //
			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f, //
			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f //
			};

			// Copy vertices into a vertex buffer and upload to GPU
			glGenBuffers(1, &glContext->vbo);
			glBindBuffer(GL_ARRAY_BUFFER, glContext->vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			// Copy indices into vertex buffer and upload to GPU
#if 0
			u32 indices[] = {
			    0, 1, 3, // first triangle
			    1, 2, 3  // second triangle
			};

			glGenBuffers(1, &glContext->ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glContext->ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
#endif

			const u32 numPosComponents   = 3;
			const u32 numColorComponents = 3;
			const u32 numTexComponents   = 2;
			const u32 stride             = sizeof(f32) * (numPosComponents + numColorComponents + numTexComponents);
			GLboolean isNormalised = GL_FALSE;

			// Describe the vertex layout for OpenGL
			if (1)
			{
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

			// Init lights
			{
				glGenVertexArrays(1, &glContext->lightVao);
				glBindVertexArray(glContext->lightVao);
				glBindBuffer(GL_ARRAY_BUFFER, glContext->vbo);

				// Use just the pos data from the vertices array array
				{
					const u32 shaderInLoc = 0;
					glVertexAttribPointer(shaderInLoc, numPosComponents, GL_FLOAT, isNormalised, stride, NULL);
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
				glGenTextures(1, &glContext->texIdContainer);
				glBindTexture(GL_TEXTURE_2D, glContext->texIdContainer);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bitmap->dim.w, bitmap->dim.h, 0, GL_RGB,
				             GL_UNSIGNED_BYTE, bitmap->memory);
				glGenerateMipmap(GL_TEXTURE_2D);
			}

			*bitmap = {};
			if (LOGL_LoadBitmap(mainStack, tempStack, bitmap, "awesomeface.png"))
			{
				glGenTextures(1, &glContext->texIdFace);
				glBindTexture(GL_TEXTURE_2D, glContext->texIdFace);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bitmap->dim.w, bitmap->dim.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->memory);
		        glGenerateMipmap(GL_TEXTURE_2D);

			}
		}

		// Setup GL environment
		{
			glEnable(GL_DEPTH_TEST);
			// glEnable(GL_CULL_FACE);
		}

		// Setup state
		{
			state->cameraP     = DqnV3_3f(0, 0, 3);
			state->cameraYaw   = 0;
			state->cameraPitch = 0;
		}
	}

	LOGLState *const state       = memory->state;
	LOGLContext *const glContext = &state->glContext;
	state->totalDt += input->deltaForFrame;

	glClearColor(0.5f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Calculate view matrix/camera code
	if (1)
	{
		state->cameraYaw   -= (input->mouse.dx * 0.1f);
		state->cameraPitch -= (input->mouse.dy * 0.1f);
		state->cameraPitch = DqnMath_Clampf(state->cameraPitch, -89, 89);

		DqnV3 cameraFront = {};
		cameraFront.x     = cosf(DQN_DEGREES_TO_RADIANS(state->cameraYaw)) * cosf(DQN_DEGREES_TO_RADIANS(state->cameraPitch));
		cameraFront.y     = sinf(DQN_DEGREES_TO_RADIANS(state->cameraPitch));
		cameraFront.z     = sinf(DQN_DEGREES_TO_RADIANS(state->cameraYaw)) * cosf(DQN_DEGREES_TO_RADIANS(state->cameraPitch));
		cameraFront       = DqnV3_Normalise(cameraFront);

		auto cameraUp     = DqnV3_3f(0, 1, 0);
		DqnV3 cameraRight = DqnV3_Normalise(DqnV3_Cross(cameraFront, cameraUp));

		const f32 cameraSpeed = 10.0f * input->deltaForFrame;
		if (input->key_w.endedDown) state->cameraP -= (cameraFront * cameraSpeed);
		if (input->key_s.endedDown) state->cameraP += (cameraFront * cameraSpeed);
		if (input->key_a.endedDown) state->cameraP -= (cameraRight * cameraSpeed);
		if (input->key_d.endedDown) state->cameraP += (cameraRight * cameraSpeed);

		DqnMat4 view = DqnMat4_LookAt(state->cameraP, state->cameraP + cameraFront, cameraUp);
		// Render model code
		if (1)
		{
			auto lightColor = DqnV3_3f(1, 1, 1);
			auto objColor   = DqnV3_3f(1.0f, 0.5f, 0.31f);

			// Cube
			{
				f32 degreesRotate = state->totalDt * 15.0f;
				f32 radiansRotate = DQN_DEGREES_TO_RADIANS(degreesRotate);

				glUseProgram(glContext->mainShaderId);
				glBindVertexArray(glContext->vao);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, glContext->texIdContainer);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, glContext->texIdFace);

				DqnMat4 model = DqnMat4_Translate(0, 0, 0);
				model         = DqnMat4_Mul(model, DqnMat4_Rotate(radiansRotate, 1.0f, 0.3f, 0.5f));
				glUniformMatrix4fv(glContext->uniformModelLoc, 1, GL_FALSE, (f32 *)model.e);
				glUniformMatrix4fv(glContext->uniformViewLoc, 1, GL_FALSE, (f32 *)view.e);

				glUniform3fv(glContext->uniformObjectColor, 1, (f32 *)objColor.e);
				glUniform3fv(glContext->uniformLightColor, 1, (f32 *)lightColor.e);

				glDrawArrays(GL_TRIANGLES, 0, 36);
			}

			// Light source
			{
				glUseProgram(glContext->lightShaderId);
				glBindVertexArray(glContext->lightVao);

				DqnMat4 model = DqnMat4_Translate(1.2f, 1.0f, 2.0f);
				glUniformMatrix4fv(glContext->lightUniformModelLoc, 1, GL_FALSE, (f32 *)model.e);
				glUniformMatrix4fv(glContext->lightUniformViewLoc, 1, GL_FALSE, (f32 *)view.e);

				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}
	}

	// glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
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
