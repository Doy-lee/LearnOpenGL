#define UNICODE
#define _UNICODE

#include "LOGL.h"
#include "LOGLPlatform.h"
#include "OpenGL.h"

#define DQN_IMPLEMENTATION
#define DQN_PLATFORM_HEADER
#define DQN_WIN32_IMPLEMENTATION
#include "dqn.h"

#include <Windows.h>
#include <Windowsx.h> // For GET_X_PARAM()/GET_Y_PARAM() macro
#include <Psapi.h>    // For GetProcessMemoryInfo()

wglChoosePixelFormatARBProc    *wglChoosePixelFormatARB;
wglCreateContextAttribsARBProc *wglCreateContextAttribsARB;

// GL 1.3
glActiveTextureProc *glActiveTexture;

// GL 1.5
glGenBuffersProc *glGenBuffers;
glBindBufferProc *glBindBuffer;
glBufferDataProc *glBufferData;

// GL 2.0
glCreateShaderProc             *glCreateShader;
glShaderSourceProc             *glShaderSource;
glCompileShaderProc            *glCompileShader;
glGetShaderivProc              *glGetShaderiv;
glGetShaderInfoLogProc         *glGetShaderInfoLog;
glCreateProgramProc            *glCreateProgram;
glAttachShaderProc             *glAttachShader;
glLinkProgramProc              *glLinkProgram;
glUseProgramProc               *glUseProgram;
glDeleteShaderProc             *glDeleteShader;
glGetProgramInfoLogProc        *glGetProgramInfoLog;
glGetProgramivProc             *glGetProgramiv;

glGetUniformLocationProc       *glGetUniformLocation;
glUniform4fProc                *glUniform4f;
glUniform1iProc                *glUniform1i;
glUniformMatrix4fvProc         *glUniformMatrix4fv;

glEnableVertexAttribArrayProc  *glEnableVertexAttribArray;
glDisableVertexAttribArrayProc *glDisableVertexAttribArray;
glVertexAttribPointerProc      *glVertexAttribPointer;

// GL 3.0
glGenVertexArraysProc *glGenVertexArrays;
glBindVertexArrayProc *glBindVertexArray;
glGenerateMipmapProc  *glGenerateMipmap;

FILE_SCOPE bool globalRunning = true;

FILE_SCOPE LRESULT CALLBACK Win32MainProcCallback(HWND window, UINT msg,
                                                  WPARAM wParam, LPARAM lParam)
{
	const LRESULT MESSAGE_HANDLED = 0;
	LRESULT result                = MESSAGE_HANDLED;
	switch (msg)
	{
		case WM_CLOSE:
		{
			globalRunning = false;
		}
		break;

		case WM_SIZE:
		{
			u32 height = ((lParam >> 16) & 0xFFFF);
			u32 width  = (lParam & 0xFFFF);
			glViewport(0, 0, width, height);
		}
		break;

		default:
		{
			result = DefWindowProcW(window, msg, wParam, lParam);
		}
		break;
	}

	return result;
}

FILE_SCOPE inline void Win32UpdateKey(PlatformKeyState *const key, const bool isDown)
{
	if (key->endedDown != isDown)
	{
		key->endedDown = isDown;
		key->halfTransitionCount++;
	}
}

FILE_SCOPE void Win32ProcessInputSeparately(HWND window, PlatformInput *const input)
{
	MSG msg;
	DQN_ASSERT(input);
	while (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
	{
		switch (msg.message)
		{
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			{
				if (!input) return;
				PlatformMouse *mouse = &input->mouse;

				bool isDown = (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN);
				if (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP)
					Win32UpdateKey(&mouse->leftBtn, isDown);
				else if (msg.message == WM_RBUTTONDOWN || msg.message == WM_RBUTTONUP)
					Win32UpdateKey(&mouse->rightBtn, isDown);
				else
					DQN_ASSERT(DQN_INVALID_CODE_PATH);
			}
			break;

			case WM_MOUSEMOVE:
			{
				if (!input) return;
				PlatformMouse *mouse = &input->mouse;

				LONG height;
				DqnWin32_GetClientDim(window, NULL, &height);
				mouse->x = GET_X_LPARAM(msg.lParam);
				mouse->y = height - GET_Y_LPARAM(msg.lParam);
			}
			break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				if (!input) return;

				bool isDown = (msg.message == WM_KEYDOWN);
				switch (msg.wParam)
				{
					case VK_UP:    Win32UpdateKey(&input->key_up, isDown);    break;
					case VK_DOWN:  Win32UpdateKey(&input->key_down, isDown);  break;
					case VK_LEFT:  Win32UpdateKey(&input->key_left, isDown);  break;
					case VK_RIGHT: Win32UpdateKey(&input->key_right, isDown); break;

					case '1': Win32UpdateKey(&input->key_1, isDown); break;
					case '2': Win32UpdateKey(&input->key_2, isDown); break;
					case '3': Win32UpdateKey(&input->key_3, isDown); break;
					case '4': Win32UpdateKey(&input->key_4, isDown); break;

					case 'Q': Win32UpdateKey(&input->key_q, isDown); break;
					case 'W': Win32UpdateKey(&input->key_w, isDown); break;
					case 'E': Win32UpdateKey(&input->key_e, isDown); break;
					case 'R': Win32UpdateKey(&input->key_r, isDown); break;

					case 'A': Win32UpdateKey(&input->key_a, isDown); break;
					case 'S': Win32UpdateKey(&input->key_s, isDown); break;
					case 'D': Win32UpdateKey(&input->key_d, isDown); break;
					case 'F': Win32UpdateKey(&input->key_f, isDown); break;

					case 'Z': Win32UpdateKey(&input->key_z, isDown); break;
					case 'X': Win32UpdateKey(&input->key_x, isDown); break;
					case 'C': Win32UpdateKey(&input->key_c, isDown); break;
					case 'V': Win32UpdateKey(&input->key_v, isDown); break;

					case VK_ESCAPE:
					{
						if (isDown) globalRunning = false;
					}
					break;

					default: break;
				}
			}
			break;

			default:
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			};
		}
	}
}

#define WIN32_GL_LOAD_FUNCTION(glFunction)                                                         \
	do                                                                                             \
	{                                                                                              \
		glFunction = (glFunction##Proc *)(wglGetProcAddress(#glFunction));                               \
		DQN_ASSERT(glFunction);                                                                    \
	} while (0)

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	////////////////////////////////////////////////////////////////////////////
	// App Init
	////////////////////////////////////////////////////////////////////////////
	// Main Loop Config
	const f32 TARGET_FRAMES_PER_S = 60.0f;
	f32 targetSecondsPerFrame     = 1 / TARGET_FRAMES_PER_S;

	// Window Config
	const char WINDOW_TITLE_A[] = u8"LearnOpenGL";
	const char WINDOW_CLASS_A[] = u8"LearnOpenGLClass";

	// UTF-8 Version
	wchar_t windowTitleW[DQN_ARRAY_COUNT(WINDOW_TITLE_A)] = {};
	wchar_t windowClassW[DQN_ARRAY_COUNT(WINDOW_CLASS_A)] = {};
	DQN_ASSERT(DqnWin32_UTF8ToWChar(WINDOW_TITLE_A, windowTitleW, DQN_ARRAY_COUNT(windowTitleW)));
	DQN_ASSERT(DqnWin32_UTF8ToWChar(WINDOW_CLASS_A, windowClassW, DQN_ARRAY_COUNT(windowClassW)));

	const u32 BUFFER_WIDTH  = 800;
	const u32 BUFFER_HEIGHT = 600;

	(void)nShowCmd;
	(void)lpCmdLine;
	(void)hPrevInstance;

	////////////////////////////////////////////////////////////////////////////
	// Setup OpenGL
	////////////////////////////////////////////////////////////////////////////
	HWND mainWindow;
	{
		////////////////////////////////////////////////////////////////////////
		// Create Temp Win32 Window For Temp OGL Rendering Context
		////////////////////////////////////////////////////////////////////////
		WNDCLASSEXW windowClass = {
		    sizeof(WNDCLASSEX),
		    CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		    Win32MainProcCallback,
		    0, // int cbClsExtra
		    0, // int cbWndExtra
		    hInstance,
		    LoadIcon(NULL, IDI_APPLICATION),
		    LoadCursor(NULL, IDC_ARROW),
		    GetSysColorBrush(COLOR_3DFACE),
		    L"", // LPCTSTR lpszMenuName
		    windowClassW,
		    NULL, // HICON hIconSm
		};

		// Register and create tmp window
		HWND tmpWindow;
		{
			if (!RegisterClassExW(&windowClass))
			{
				DqnWin32_DisplayLastError("RegisterClassEx() failed.");
				return -1;
			}

			tmpWindow =
			    CreateWindowExW(0, windowClass.lpszClassName, windowTitleW, 0, CW_USEDEFAULT,
			                    CW_USEDEFAULT, 0, 0, NULL, NULL, hInstance, NULL);

			if (!tmpWindow)
			{
				DqnWin32_DisplayLastError("CreateWindowEx() failed.");
				return -1;
			}
		}

		////////////////////////////////////////////////////////////////////////
		// Setup A Temp OGL Context To Use For Getting Extended Functionality
		////////////////////////////////////////////////////////////////////////
		HDC tmpDeviceContext = GetDC(tmpWindow);
		HGLRC tmpOGLRenderingContext;
		{
			PIXELFORMATDESCRIPTOR tmpDesiredPixelFormat = {};
			tmpDesiredPixelFormat.nSize                 = sizeof(tmpDesiredPixelFormat);
			tmpDesiredPixelFormat.nVersion              = 1;
			tmpDesiredPixelFormat.iPixelType            = PFD_TYPE_RGBA;
			tmpDesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
			tmpDesiredPixelFormat.cColorBits = 32;
			tmpDesiredPixelFormat.cAlphaBits = 8;
			tmpDesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

			// ChoosePixelFormat finds the closest matching pixelFormat available on the machine
			i32 tmpSuggestedPixelFormatIndex =
			    ChoosePixelFormat(tmpDeviceContext, &tmpDesiredPixelFormat);

			// Create the PixelFormat based on the closest matching one
			PIXELFORMATDESCRIPTOR tmpSuggestedPixelFormat = {};
			DescribePixelFormat(tmpDeviceContext, tmpSuggestedPixelFormatIndex,
			                    sizeof(PIXELFORMATDESCRIPTOR), &tmpSuggestedPixelFormat);

			// Set it and assign the OpenGL context to our program
			if (!SetPixelFormat(tmpDeviceContext, tmpSuggestedPixelFormatIndex,
			                    &tmpSuggestedPixelFormat))
			{
				DqnWin32_DisplayLastError("SetPixelFormat() failed");
				return -1;
			}

			tmpOGLRenderingContext = wglCreateContext(tmpDeviceContext);
			if (!wglMakeCurrent(tmpDeviceContext, tmpOGLRenderingContext))
			{
				DqnWin32_DisplayLastError("wglMakeCurrent() failed");
				return -1;
			}
		}

		////////////////////////////////////////////////////////////////////////
		// Load WGL Functions For Creating Modern OGL Windows
		////////////////////////////////////////////////////////////////////////
		WIN32_GL_LOAD_FUNCTION(wglChoosePixelFormatARB);
		WIN32_GL_LOAD_FUNCTION(wglCreateContextAttribsARB);

		////////////////////////////////////////////////////////////////////////
		// Create Window Using Modern OGL Functions
		////////////////////////////////////////////////////////////////////////
		{
			// NOTE: Regarding Window Sizes
			// If you specify a window size, e.g. 800x600, Windows regards the 800x600
			// region to be inclusive of the toolbars and side borders. So in actuality,
			// when you blit to the screen blackness, the area that is being blitted to
			// is slightly smaller than 800x600. Windows provides a function to help
			// calculate the size you'd need by accounting for the window style.
			RECT windowRect   = {};
			windowRect.right  = BUFFER_WIDTH;
			windowRect.bottom = BUFFER_HEIGHT;

			const bool HAS_MENU_BAR  = false;
			const DWORD WINDOW_STYLE = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
			AdjustWindowRect(&windowRect, WINDOW_STYLE, HAS_MENU_BAR);

			mainWindow =
			    CreateWindowExW(0, windowClass.lpszClassName, windowTitleW, WINDOW_STYLE,
			                    CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left,
			                    windowRect.bottom - windowRect.top, NULL, NULL, hInstance, NULL);
			if (!mainWindow)
			{
				DqnWin32_DisplayLastError("CreateWindowEx() failed.");
				return -1;
			}

			const i32 DESIRED_PIXEL_FORMAT[] = {WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			                                    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			                                    WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
			                                    WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
			                                    WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
			                                    WGL_COLOR_BITS_ARB,     32,
			                                    WGL_ALPHA_BITS_ARB,     8,
			                                    WGL_DEPTH_BITS_ARB,     24,
			                                    WGL_STENCIL_BITS_ARB,   8,
			                                    WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
			                                    WGL_SAMPLES_ARB,        4,
			                                    0};

			HDC deviceContext = GetDC(mainWindow);
			i32 suggestedPixelFormatIndex;
			u32 numFormatsSuggested;
			bool wglResult =
			    wglChoosePixelFormatARB(deviceContext, DESIRED_PIXEL_FORMAT, NULL, 1,
			                            &suggestedPixelFormatIndex, &numFormatsSuggested);
			if (!wglResult || numFormatsSuggested == 0)
			{
				DQN_WIN32_ERROR_BOX("wglChoosePixelFormatARB() failed", NULL);
				return -1;
			}

			PIXELFORMATDESCRIPTOR suggestedPixelFormat = {};
			DescribePixelFormat(deviceContext, suggestedPixelFormatIndex,
			                    sizeof(suggestedPixelFormat), &suggestedPixelFormat);
			if (!SetPixelFormat(deviceContext, suggestedPixelFormatIndex, &suggestedPixelFormat))
			{
				DqnWin32_DisplayLastError("SetPixelFormat() Modern OGL Creation failed");
				return -1;
			}

			const i32 OGL_MAJOR_MIN = 3, OGL_MINOR_MIN = 3;
			const i32 CONTEXT_ATTRIBS[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, OGL_MAJOR_MIN,
			                               WGL_CONTEXT_MINOR_VERSION_ARB, OGL_MINOR_MIN,
			                               WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			                               0};

			HGLRC oglRenderingContext =
			    wglCreateContextAttribsARB(deviceContext, 0, CONTEXT_ATTRIBS);
			if (!oglRenderingContext)
			{
				DQN_WIN32_ERROR_BOX("wglCreateContextAttribsARB() failed", NULL);
				return -1;
			}

			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(tmpOGLRenderingContext);
			ReleaseDC(tmpWindow, tmpDeviceContext);
			DestroyWindow(tmpWindow);

			if (!wglMakeCurrent(deviceContext, oglRenderingContext))
			{
				DqnWin32_DisplayLastError("wglMakeCurrent() Modern OGL Creation failed");
				return -1;
			}

			ReleaseDC(mainWindow, deviceContext);
		}

		WIN32_GL_LOAD_FUNCTION(glActiveTexture);

		WIN32_GL_LOAD_FUNCTION(glGenBuffers);
		WIN32_GL_LOAD_FUNCTION(glBindBuffer);
		WIN32_GL_LOAD_FUNCTION(glBufferData);
		WIN32_GL_LOAD_FUNCTION(glCreateShader);
		WIN32_GL_LOAD_FUNCTION(glShaderSource);
		WIN32_GL_LOAD_FUNCTION(glCompileShader);
		WIN32_GL_LOAD_FUNCTION(glGetShaderiv);
		WIN32_GL_LOAD_FUNCTION(glGetShaderInfoLog);
		WIN32_GL_LOAD_FUNCTION(glCreateProgram);
		WIN32_GL_LOAD_FUNCTION(glAttachShader);
		WIN32_GL_LOAD_FUNCTION(glLinkProgram);
		WIN32_GL_LOAD_FUNCTION(glUseProgram);
		WIN32_GL_LOAD_FUNCTION(glDeleteShader);
		WIN32_GL_LOAD_FUNCTION(glGetProgramInfoLog);
		WIN32_GL_LOAD_FUNCTION(glGetProgramiv);

		WIN32_GL_LOAD_FUNCTION(glGetUniformLocation);
		WIN32_GL_LOAD_FUNCTION(glUniform4f);
		WIN32_GL_LOAD_FUNCTION(glUniform1i);
		WIN32_GL_LOAD_FUNCTION(glUniformMatrix4fv);

		WIN32_GL_LOAD_FUNCTION(glEnableVertexAttribArray);
		WIN32_GL_LOAD_FUNCTION(glDisableVertexAttribArray);
		WIN32_GL_LOAD_FUNCTION(glVertexAttribPointer);

		WIN32_GL_LOAD_FUNCTION(glGenVertexArrays);
		WIN32_GL_LOAD_FUNCTION(glBindVertexArray);
		WIN32_GL_LOAD_FUNCTION(glGenerateMipmap);

		glViewport(0, 0, BUFFER_WIDTH, BUFFER_HEIGHT);
	}

	f64 frameTimeInS    = 0.0f;
	PlatformInput input = {};
	input.screenDim     = DqnV2_2i(BUFFER_WIDTH, BUFFER_HEIGHT);

	PlatformMemory memory = {};
	bool memInitResult    = (memory.mainStack.Init(DQN_MEGABYTE(16), true, 4) &&
	                         memory.tempStack.Init(DQN_MEGABYTE(16), true, 4));
	if (!DQN_ASSERT(memInitResult)) return -1;

	while (globalRunning)
	{
		f64 startFrameTimeInS = DqnTimer_NowInS();
		input.deltaForFrame   = (f32)frameTimeInS;
		Win32ProcessInputSeparately(mainWindow, &input);

		////////////////////////////////////////////////////////////////////////
		// Update and Render
		////////////////////////////////////////////////////////////////////////
		LOGL_Update(&input, &memory);
		if (1)
		{
			HDC deviceContext = GetDC(mainWindow);
			SwapBuffers(deviceContext);
			ReleaseDC(mainWindow, deviceContext);
		}

		////////////////////////////////////////////////////////////////////////
		// Frame Limiting
		////////////////////////////////////////////////////////////////////////
		if (1)
		{
			f64 workTimeInS = DqnTimer_NowInS() - startFrameTimeInS;
			if (workTimeInS < targetSecondsPerFrame)
			{
				DWORD remainingTimeInMs =
				    (DWORD)((targetSecondsPerFrame - workTimeInS) * 1000);
				Sleep(remainingTimeInMs);
			}
		}

		frameTimeInS        = DqnTimer_NowInS() - startFrameTimeInS;
		f32 msPerFrame      = 1000.0f * (f32)frameTimeInS;
		f32 framesPerSecond = 1.0f / (f32)frameTimeInS;

		////////////////////////////////////////////////////////////////////////
		// Misc
		////////////////////////////////////////////////////////////////////////
		// Update title bar
		if (1)
		{
			const f32 titleUpdateFrequencyInS  = 0.1f;
			LOCAL_PERSIST f32 titleUpdateTimer = titleUpdateFrequencyInS;
			titleUpdateTimer += (f32)frameTimeInS;
			if (titleUpdateTimer > titleUpdateFrequencyInS)
			{
				titleUpdateTimer = 0;

				// Get Win32 reported mem usage
				PROCESS_MEMORY_COUNTERS memCounter = {};
				GetProcessMemoryInfo(GetCurrentProcess(), &memCounter, sizeof(memCounter));

				// Create UTF-8 buffer string
				const char formatStr[]       = "%s - dev - %5.2f ms/f - %5.2f fps - working set mem %'dkb - page file touched mem %'dkb";
				const u32 windowTitleBufSize = DQN_ARRAY_COUNT(formatStr) + DQN_ARRAY_COUNT(WINDOW_TITLE_A) + 32;
				char windowTitleBufA[windowTitleBufSize] = {};

				// Form UTF-8 buffer string
				Dqn_sprintf(windowTitleBufA, formatStr, WINDOW_TITLE_A, msPerFrame, framesPerSecond,
				            (u32)(memCounter.WorkingSetSize / 1024.0f),
				            (u32)(memCounter.PagefileUsage / 1024.0f));

				// Convert to wchar_t for windows
				wchar_t windowTitleBufW[windowTitleBufSize] = {};
				DQN_ASSERT(DqnWin32_UTF8ToWChar(windowTitleBufA, windowTitleBufW, windowTitleBufSize));
				SetWindowTextW(mainWindow, windowTitleBufW);
			}
		}
	}

	return 0;
}
