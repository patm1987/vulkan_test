
#include "stdafx.h"

#include "Win32VulkanTest.h"

#include "VulkanRenderer.h"

static const int kDefaultWidth = 640;
static const int kDefaultHeight = 480;

typedef struct {
	HWND hWindow;
	BOOL running;
	VulkanRenderer* pVulkanRenderer;
} ApplicationData;

LRESULT CALLBACK WindowProc(
	_In_ HWND   hwnd,
	_In_ UINT   uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam)
{
	ApplicationData* pAppData = (ApplicationData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_PAINT:
		// Render
		break;
	default:
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

INT WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PSTR lpCmdLine,
	INT nCmdShow)
{
	LPCWSTR className = L"Win32VulkanTest";

	WNDCLASSEX wndClass = { 0 };
	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_VREDRAW | CS_HREDRAW;
	wndClass.lpfnWndProc = WindowProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = className;
	wndClass.hIconSm = NULL;

	if (!RegisterClassEx(&wndClass)) {
		MessageBox(NULL, L"Failed to Create Window Class", NULL, 0);
		exit(1);
	}

	HWND hWindow = CreateWindowEx(
		WS_EX_LEFT,
		className,
		L"Vulkan Test Application",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		kDefaultWidth,
		kDefaultHeight,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hWindow) {
		MessageBox(NULL, L"Failed to Create Window", NULL, 0);
		exit(1);
	}

	ApplicationData appData = { 0 };
	appData.hWindow = hWindow;
	appData.running = TRUE;
	appData.pVulkanRenderer = VulkanRenderer_Create();

	SetWindowLongPtr(hWindow, GWLP_USERDATA, (LONG_PTR)&appData);

	MSG msg;
	while (appData.running) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				appData.running = FALSE;
			}
		}

		// Game Stuff
		VulkanRenderer_Render(appData.pVulkanRenderer);
	}

	VulkanRenderer_Destroy(appData.pVulkanRenderer);
	appData.pVulkanRenderer = NULL;
	DestroyWindow(appData.hWindow);
	appData.hWindow = NULL;

	return 0;
}
