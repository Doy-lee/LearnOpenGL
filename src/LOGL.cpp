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

		LOGLState *const state       = memory->state;
		LOGLContext *const glContext = &state->glContext;

		// Compile Shaders
		u32 vertexShader;
		u32 fragmentShader;
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
			glContext->shaderId = glCreateProgram();
			glAttachShader(glContext->shaderId, vertexShader);
			glAttachShader(glContext->shaderId, fragmentShader);
			glLinkProgram(glContext->shaderId);

			i32 success;
			char infoLog[512] = {};
			glGetProgramiv(glContext->shaderId, GL_LINK_STATUS, &success);
			if (!success)
			{
				glGetProgramInfoLog(glContext->shaderId, DQN_ARRAY_COUNT(infoLog), NULL, infoLog);
				if (!DQN_ASSERT_MSG(DQN_INVALID_CODE_PATH, "glLinkProgram() failed: %s", infoLog))
				{
					return;
				}
			}

			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);

			// Setup shader uniforms
			{
				glUseProgram(glContext->shaderId);
				u32 tex1Loc = glGetUniformLocation(glContext->shaderId, "inTexture1");
				u32 tex2Loc = glGetUniformLocation(glContext->shaderId, "inTexture2");
				DQN_ASSERT_MSG(tex1Loc != -1 && tex2Loc != -1, "tex1Loc: %d, tex2Loc: %d", tex1Loc,
				               tex2Loc);

				glUniform1i(tex1Loc, 0);
				glUniform1i(tex2Loc, 1);

				glContext->uniformProjectionLoc = glGetUniformLocation(glContext->shaderId, "projection");
				glContext->uniformViewLoc       = glGetUniformLocation(glContext->shaderId, "view");
				glContext->uniformModelLoc      = glGetUniformLocation(glContext->shaderId, "model");
				DQN_ASSERT_MSG((glContext->uniformProjectionLoc != -1 &&
				                glContext->uniformViewLoc       != -1 &&
				                glContext->uniformModelLoc      != -1),
				               "uniformProjectionLoc: %d, uniformViewLoc: %d, uniformModelLoc: %d",
				               glContext->uniformProjectionLoc, glContext->uniformViewLoc,
				               glContext->uniformModelLoc);
			}
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

			u32 indices[] = {
			    0, 1, 3, // first triangle
			    1, 2, 3  // second triangle
			};

			// Copy vertices into a vertex buffer and upload to GPU
			glGenBuffers(1, &glContext->vbo);
			glBindBuffer(GL_ARRAY_BUFFER, glContext->vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			// Copy indices into vertex buffer and upload to GPU
#if 0
			glGenBuffers(1, &glContext->ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glContext->ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
#endif

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

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bitmap->dim.w, bitmap->dim.h, 0, GL_RGBA,
				             GL_UNSIGNED_BYTE, bitmap->memory);
				glGenerateMipmap(GL_TEXTURE_2D);

			}
		}

		// Setup GL environment
		{
			glEnable(GL_DEPTH_TEST);

			f32 fovDegrees     = 45.0f;
			f32 aspectRatio    = input->screenDim.w / input->screenDim.h;
			DqnMat4 projection = DqnMat4_Perspective(fovDegrees, aspectRatio, 0.1f, 100.0f);

			// Upload to GPU
			glUniformMatrix4fv(glContext->uniformProjectionLoc, 1, GL_FALSE, (f32 *)projection.e);
		}

		// Setup state
		{
			state->cameraP     = DqnV3_3f(0, 0, 3);
			state->lastMouseP  = DqnV2_2i(input->mouse.x, input->mouse.y);
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

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, glContext->texIdContainer);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, glContext->texIdFace);

	glUseProgram(glContext->shaderId);
	glBindVertexArray(glContext->vao);

	// Make xform matrix
	{
		f32 degreesRotate = state->totalDt * 15.0f;
		f32 radiansRotate = DQN_DEGREES_TO_RADIANS(degreesRotate);

		DqnV3 cubePositions[] = {
		    DqnV3_3f(0.0f, 0.0f, 0.0f),     //
		    DqnV3_3f(2.0f, 5.0f, -15.0f),   //
		    DqnV3_3f(-1.5f, -2.2f, -2.5f),  //
		    DqnV3_3f(-3.8f, -2.0f, -12.3f), //
		    DqnV3_3f(2.4f, -0.4f, -3.5f),   //
		    DqnV3_3f(-1.7f, 3.0f, -7.5f),   //
		    DqnV3_3f(1.3f, -2.0f, -2.5f),   //
		    DqnV3_3f(1.5f, 2.0f, -2.5f),    //
		    DqnV3_3f(1.5f, 0.2f, -1.5f),    //
		    DqnV3_3f(-1.3f, 1.0f, -1.5f)    //
		};

		DqnV2 mouseOffset = DqnV2_2f(input->mouse.x - state->lastMouseP.x, state->lastMouseP.y - input->mouse.y);
		mouseOffset *= 0.1f;
		state->lastMouseP = DqnV2_2i(input->mouse.x, input->mouse.y);

		state->cameraYaw   -= mouseOffset.x;
		state->cameraPitch += mouseOffset.y;
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
		glUniformMatrix4fv(glContext->uniformViewLoc, 1, GL_FALSE, (f32 *)view.e);

		for (DqnV3 vec : cubePositions)
		{
			DqnMat4 model = DqnMat4_Translate(vec.x, vec.y, vec.z);
			model         = DqnMat4_Mul(model, DqnMat4_Rotate(radiansRotate, 1.0f, 0.3f, 0.5f));
			glUniformMatrix4fv(glContext->uniformModelLoc, 1, GL_FALSE, (f32 *)model.e);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
	}

	// glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

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
