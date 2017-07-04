#ifndef LOGL_H
#define LOGL_H

#include "dqn.h"


struct LOGLBitmap
{
	u8    *memory;
	DqnV2i dim;
	i32    bytesPerPixel;
};

struct LOGLContext
{
	u32 uniformProjectionLoc;
	u32 uniformViewLoc;
	u32 uniformModelLoc;

	u32 shaderId;

	u32 vao;
	u32 vbo;
	u32 ebo;

	u32 texIdContainer;
	u32 texIdFace;
};

struct LOGLState
{
	LOGLContext glContext;
	f32 totalDt;
};

void LOGL_Update    (struct PlatformInput *const input, struct PlatformMemory *const memory);
bool LOGL_LoadBitmap(DqnMemStack *const memStack, DqnMemStack *const tempStack, LOGLBitmap *const bitmap, const char *const path);

#endif
