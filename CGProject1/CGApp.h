#pragma once

#include <windows.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <assert.h>

#include <string>
#include <array>
#include <vector>
#include <map>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

#include "Timer.h"
#include "MathHelper.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};


class Edge
{
public:
	float x1, x2, y1, y2, ymax, deltax;
	float x;
	int polyID;
	bool operator <(const Edge& b) const {
		return x < b.x;
	}
};

class Poly
{
public:
	int polyID;
	float A, B, C, D;
	float ymin, ymax, z;
	XMFLOAT4 Color;
	bool in;
};

struct Obj
{
	std::vector<XMFLOAT4> vectors;
	std::vector<std::uint16_t> indices;
	int polyNum = 0;
	std::vector<Edge> EdgeList;
	std::vector<Poly> PolyList;

	float ymax, ymin;
};

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

	bool Initialize(const char*);
	int  Run();
	void Update(const Timer& gt);
	void Draw(const Timer& gt);

	Obj mObj;

private:
	bool InitMainWindow();
	bool Init3DDevice();
	bool InitModelInput(const char*);
	void CalculateFrameStats();
	void Rasterizer();
	float CalCuZ(int, float, int);
	std::vector<Edge>::iterator Edgepop(std::vector<Edge>& AET, std::vector<int>& APT, std::vector<Edge>::iterator& enow, int y, bool in);
	void DrawLine(float, float, int, XMFLOAT4);
	void DrawPixel(int, int, XMFLOAT4);

	HINSTANCE mhAppInst = nullptr; // application instance handle
	HWND      mhMainWnd = nullptr; // main window handle
	HDC		  mhScreenDC = nullptr;// screen handle DC
	HBITMAP   mScreenHB1 = nullptr;// BITMAP1
	HBITMAP   mScreenHB2 = nullptr;// BITMAP2
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled

	unsigned char* screen_fb = nullptr;// Frame Buffer
	unsigned int** framebuffer = nullptr;
	float **       zbuffer = nullptr;


	int mClientWidth = 800;
	int mClientHeight = 600;
	long mClientPitch = 0;

	Timer mTimer;

	std::wstring mMainWndCaption = L"CG App";

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

	float mTheta = 1.5f*XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	POINT mLastMousePos;

	std::vector<XMFLOAT4> mVertices;
	std::vector<int> mIndices;
	std::vector<XMFLOAT3> mNormals;
	bool cull[1000000];
};

