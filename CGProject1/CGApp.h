#pragma once

#include <windows.h>
#include <wrl.h>
#include <assert.h>

#include <string>

#include "Timer.h"

class CGApp
{
public:
	CGApp(HINSTANCE hinstance);
	CGApp(const CGApp& rhs) = delete;
	CGApp& operator = (const CGApp& rhs) = delete;
	~CGApp();

	static CGApp*    GetApp();

	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	LRESULT   MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static CGApp* mApp;


private:
	bool InitMainWindow();
	bool Init3DDevice();

	HINSTANCE mhAppInst = nullptr; // application instance handle
	HWND      mhMainWnd = nullptr; // main window handle
	HDC		  mhScreenDC = nullptr;// screen handle DC
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled

	int mClientWidth = 800;
	int mClientHeight = 600;

	Timer mTimer;

	//HDC screen_dc = NULL;

	std::wstring mMainWndCaption = L"CG App";
};

