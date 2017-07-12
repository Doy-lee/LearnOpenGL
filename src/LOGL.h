#ifndef LOGL_H
#define LOGL_H

#include "dqn.h"


struct LOGLBitmap
{
	u8    *memory;
	DqnV2i dim;
	i32    bytesPerPixel;
};

union LOGLPointLight {
	// Uniform locations
	i32 uniforms[7];
	struct
	{
		i32 pos;

		i32 ambient;
		i32 diffuse;
		i32 specular;

		i32 constant;
		i32 linear;
		i32 quadratic;
	};
};

struct LOGLContext
{
	i32 lightUniformProjectionLoc;
	i32 lightUniformViewLoc;
	i32 lightUniformModelLoc;

	i32 uniformProjectionLoc;
	i32 uniformViewLoc;
	i32 uniformModelLoc;

	i32 uniformMaterialDiffuse;
	i32 uniformMaterialSpecular;
	i32 uniformMaterialShininess;

	i32 uniformDirLightDir;
	i32 uniformDirLightAmbient;
	i32 uniformDirLightDiffuse;
	i32 uniformDirLightSpecular;

	LOGLPointLight uniformPointLights[4];
	i32 uniformSpotLightPos;
	i32 uniformSpotLightDir;
	i32 uniformSpotLightCutOff;
	i32 uniformSpotLightOuterCutOff;
	i32 uniformSpotLightAmbient;
	i32 uniformSpotLightDiffuse;
	i32 uniformSpotLightSpecular;
	i32 uniformSpotLightConstant;
	i32 uniformSpotLightLinear;
	i32 uniformSpotLightQuadratic;

	i32 uniformViewPos;

	u32 mainShaderId;
	u32 lightShaderId;

	u32 lightVao;
	u32 vao;
	u32 vbo;
	u32 ebo;

	u32 texIdCrateSpecular;
	u32 texIdCrate;
	u32 texIdContainer;
	u32 texIdFace;
};

struct LOGLState
{
	LOGLContext glContext;

	DqnV3 cameraP;
	f32   cameraYaw;
	f32   cameraPitch;

	f32 totalDt;
};

void LOGL_Update    (struct PlatformInput *const input, struct PlatformMemory *const memory);
bool LOGL_LoadBitmap(DqnMemStack *const memStack, DqnMemStack *const tempStack, LOGLBitmap *const bitmap, const char *const path);

#endif
