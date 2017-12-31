#include "CGApp.h"
#include <WindowsX.h>
#include <tchar.h>  

#include <iostream>
#include <string>
#include <algorithm>

#include <cmath>
#include <cassert>

using Microsoft::WRL::ComPtr;
using namespace std;

const float EPSINON = 0.00001f;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return CGApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}



std::array<Vertex, 8> vertices =
{
	Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
	Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Pink) }),
	Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
	Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
	Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
	Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
	Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
	Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
};

//std::array<std::uint16_t, 36> indices =
//{
//	// front face
//	0, 1, 2,
//	0, 2, 3,
//
//	// back face
//	4, 6, 5,
//	4, 7, 6,
//
//	// left face
//	4, 5, 1,
//	4, 1, 0,
//
//	// right face
//	3, 2, 6,
//	3, 6, 7,
//
//	// top face
//	1, 5, 6,
//	1, 6, 2,
//
//	// bottom face
//	4, 0, 3,
//	4, 3, 7
//};

CGApp* CGApp::mApp = nullptr;

CGApp* CGApp::GetApp()
{
	return mApp;
}

CGApp::CGApp(HINSTANCE hinstance)
	:mhAppInst(hinstance)
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

bool CGApp::Initialize(const char* fpath)
{
	if (!InitMainWindow())
		return false;
	if (!Init3DDevice())
		return false;
	return InitModelInput(fpath);
}

int CGApp::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats();
				Update(mTimer);
				Rasterizer();
				Draw(mTimer);
				//Sleep(1);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

int check_ccv(const XMFLOAT4* v)
{
	float w = v->w;
	int check = 0;
	if (v->z < 0.0f) check |= 1;
	if (v->z >  w) check |= 2;
	if (v->x < -w) check |= 4;
	if (v->x >  w) check |= 8;
	if (v->y < -w) check |= 16;
	if (v->y >  w) check |= 32;
	return check;
}

XMFLOAT4 homogenize(int w, int h, const XMFLOAT4* v)
{
	XMFLOAT4 y;
	float rhw = 1.0f / v->w;
	y.x = (v->x * rhw + 1.0f) * w * 0.5f;
	y.y = (1.0f - v->y * rhw) * h * 0.5f;
	y.z = v->z * rhw;
	y.w = 1.0;
	return y;
}

void CGApp::Update(const Timer & gt)
{
	// Convert Spherical to Cartesian coordinates.
	//mTheta = 4.376411f;
	//mPhi = 0.5759f;
	//TCHAR chInput[512];
	//_stprintf_s(chInput, _T("%f %f %f\n"), mRadius, mPhi, mTheta);
	//OutputDebugString(chInput);


	float x = mRadius*sinf(mPhi)*cosf(mTheta);
	float z = mRadius*sinf(mPhi)*sinf(mTheta);
	float y = mRadius*cosf(mPhi);

	// 创建mvp矩阵
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, static_cast<float>(mClientWidth) / mClientHeight, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world*view*proj;

	for (int y = 0; y < mClientHeight; y++) {
		for (int x = 0; x < mClientWidth; x++)
			framebuffer[y][x] = 0x0;
	}

	// 顶点着色器
	mObj.vectors.clear();
	mObj.EdgeList.clear();
	mObj.PolyList.clear();
	mObj.polyNum = 0;

	mObj.ymax = -1;
	mObj.ymin = 4096;
	
	// 背面剔除
	memset(cull, 0, sizeof(bool)*(mIndices.size() / 3));

	for (int i = 0; i < mIndices.size(); i+=3) {
		float nx = mNormals[mIndices[i]].x + mNormals[mIndices[i + 1]].x + mNormals[mIndices[i + 2]].x;
		float ny = mNormals[mIndices[i]].y + mNormals[mIndices[i + 1]].y + mNormals[mIndices[i + 2]].y;
		float nz = mNormals[mIndices[i]].z + mNormals[mIndices[i + 1]].z + mNormals[mIndices[i + 2]].z;
		
		if (nx*x+ny*y+nz*z<0)
			cull[i / 3] = false;
	}


	for (int i = 0; i < mVertices.size(); i++) {
		//XMVECTOR v = XMLoadFloat4(&XMFLOAT4(vertices[i].Pos.x, vertices[i].Pos.y, vertices[i].Pos.z, 1));
		XMVECTOR v = XMLoadFloat4(&XMFLOAT4(mVertices[i]));
		v = XMVector4Transform(v, worldViewProj);
		XMFLOAT4 p;
		XMStoreFloat4(&p, v);
		mObj.vectors.push_back(p);
	}

	// 创建边表和多边形表
	for (int i = 0; i < mIndices.size(); i+=3) {
		XMFLOAT4 p[3];
		float p_ymax = -1;
		float p_ymin = 4096;
		float z = 1;

		if (cull[i / 3]) continue;

		for (int j = 0; j < 3; j++)
			p[j] = mObj.vectors[mIndices[i+j]];
			//p[j] = mVertices[mIndices[i + j]];
		if (check_ccv(&p[0]) != 0) continue;
		if (check_ccv(&p[1]) != 0) continue;
		if (check_ccv(&p[2]) != 0) continue;
		
		//归一化
		for (int j = 0; j < 3; j++)
		{	
			p[j] = homogenize(mClientWidth, mClientHeight, &p[j]);
			if (p[j].y > mObj.ymax) mObj.ymax = p[j].y;
			if (p[j].y < mObj.ymin) mObj.ymin = p[j].y;
			//TCHAR chInput[512];
			//_stprintf_s(chInput, _T("%f "), p[j].z);
			//OutputDebugString(chInput);
		}

		for (int j = 0; j < 3; j++) {
			Edge e;
			int m = j;
			int n = (j + 1) % 3;
			if (p[m].x > p[n].x) {
				e.y2 = p[m].y;
				e.x2 = p[m].x;
				e.y1 = p[n].y;
				e.x1 = p[n].x;
			}
			else {
				e.y1 = p[m].y;
				e.x1 = p[m].x;
				e.y2 = p[n].y;
				e.x2 = p[n].x;
			}
			if (p[m].y != p[n].y)
				e.deltax = (p[n].x - p[m].x) / (p[m].y - p[n].y);
			else
				e.deltax = 0;
			e.ymax = p[m].y;
			e.x = p[m].x;
			if (p[m].y < p[n].y)
				e.ymax = p[n].y, e.x = p[n].x;
			e.polyID = mObj.polyNum;
			z = p[m].z;

			p_ymax = (p_ymax < e.ymax) ? e.ymax : p_ymax;
			float t = e.y1 + e.y2 - e.ymax;
			p_ymin = (p_ymin > t) ? t : p_ymin;

			mObj.EdgeList.push_back(e);
		}

		Poly poly;
		poly.polyID = mObj.polyNum;

		poly.A = ((p[1].y - p[0].y)*(p[2].z - p[0].z) - (p[1].z - p[0].z)*(p[2].y - p[0].y));
		poly.B = ((p[1].z - p[0].z)*(p[2].x - p[0].x) - (p[1].x - p[0].x)*(p[2].z - p[0].z));
		poly.C = ((p[1].x - p[0].x)*(p[2].y - p[0].y) - (p[1].y - p[0].y)*(p[2].x - p[0].x));
		poly.D = -(poly.A*p[0].x + poly.B*p[0].y + poly.C*p[0].z);

		poly.Color = vertices[i%8].Color;
		poly.in = false;

		poly.ymax = p_ymax;
		poly.ymin = p_ymin;

		poly.z = z;
		
		mObj.PolyList.push_back(poly);
		mObj.polyNum++;
	}
}

bool comp(const Edge& e1, const Edge& e2) {
	return e1.ymax > e2.ymax;
}

float CGApp::CalCuZ(int polyID, float x, int y)
{
	Poly* p = &(mObj.PolyList[polyID]);
	float z;
	if (abs(p->C) < EPSINON)
		return p->z;
	z = -p->D - p->A*x - p->B*y;
	return z / p->C;
}

// 弹出x最小的一条边并标记这一点的所有边
std::vector<Edge>::iterator CGApp::Edgepop(std::vector<Edge>& AET, std::vector<int>& APT, std::vector<Edge>::iterator& enow, int y, bool in)
{
	auto ret = enow;
	auto e = enow;
	//float zmin = 1.0f;
	//float xmin = enow->x1;
	//float znow;

	if (enow == AET.end()) return enow;

	//if (in) APT[e->polyID] = !APT[e->polyID];
	//e++;
	while (e != AET.end()) {
		// 不重合
		if (abs(enow->x - e->x) > EPSINON) break;

		// 上下端点重合取两条边
		if (abs(y - mObj.PolyList[e->polyID].ymax) < EPSINON || abs(y - mObj.PolyList[e->polyID].ymin) < EPSINON)
		{
			//assert(APT.find(e->polyID) != APT.end());
			//if (in) APT[e->polyID] = !APT[e->polyID];
			if (in) mObj.PolyList[e->polyID].in = !mObj.PolyList[e->polyID].in;
		}
		else {
		// 普通端点
			if (abs(e->ymax - e->y1) < EPSINON) {
				if (floor(e->y2)==y ) {
					e++;
					continue;
				}
				if (ceil(e->y1) == y) {
					e++;
					continue;
				}
			}
			else if (abs(e->ymax - e->y2) < EPSINON) {
				if (floor(e->y1) == y) {
					e++;
					continue;
				}
				if (ceil(e->y2) == y) {
					e++;
					continue;
				}
			}
			//if (in) APT[e->polyID] = !APT[e->polyID];
			if (in) mObj.PolyList[e->polyID].in = !mObj.PolyList[e->polyID].in;
		}
		ret = e;
		e++;
	}
	if (in) enow = e;
	return ret;
}

void CGApp::DrawPixel(int x, int y, XMFLOAT4 color)
{
	unsigned int cc;
	cc = ((unsigned int)floor(color.x * 255 + 0.5)) << 16 
		| ((unsigned int)floor(color.y * 255 + 0.5)) << 8 
		| (unsigned int)floor(color.z * 255 + 0.5);
	framebuffer[y][x] = cc;
}

void CGApp::DrawLine(float x1, float x2, int y, XMFLOAT4 color)
{
	int X1, X2;
	if (y >= mClientHeight) return;
	X1 = (int)floor(x1 + 0.5);
	X2 = (int)floor(x2 + 0.5);
	if (X2 < 0) return;
	if (X1 < 0) X1 = 0;
	if (X2 >= mClientWidth) X2 = mClientWidth - 1;
	if (X1 > X2) return;
	for (int i = X1; i <= X2; i++)
		DrawPixel(i, y, color);
}

std::vector<Edge> AET;
std::vector<int> APT;
//std::map<int, bool> APT;

void CGApp::Rasterizer()
{
	int YMIN, YMAX;
	auto enow = mObj.EdgeList.begin();
	std::vector<Edge>::iterator aenow;
	bool needSort;
	XMFLOAT4 color;
	float zmin, znow, xmid;

	//// 清空zbuffer
	//for (int y = 0; y < mClientHeight; y++) {
	//	float *dst = zbuffer[y];
	//	float cc;
	//	cc = 0.0f;
	//	for (int x = mClientWidth; x > 0; dst++, x--) dst[0] = cc;
	//}

	AET.clear();


	// 边表按ymax初排序
	std::sort(mObj.EdgeList.begin(), mObj.EdgeList.end(), comp);
	YMAX = ceil(mObj.ymax);
	YMIN = floor(mObj.ymin);
	for (int y = YMAX; y >= YMIN; y--)
	{
		needSort = false;
		APT.clear();
		
		// 加入新边
		while (enow != mObj.EdgeList.end() && enow->ymax > y) {
			if (abs(enow->y1 - enow->y2) < EPSINON) {
				enow++;
				continue;
			}
			AET.push_back(*enow);
			enow++;
			needSort = true;
		}
		if (AET.empty()) continue;

		// 判断是否需要排序
		if (!needSort)
		for (auto e = AET.begin(); e != AET.end(); e++) {
			if (e + 1 == AET.end()) break;
			if (e->x > (e + 1)->x) needSort = true;
		}

		// 建立边表
		for (auto e = AET.begin(); e != AET.end(); e++)
			//APT[e->polyID] = false;
			APT.push_back(e->polyID);

		// 去重
		APT.erase(unique(APT.begin(), APT.end()), APT.end());

		// 对当前扫描线上的边按x排序
		if (needSort)	sort(AET.begin(), AET.end());

		// 扫描线
		aenow = AET.begin();
		auto e1 = Edgepop(AET, APT, aenow, y, true);
		
		while (e1 != AET.end())
		{
			auto e2 = Edgepop(AET, APT, aenow, y, false);
			if (e2 == AET.end())break;
			
			xmid = (e1->x + e2->x) / 2;
			zmin = 100.0f;
			color = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
			for (auto pl = APT.begin(); pl != APT.end(); pl++) {
				if (!mObj.PolyList[*pl].in) continue;
				znow = CalCuZ(*pl, xmid, y);
				if (znow < zmin) {
					zmin = znow;
					color = mObj.PolyList[*pl].Color;
				}
			}

			DrawLine(e1->x, e2->x, y, color);

			e1 = Edgepop(AET, APT, aenow, y, true);
		}

		// 更新活化边表
		for (auto e = AET.begin(); e != AET.end();) {
			float ymin = e->y1 + e->y2 - e->ymax;
			if (y < ymin) {
				e = AET.erase(e);
			}
			else {
				e->x += e->deltax;
				e++;
			}
		}
	}
}

void CGApp::Draw(const Timer & gt)
{
	HDC hDC = GetDC(mhMainWnd);
	SelectObject(mhScreenDC, mScreenHB1);
	BitBlt(hDC, 0, 0, mClientWidth, mClientHeight, mhScreenDC, 0, 0, SRCCOPY);
	ReleaseDC(mhMainWnd, hDC);
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
		0, 0, 0, 0, 0 } };

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
	//ReleaseDC(mhMainWnd, hDC);

	//mScreenHB1 = CreateDIBSection(mhScreenDC, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
	mScreenHB1 = CreateCompatibleBitmap(hDC, mClientWidth, mClientHeight);
	if (mScreenHB1 == nullptr) {
		MessageBox(0, L"CreateDIB Failed.", 0, 0);
		return false;
	}
	SelectObject(mhScreenDC, mScreenHB1);
	mScreenHB1 = CreateDIBSection(mhScreenDC, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
	//mScreenHB2 = (HBITMAP)SelectObject(mhScreenDC, mScreenHB1);
	
	screen_fb = (unsigned char*)ptr;

	mClientPitch = width * 4;

	ShowWindow(mhMainWnd, SW_SHOWNORMAL);
	UpdateWindow(mhMainWnd);

	memset(screen_fb, 0, mClientWidth * mClientHeight * 4);

	return true;
}

bool CGApp::Init3DDevice()
{
	int height = mClientHeight;
	int width = mClientWidth;
	int need = sizeof(void*) * (height * 2) + width * height * 8;
	char *ptr = (char*)malloc(need + 64);//zbuffer和framebuffer申请内存
	char *framebuf, *zbuf;

	framebuffer = (unsigned int**)ptr;
	zbuffer = (float**)(ptr + sizeof(void*) * height);
	ptr += sizeof(void*) * height * 2;
	framebuf = (char*)ptr;
	zbuf = (char*)ptr + width*height * 4;
	if (screen_fb != NULL) framebuf = (char*)screen_fb;

	for (int j = 0; j < height; j++) {
		framebuffer[j] = (unsigned int*)(framebuf + width * 4 * j);
		zbuffer[j] = (float*)(zbuf + width * 4 * j);
	}

	return true;
}

bool CGApp::InitModelInput(const char* fpath)
{

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(std::string(fpath),
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType |
		aiProcess_MakeLeftHanded |
		aiProcess_GenSmoothNormals
	);

	if (!scene)
	{
		MessageBox(0, L"Import Model Failed", 0, 0);
		return false;
	}

	const aiMesh* mesh;
	int vertexNum = 0;

	mVertices.clear();
	mIndices.clear();
	for (int l = 0; l < scene->mNumMeshes; l++)
	{
		mesh = scene->mMeshes[l];
		for (int i = 0; i < mesh->mNumVertices; i++) {
			mVertices.push_back(XMFLOAT4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f));
			mNormals.push_back(XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z));
		}

		for (int i = 0; i < mesh->mNumFaces; i++) {
			mIndices.push_back(mesh->mFaces[i].mIndices[0] + vertexNum);
			mIndices.push_back(mesh->mFaces[i].mIndices[1] + vertexNum);
			mIndices.push_back(mesh->mFaces[i].mIndices[2] + vertexNum);
		}
		vertexNum = mVertices.size();
	}
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
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CGApp::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}



CGApp::~CGApp()
{
	if (mhScreenDC) {
		if (mScreenHB2) {
			SelectObject(mhScreenDC, mScreenHB2);
			mScreenHB2 = NULL;
		}
		DeleteDC(mhScreenDC);
		mhScreenDC = NULL;
	}
	if (mScreenHB1) {
		DeleteObject(mScreenHB1);
		mScreenHB1 = NULL;
	}
	if (mhMainWnd) {
		CloseWindow(mhMainWnd);
		mhMainWnd = NULL;
	}
}

void CGApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void CGApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void CGApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f*static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f*static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		//mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}