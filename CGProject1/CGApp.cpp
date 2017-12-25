#include "CGApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return CGApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

CGApp* CGApp::mApp = nullptr;

CGApp* CGApp::GetApp()
{
	return mApp;
}

CGApp::CGApp(HINSTANCE hinstance)
{
	assert(mApp == nullptr);
	mApp = this;
}

HINSTANCE CGApp::AppInst() const
{
	return mhAppInst;
}

HWND CGApp::MainWnd() const
{
	return mhMainWnd;
}

bool CGApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), mClientWidth, -mClientHeight, 1, 32, BI_RGB,
		mClientWidth * mClientHeight * 4, 0, 0, 0, 0 } };

	LPVOID ptr;
	HDC hDC;

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}
	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	hDC = GetDC(mhMainWnd);
	mhScreenDC = CreateCompatibleDC(hDC);
	ReleaseDC(mhMainWnd, hDC);

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool CGApp::Init3DDevice()
{
	return true;
}

LRESULT CGApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;
	}
}



CGApp::~CGApp()
{
}
