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

#define OGL_ASSERT(func) func; OpenGL_AssertError()
FILE_SCOPE inline GLenum OpenGL_AssertError()
{
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR)
	{
		switch (errorCode)
		{
			case GL_INVALID_ENUM:                  DQN_ASSERT((bool)GL_INVALID_ENUM);                  break;
			case GL_INVALID_VALUE:                 DQN_ASSERT((bool)GL_INVALID_VALUE);                 break;
			case GL_INVALID_OPERATION:             DQN_ASSERT((bool)GL_INVALID_OPERATION);             break;
			case GL_STACK_OVERFLOW:                DQN_ASSERT((bool)GL_STACK_OVERFLOW);                break;
			case GL_STACK_UNDERFLOW:               DQN_ASSERT((bool)GL_STACK_UNDERFLOW);               break;
			case GL_OUT_OF_MEMORY:                 DQN_ASSERT((bool)GL_OUT_OF_MEMORY);                 break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: DQN_ASSERT((bool)GL_INVALID_FRAMEBUFFER_OPERATION); break;
		}
	}

	return errorCode;
}

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
			layout(location = 3) in vec3 aNormal;

			uniform mat4 model;
			uniform mat4 view;
			uniform mat4 projection;

			out vec3 vertexColor;
			out vec2 texCoord;
			out vec3 normal;
			out vec3 fragPos;

			void main()
			{
				fragPos     = vec3(model * vec4(aPos, 1.0));
			    gl_Position = projection * view * vec4(fragPos, 1.0f);
			    vertexColor = aColor;
			    texCoord    = aTexCoord;
				normal      = mat3(transpose(inverse(model))) * aNormal;
			}
			)DQN";

			char *fragmentShaderSrc = R"DQN(
			#version 330 core
			out vec4 fragColor;

			in  vec3 vertexColor;
			in  vec2 texCoord;
			in  vec3 normal;
			in  vec3 fragPos;

			struct Material
			{
				sampler2D diffuse;
				sampler2D specular;
				float     shininess;
			};

			struct Light {
				vec3 position;
				vec3 direction;
				float cutOff;
				float outerCutOff;

				vec3 diffuse;
				vec3 ambient;
				vec3 specular;

				float constant;
				float linear;
				float quadratic;
			};

			uniform Material material;
			uniform Light    light;
			uniform vec3     viewPos;

			void main()
			{
				vec3 ambient  = light.ambient * texture(material.diffuse, texCoord).rgb;
				vec3 lightDir = normalize(light.position - fragPos);
				vec3 norm     = normalize(normal);

				// Diffuse, the angle between the light direction and surface normal
				float diffuseVal = (max(dot(norm, lightDir), 0));
				vec3 diffuse     = light.diffuse * diffuseVal * texture(material.diffuse, texCoord).rgb;

				// Specular, the angle between the view and the reflection of the light vector
				// NOTE(doyle): Reflect first arg expects the vector to point from the light src to fragment, so reverse it
				vec3  viewDir     = normalize(viewPos - fragPos);
				vec3  reflectDir  = reflect(-lightDir, norm);
				float specularVal = pow(max(dot(viewDir, reflectDir), 0), material.shininess);
				vec3  specular    = light.specular * specularVal * texture(material.specular, texCoord).rgb;

				// spotlight w/ soft edges
				float theta     = dot(lightDir, normalize(light.direction));
				float epsilon   = light.cutOff - light.outerCutOff;
				float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
				diffuse  *= intensity;
				specular *= intensity;

				// Calculate attenuation
				float distance    = length(light.position - fragPos);
				float attenuation = 1.0f / (light.constant + (light.linear * distance) + (light.quadratic * distance * distance));
				ambient  *= attenuation;
				diffuse  *= attenuation;
				specular *= attenuation;

				fragColor = vec4(ambient + diffuse + specular, 1.0f);
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

				// MVP matrix
				{
					glContext->uniformProjectionLoc = glGetUniformLocation(glContext->mainShaderId, "projection");
					glContext->uniformViewLoc       = glGetUniformLocation(glContext->mainShaderId, "view");
					glContext->uniformModelLoc      = glGetUniformLocation(glContext->mainShaderId, "model");
					DQN_ASSERT(glContext->uniformProjectionLoc != -1);
					DQN_ASSERT(glContext->uniformViewLoc != -1);
					DQN_ASSERT(glContext->uniformModelLoc != -1);
				}

				glContext->uniformViewPos = glGetUniformLocation(glContext->mainShaderId, "viewPos");
				DQN_ASSERT(glContext->uniformViewPos != -1);

				// Material uniforms
				{
					glContext->uniformMaterialDiffuse   = glGetUniformLocation(glContext->mainShaderId, "material.diffuse");
					glContext->uniformMaterialSpecular  = glGetUniformLocation(glContext->mainShaderId, "material.specular");
					glContext->uniformMaterialShininess = glGetUniformLocation(glContext->mainShaderId, "material.shininess");
					DQN_ASSERT(glContext->uniformMaterialDiffuse != -1);
					DQN_ASSERT(glContext->uniformMaterialSpecular != -1);
					DQN_ASSERT(glContext->uniformMaterialShininess != -1);

					// Set the uniform sampler2D to use GL_TEXTURE0 and 1
					glUniform1i(glContext->uniformMaterialDiffuse, 0);
					glUniform1i(glContext->uniformMaterialSpecular, 1);
				}

				// Setup lights
				{
					glContext->uniformLightAmbient  = glGetUniformLocation(glContext->mainShaderId, "light.ambient");
					glContext->uniformLightDiffuse  = glGetUniformLocation(glContext->mainShaderId, "light.diffuse");
					glContext->uniformLightSpecular = glGetUniformLocation(glContext->mainShaderId, "light.specular");
					DQN_ASSERT(glContext->uniformLightAmbient != -1);
					DQN_ASSERT(glContext->uniformLightDiffuse != -1);
					DQN_ASSERT(glContext->uniformLightSpecular != -1);

					glContext->uniformLightConstant  = glGetUniformLocation(glContext->mainShaderId, "light.constant");
					glContext->uniformLightLinear    = glGetUniformLocation(glContext->mainShaderId, "light.linear");
					glContext->uniformLightQuadratic = glGetUniformLocation(glContext->mainShaderId, "light.quadratic");
					DQN_ASSERT(glContext->uniformLightConstant != -1);
					DQN_ASSERT(glContext->uniformLightLinear != -1);
					DQN_ASSERT(glContext->uniformLightQuadratic != -1);

					glContext->uniformLightPos         = glGetUniformLocation(glContext->mainShaderId, "light.position");
					glContext->uniformLightDir         = glGetUniformLocation(glContext->mainShaderId, "light.direction");
					glContext->uniformLightCutOff      = glGetUniformLocation(glContext->mainShaderId, "light.cutOff");
					glContext->uniformLightOuterCutOff = glGetUniformLocation(glContext->mainShaderId, "light.outerCutOff");
					DQN_ASSERT(glContext->uniformLightPos != -1);
					DQN_ASSERT(glContext->uniformLightDir != -1);
					DQN_ASSERT(glContext->uniformLightCutOff != -1);
					DQN_ASSERT(glContext->uniformLightOuterCutOff != -1);
				}

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
			    // positions          // color  // tex coords //normals
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 0.0f,   0.0f,  0.0f, -1.0f,//
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  1.0f, 0.0f,   0.0f,  0.0f, -1.0f,//
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f,   0.0f,  0.0f, -1.0f,//
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f,   0.0f,  0.0f, -1.0f,//
			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   0.0f,  0.0f, -1.0f,//
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 0.0f,   0.0f,  0.0f, -1.0f,//

			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f,   0.0f,  0.0f, 1.0f, //
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   0.0f,  0.0f, 1.0f, //
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 1.0f,   0.0f,  0.0f, 1.0f, //
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 1.0f,   0.0f,  0.0f, 1.0f, //
			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  0.0f, 1.0f,   0.0f,  0.0f, 1.0f, //
			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f,   0.0f,  0.0f, 1.0f, //

			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   1.0f,  0.0f,  0.0f,//
			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f,   1.0f,  0.0f,  0.0f,//
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   1.0f,  0.0f,  0.0f,//
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   1.0f,  0.0f,  0.0f,//
			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f,   1.0f,  0.0f,  0.0f,//
			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   1.0f,  0.0f,  0.0f,//

			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   1.0f,  0.0f,  0.0f,//
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f,   1.0f,  0.0f,  0.0f,//
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   1.0f,  0.0f,  0.0f,//
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   1.0f,  0.0f,  0.0f,//
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f,   1.0f,  0.0f,  0.0f,//
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   1.0f,  0.0f,  0.0f,//

			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   0.0f, -1.0f,  0.0f,//
			     0.5f, -0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f,   0.0f, -1.0f,  0.0f,//
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   0.0f, -1.0f,  0.0f,//
			     0.5f, -0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   0.0f, -1.0f,  0.0f,//
			    -0.5f, -0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f,   0.0f, -1.0f,  0.0f,//
			    -0.5f, -0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   0.0f, -1.0f,  0.0f,//

			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   0.0f,  1.0f,  0.0f,//
			     0.5f,  0.5f, -0.5f,  1, 1, 1,  1.0f, 1.0f,   0.0f,  1.0f,  0.0f,//
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   0.0f,  1.0f,  0.0f,//
			     0.5f,  0.5f,  0.5f,  1, 1, 1,  1.0f, 0.0f,   0.0f,  1.0f,  0.0f,//
			    -0.5f,  0.5f,  0.5f,  1, 1, 1,  0.0f, 0.0f,   0.0f,  1.0f,  0.0f,//
			    -0.5f,  0.5f, -0.5f,  1, 1, 1,  0.0f, 1.0f,   0.0f,  1.0f,  0.0f,//
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

			const u32 numPosComponents    = 3;
			const u32 numColorComponents  = 3;
			const u32 numTexComponents    = 2;
			const u32 numNormalComponents = 3;
			const u32 stride       = sizeof(f32) * (numPosComponents + numColorComponents + numTexComponents + numNormalComponents);
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

				// Set the vertex attributes pointer for normals
				{
					const u32 shaderInLoc = 3;
					void *vertexOffset = (void *)((numColorComponents + numPosComponents + numTexComponents) * sizeof(f32));
					glVertexAttribPointer(shaderInLoc, numNormalComponents, GL_FLOAT, isNormalised, stride, vertexOffset);
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

			*bitmap = {};
			if (LOGL_LoadBitmap(mainStack, tempStack, bitmap, "container2.png"))
			{
				glGenTextures(1, &glContext->texIdCrate);
				glBindTexture(GL_TEXTURE_2D, glContext->texIdCrate);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bitmap->dim.w, bitmap->dim.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->memory);
				glGenerateMipmap(GL_TEXTURE_2D);
			}

			*bitmap = {};
			if (LOGL_LoadBitmap(mainStack, tempStack, bitmap, "container2_specular.png"))
			{
				glGenTextures(1, &glContext->texIdCrateSpecular);
				glBindTexture(GL_TEXTURE_2D, glContext->texIdCrateSpecular);

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

	glClearColor(0.1f, 0.1f, 0.1f, 0.1f);
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
			DqnV3 lightPos = DqnV3_3f(1.2f, 1.0f, 2.0f);
			// Light source
			{
				glUseProgram(glContext->lightShaderId);
				glBindVertexArray(glContext->lightVao);

				DqnMat4 model = DqnMat4_TranslateV3(lightPos);
				model         = DqnMat4_Mul(model, DqnMat4_ScaleV3(DqnV3_1f(0.25f)));
				glUniformMatrix4fv(glContext->lightUniformModelLoc, 1, GL_FALSE, (f32 *)model.e);
				glUniformMatrix4fv(glContext->lightUniformViewLoc, 1, GL_FALSE, (f32 *)view.e);

				glDrawArrays(GL_TRIANGLES, 0, 36);
			}

			// Cube
			{
				f32 degreesRotate = state->totalDt * 15.0f;
				f32 radiansRotate = DQN_DEGREES_TO_RADIANS(degreesRotate);

				glUseProgram(glContext->mainShaderId);
				glBindVertexArray(glContext->vao);

				// Activate texture bindings for cube
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, glContext->texIdCrate);
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, glContext->texIdCrateSpecular);
				}

				// Set view data
				glUniformMatrix4fv(glContext->uniformViewLoc, 1, GL_FALSE, (f32 *)view.e);
				glUniform3fv(glContext->uniformViewPos, 1, state->cameraP.e);

				// Set material uniforms
				glUniform1f(glContext->uniformMaterialShininess, 32.0f);

				// Setup light
				{
					// Set physical light uniforms
					glUniform3f(glContext->uniformLightAmbient, 0.1f, 0.1f, 0.1f);
					glUniform3f(glContext->uniformLightDiffuse, 0.8f, 0.8f, 0.8f);
					glUniform3f(glContext->uniformLightSpecular, 1.0f, 1.0f, 1.0f);

					// Set light attenuation, a quadratic falloff in intensity
					glUniform1f(glContext->uniformLightConstant, 1.0f);
					glUniform1f(glContext->uniformLightLinear, 0.09f);
					glUniform1f(glContext->uniformLightQuadratic, 0.032f);

					// Set spot light data
					glUniform3fv(glContext->uniformLightPos, 1, state->cameraP.e);
					glUniform3fv(glContext->uniformLightDir, 1, cameraFront.e);
					glUniform1f(glContext->uniformLightCutOff,      cosf(DQN_DEGREES_TO_RADIANS(12.5f)));
					glUniform1f(glContext->uniformLightOuterCutOff, cosf(DQN_DEGREES_TO_RADIANS(17.5f)));
				}

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

				for (DqnV3 vec : cubePositions)
				{
					// Set transform matrices
					DqnMat4 model = DqnMat4_TranslateV3(vec);
					model         = DqnMat4_Mul(model, DqnMat4_Rotate(radiansRotate, 1.0f, 0.3f, 0.5f));
					glUniformMatrix4fv(glContext->uniformModelLoc, 1, GL_FALSE, (f32 *)model.e);

					glDrawArrays(GL_TRIANGLES, 0, 36);
				}
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
