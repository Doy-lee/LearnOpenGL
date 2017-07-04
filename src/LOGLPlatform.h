#ifndef LOGL_PLATFORM_H
#define LOGL_PLATFORM_H

#include "OpenGL.h"
#include "dqn.h"

enum PlatformKey
{
	PlatformKey_Up,
	PlatformKey_Down,
	PlatformKey_Left,
	PlatformKey_Right,

	PlatformKey_1,
	PlatformKey_2,
	PlatformKey_3,
	PlatformKey_4,

	PlatformKey_q,
	PlatformKey_w,
	PlatformKey_e,
	PlatformKey_r,

	PlatformKey_a,
	PlatformKey_s,
	PlatformKey_d,
	PlatformKey_f,

	PlatformKey_z,
	PlatformKey_x,
	PlatformKey_c,
	PlatformKey_v,

	PlatformKey_Count,
};

struct PlatformKeyState
{
	bool endedDown;
	u32 halfTransitionCount;
};

struct PlatformMouse
{
	i32 dx;
	i32 dy;
	PlatformKeyState leftBtn;
	PlatformKeyState rightBtn;
};

struct PlatformMemory
{
	DqnMemStack      mainStack;
	DqnMemStack      tempStack;
	struct LOGLState *state;
};

struct PlatformInput
{
	f32 deltaForFrame;
	PlatformMouse mouse;

	DqnV2 screenDim;
	union {
		PlatformKeyState key[PlatformKey_Count];
		struct
		{
			PlatformKeyState key_up;
			PlatformKeyState key_down;
			PlatformKeyState key_left;
			PlatformKeyState key_right;

			PlatformKeyState key_1;
			PlatformKeyState key_2;
			PlatformKeyState key_3;
			PlatformKeyState key_4;
			PlatformKeyState key_5;
			PlatformKeyState key_6;
			PlatformKeyState key_7;
			PlatformKeyState key_8;
			PlatformKeyState key_9;

			PlatformKeyState key_q;
			PlatformKeyState key_w;
			PlatformKeyState key_e;
			PlatformKeyState key_r;

			PlatformKeyState key_a;
			PlatformKeyState key_s;
			PlatformKeyState key_d;
			PlatformKeyState key_f;

			PlatformKeyState key_z;
			PlatformKeyState key_x;
			PlatformKeyState key_c;
			PlatformKeyState key_v;
		};
	};
};

#endif
