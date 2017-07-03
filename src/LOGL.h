#ifndef LOGL_H
#define LOGL_H

#include "dqn.h"


struct LOGLBitmap
{
	u8    *memory;
	DqnV2i dim;
	i32    bytesPerPixel;
};

struct LOGLState
{
	u32 glShaderProgram;
	u32 vao;
	u32 vbo;
	u32 ebo;

	u32 shaderTransformLoc;

	u32 glTexIdContainer;
	u32 glTexIdFace;
};

void LOGL_Update    (struct PlatformInput *const input, struct PlatformMemory *const memory);
bool LOGL_LoadBitmap(DqnMemStack *const memStack, DqnMemStack *const tempStack, LOGLBitmap *const bitmap, const char *const path);

#endif
