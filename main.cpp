#include <iostream>
#include <Windows.h>
#include <vector>
#include <atomic>

struct Bound
{
	int x, y;
	int w, h;
};

enum State
{
	IDLE,
	RECORD,
	PLAY,
	COUNT
};

struct Event
{
	INPUT input;
	DWORD time;
};

char HELP_STRING[] = "--------------------------------------------------------------\r\nHelp\r\n--------------------------------------------------------------\r\nF5 for start record\r\nF6 for stop record\r\nF7 for play macro\r\nF8 for stop macro\r\n\r\nStatus:        \0";
const unsigned int HELP_STATUS_PTR = 223;

const char *STATUS_STRINGS[State::COUNT] =
{
	"NOTHING\0",
	"RECORDING\0",
	"PLAYING BACK\0"
};

std::atomic<DWORD> clockTimeEvent = 0;
std::atomic<State> state;
std::vector<Event> events;
Bound virtualScreenBound;
HWND edit;

// KEY EVENTS
/*
#define WM_KEYFIRST                     0x0100
#define WM_KEYDOWN                      0x0100
#define WM_KEYUP                        0x0101
#define WM_CHAR                         0x0102
#define WM_DEADCHAR                     0x0103
#define WM_SYSKEYDOWN                   0x0104
#define WM_SYSKEYUP                     0x0105
#define WM_SYSCHAR                      0x0106
#define WM_SYSDEADCHAR                  0x0107
*/

void KeyDownEvent(LPKBDLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->ki.dwFlags = 0;
	outInput->ki.wVk = msg->vkCode;
	outInput->ki.wScan = msg->scanCode;
}

void KeyUpEvent(LPKBDLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->ki.dwFlags = KEYEVENTF_KEYUP;
	outInput->ki.wVk = msg->vkCode;
	outInput->ki.wScan = msg->scanCode;
}

void SystemKeyDownEvent(LPKBDLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->ki.dwFlags = 0;
	outInput->ki.wVk = msg->vkCode;
	outInput->ki.wScan = msg->scanCode;
}

void SystemKeyUpEvent(LPKBDLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->ki.dwFlags = KEYEVENTF_KEYUP;
	outInput->ki.wVk = msg->vkCode;
	outInput->ki.wScan = msg->scanCode;
}

#define KEY_EVENT_MASK 7
void (*keyEventConvertors[KEY_EVENT_MASK])(LPKBDLLHOOKSTRUCT msg, INPUT *outInput) =
{
	KeyDownEvent, KeyUpEvent, 0, 0,
	SystemKeyDownEvent, SystemKeyUpEvent
};

// MOUSE EVENTS
/*
#define WM_MOUSEFIRST                   0x0200
#define WM_MOUSEMOVE                    0x0200
#define WM_LBUTTONDOWN                  0x0201
#define WM_LBUTTONUP                    0x0202
#define WM_LBUTTONDBLCLK                0x0203
#define WM_RBUTTONDOWN                  0x0204
#define WM_RBUTTONUP                    0x0205
#define WM_RBUTTONDBLCLK                0x0206
#define WM_MBUTTONDOWN                  0x0207
#define WM_MBUTTONUP                    0x0208
#define WM_MBUTTONDBLCLK                0x0209
*/

void LeftButtonDownEvent(LPMSLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
}

void LeftButtonUpEvent(LPMSLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->mi.dwFlags = MOUSEEVENTF_LEFTUP;
}

void RightButtonDownEvent(LPMSLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
}

void RightButtonUpEvent(LPMSLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->mi.dwFlags = MOUSEEVENTF_RIGHTUP;
}

void MiddleButtonDownEvent(LPMSLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
}

void MiddleButtonUpEvent(LPMSLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
}

void MouseMoveEvent(LPMSLLHOOKSTRUCT msg, INPUT *outInput)
{
	outInput->mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
	outInput->mi.dx = 65535 / (float)virtualScreenBound.w * (msg->pt.x - virtualScreenBound.x);
	outInput->mi.dy = 65535 / (float)virtualScreenBound.h * (msg->pt.y - virtualScreenBound.y);
}

#define MOUSE_EVENT_MASK 15
void (*mouseEventConvertors[MOUSE_EVENT_MASK])(LPMSLLHOOKSTRUCT msg, INPUT *outInput) =
{
	MouseMoveEvent,
	LeftButtonDownEvent, LeftButtonUpEvent, 0,
	RightButtonDownEvent, RightButtonUpEvent, 0,
	MiddleButtonDownEvent, MiddleButtonUpEvent, 0,
	0, 0, 0, 0, 0
};

void PushEvent(INPUT input, DWORD time)
{
	events.push_back({ input, time - clockTimeEvent });
	clockTimeEvent = time;
}

void SetState(State newState)
{
	char *ptr = HELP_STRING + HELP_STATUS_PTR;
	const char *status = STATUS_STRINGS[newState];
	memset(ptr, 0, sizeof(HELP_STRING) - HELP_STATUS_PTR);
	
	while (*status)
	{
		*ptr = *status;
		++ptr;
		++status;
	}

	SendMessage(edit, WM_SETTEXT, 0, (LPARAM)HELP_STRING);
	state = newState;
}

DWORD WINAPI ThreadProcess(LPVOID lpParameter)
{
	int size = events.size();
	int i = 0;

	while (state == PLAY)
	{
		Event &e = events[i];
		Sleep(e.time);
		SendInput(1, &e.input, sizeof(INPUT));
		i = (i + 1) % size;
	}

	return 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION) return CallNextHookEx(0, nCode, wParam, lParam);

	LPKBDLLHOOKSTRUCT msg = (LPKBDLLHOOKSTRUCT)lParam;

	if (state == IDLE)
	{
		if (wParam != WM_KEYDOWN) return 0;

		if (msg->vkCode == VK_F5)
		{
			virtualScreenBound.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
			virtualScreenBound.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
			virtualScreenBound.w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			virtualScreenBound.h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

			clockTimeEvent = msg->time;
			events.clear();
			SetState(State::RECORD);
		}
		else if (msg->vkCode == VK_F7 && clockTimeEvent)
		{
			SetState(State::PLAY);
			CreateThread(0, 0, ThreadProcess, 0, 0, 0);
		}
		return 0;
	}

	if (state == PLAY)
	{
		if (wParam == WM_KEYDOWN && msg->vkCode == VK_F8) SetState(State::IDLE);
		return 0;
	}

	if (wParam == WM_KEYDOWN && msg->vkCode == VK_F6)
	{
		SetState(State::IDLE);
		return 0;
	}

	void (*eventConvertor)(LPKBDLLHOOKSTRUCT msg, INPUT *outInput) = keyEventConvertors[wParam & KEY_EVENT_MASK];
	if (eventConvertor == 0) return 0;
	INPUT input = { INPUT_KEYBOARD };
	eventConvertor(msg, &input);
	PushEvent(input, msg->time);
	return 0;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION) return CallNextHookEx(0, nCode, wParam, lParam);
	if (state != RECORD) return 0;

	void(*eventConvertor)(LPMSLLHOOKSTRUCT msg, INPUT *outInput) = mouseEventConvertors[wParam & MOUSE_EVENT_MASK];
	if (eventConvertor == 0) return 0;
	LPMSLLHOOKSTRUCT msg = (LPMSLLHOOKSTRUCT)lParam;
	INPUT input = { INPUT_MOUSE };
	eventConvertor(msg, &input);
	PushEvent(input, msg->time);
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CLOSE)
	{
		if (state == IDLE) DestroyWindow(hwnd);
		return 0;
	}

	if (uMsg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	/*AllocConsole();
	FILE *fOut;
	freopen_s(&fOut, "CONOUT$", "w", stdout);*/

	WNDCLASS wc;
	memset(&wc, 0, sizeof(WNDCLASS));
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = "kbam recorder";
	RegisterClass(&wc);

	HWND window = CreateWindow("kbam recorder", "KBAM event record/play", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, 0, 0, 0, 0);

	edit = CreateWindow("edit", 0, WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY, 0, 0, 784, 561, window, 0, (HINSTANCE)GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT | GMEM_SHARE, sizeof(HELP_STRING)), 0);
	SetState(State::IDLE);

	HHOOK keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, 0, 0);
	HHOOK mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, 0, 0);
	MSG msg;

	ShowWindow(window, SW_SHOW);
	UpdateWindow(window);

	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	UnhookWindowsHookEx(mouseHook);
	UnhookWindowsHookEx(keyHook);

	/*fclose(fOut);
	FreeConsole();*/

	return 0;
}
