#include"SimpleDebugger.h"
#include<stdio.h>

int wmain(int argc, wchar_t *argv[]) {
	SimpleDebugger *sd;

	if (argc <= 2) {
		sd = new SimpleDebugger(L"../test/TestSimpleWindow.exe");
	}
	else {
		sd = new SimpleDebugger(argv[1]);
	}

	if (sd->Setup()) {
		sd->Start();
		sd->Run();
	}

	delete sd;

	return 0;
}