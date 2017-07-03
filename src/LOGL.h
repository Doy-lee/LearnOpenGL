#ifndef LOGL_H
#define LOGL_H

#include "dqn.h"

void LOGL_Update(struct PlatformInput *const input, struct PlatformMemory *const memory);

struct LOGLState
{
	u32 glShaderProgram;
	u32 vao;
	u32 vbo;

	u32 ebo;
};

#endif
