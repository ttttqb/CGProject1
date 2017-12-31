#include "CGApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	CGApp thisApp(hInstance);
	if (!thisApp.Initialize("2.obj"))
		return 0;
	return thisApp.Run();
}