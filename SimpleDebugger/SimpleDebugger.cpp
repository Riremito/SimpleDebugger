#include"SimpleDebugger.h"

SimpleDebugger::SimpleDebugger(std::wstring wFilePath) {
	wTargetPath = wFilePath;
	ZeroClear(siTarget);
	ZeroClear(siTarget);
	ZeroClear(deTarget);
	ZeroClear(cpdiTarget);
	ZeroClear(ediTarget);
}

SimpleDebugger::~SimpleDebugger() {
	if (piTarget.hThread) {
		CloseHandle(piTarget.hThread);
	}
	if (piTarget.hProcess) {
		CloseHandle(piTarget.hProcess);
	}
}

bool SimpleDebugger::Setup() {
	wprintf(L"*TargetPath = %s\n", wTargetPath.c_str());
	return CreateProcessW(wTargetPath.c_str(), NULL, NULL, NULL, NULL, DEBUG_PROCESS | CREATE_SUSPENDED, NULL, NULL, &siTarget, &piTarget);
}

void SimpleDebugger::Start() {
	ResumeThread(piTarget.hThread);
}

bool SimpleDebugger::Run() {
	while (WaitForDebugEvent(&deTarget, INFINITE)) {
		// check main thread
		if (deTarget.dwThreadId != piTarget.dwThreadId) {
			ContinueDebugEvent(deTarget.dwProcessId, deTarget.dwThreadId, DBG_CONTINUE);
			continue;
		}

		// debug event
		switch (deTarget.dwDebugEventCode) {
			// break point and single step
		case EXCEPTION_DEBUG_EVENT:
			Exception();
			break;
			// break on entry point
		case CREATE_PROCESS_DEBUG_EVENT:
			BreakOnEntryPoint();
			break;
			// stop debugging
		case EXIT_PROCESS_DEBUG_EVENT:
			return true;
			// others
		default:
			break;
		}
		if (!ContinueDebugEvent(deTarget.dwProcessId, deTarget.dwThreadId, DBG_CONTINUE)) {
			return false;
		}
	}
	return false;
}

void SimpleDebugger::BreakOnEntryPoint() {
	cpdiTarget = deTarget.u.CreateProcessInfo;
	printf("*Base %p\n", cpdiTarget.lpBaseOfImage);
	printf("*EntryPoint = %p\n", cpdiTarget.lpStartAddress);

	bool ret = SetBreakPoint(cpdiTarget.lpStartAddress);

	printf("*SetBP = %d\n", ret);
}

bool SimpleDebugger::SetBreakPoint(LPVOID lpAddress) {
	SIZE_T st;
	std::vector<BYTE> vb(1);

	if (!ReadMemory(lpAddress, vb)) {
		return false;
	}

	std::vector<BYTE> vbp;
	vbp.push_back(0xCC);

	if (!WriteMemory(lpAddress, vbp)) {
		return false;
	}

	vBreakPoints.push_back(lpAddress);
	vBreakPointCodes.push_back(vb);

	printf("*SetBP %p (%02X -> CC)\n", vBreakPoints.back(), vb[0]);
	return true;
}

bool SimpleDebugger::DeleteBreakPoint(LPVOID lpAddress) {
	for (size_t i = 0; i < vBreakPoints.size(); i++) {
		if (vBreakPoints[i] == lpAddress) {
			if (!WriteMemory(lpAddress, vBreakPointCodes[i])) {
				return false;
			}
			printf("*DeleteBP %p (CC -> %02X)\n", vBreakPoints.back(), vBreakPointCodes[i][0]);
			vBreakPoints.erase(vBreakPoints.begin() + i);
			vBreakPointCodes.erase(vBreakPointCodes.begin() + i);
			return true;
		}
	}
	return false;
}

bool SimpleDebugger::ReadMemory(LPVOID lpAddress, std::vector<BYTE> &vMemory) {
	SIZE_T st;

	if (!ReadProcessMemory(cpdiTarget.hProcess, lpAddress, &vMemory[0], vMemory.size(), &st)) {
		return false;
	}

	return true;
}

bool SimpleDebugger::WriteMemory(LPVOID lpAddress, std::vector<BYTE> &vMemory) {
	DWORD dw;

	if (!VirtualProtectEx(cpdiTarget.hProcess, lpAddress, vMemory.size(), PAGE_EXECUTE_READWRITE, &dw)) {
		return false;
	}

	SIZE_T st;

	if (!WriteProcessMemory(cpdiTarget.hProcess, lpAddress, &vMemory[0], vMemory.size(), &st)) {
		return false;
	}

	if (!VirtualProtectEx(cpdiTarget.hProcess, lpAddress, vMemory.size(), dw, &dw)) {
		return false;
	}

	return true;
}

void SimpleDebugger::Exception() {
	ediTarget = deTarget.u.Exception;

	switch (ediTarget.ExceptionRecord.ExceptionCode) {
		// break point
	case EXCEPTION_BREAKPOINT:
	case STATUS_WX86_BREAKPOINT:
		Break();
		break;
	case STATUS_SINGLE_STEP:
	case STATUS_WX86_SINGLE_STEP:
		SingleStep();
		break;
	case STATUS_ACCESS_VIOLATION:
		break;
	default:
		printf("%p = %08X\n", ediTarget.ExceptionRecord.ExceptionAddress, ediTarget.ExceptionRecord.ExceptionCode);
		break;
	}
}

void SimpleDebugger::Break() {
	//printf("*Break = %p\n", ediTarget.ExceptionRecord.ExceptionAddress);

	bool ret = DeleteBreakPoint(ediTarget.ExceptionRecord.ExceptionAddress);

	//printf("*DeleteBP = %d\n", ret);

	if (ret) {
		CONTEXT ct;
		ZeroClear(ct);
		ct.ContextFlags = CONTEXT_FULL;

		if (GetThreadContext(cpdiTarget.hThread, &ct)) {
			//printf("rip = %p\n", ct.Rip);
			ct.EFlags |= 0x00000100;
			ct.Rip--;
			ret = SetThreadContext(cpdiTarget.hThread, &ct);
			//printf("*SetRIP = %d\n", ret);
		}
		else {
			//printf("*GetRIP = %d\n", 0);
		}
	}
}

void SimpleDebugger::SingleStep() {
	CONTEXT ct;
	ZeroClear(ct);
	ct.ContextFlags = CONTEXT_FULL;

	if (GetThreadContext(cpdiTarget.hThread, &ct)) {
		std::vector<BYTE> b(1);
		if (ReadMemory((LPVOID)ct.Rip, b)) {
			if (b[0] == 0xE8) {
				//printf("%p (%02X)\n", ct.Rip, b[0]);
				SetBreakPoint((LPVOID)(ct.Rip + 0x05));
				return;
			}
			/*
			else if (b[0] == 0xFF) {
				if (ReadMemory((LPVOID)(ct.Rip + 0x01), b)) {
					if (b[0] == 0x15) {
						SetBreakPoint((LPVOID)(ct.Rip + 0x06));
						return;
					}
				}
			}
			*/
		}
		else {
			printf("%p (ERROR)\n", ct.Rip);
		}

		ct.EFlags |= 0x00000100;
		SetThreadContext(cpdiTarget.hThread, &ct);
	}
	else {
		printf("*Single Step Error");
	}
}

BYTE SimpleDebugger::Opcode(LPVOID lpAddress) {
	SIZE_T st;
	BYTE b;

	if (!ReadProcessMemory(cpdiTarget.hProcess, lpAddress, &b, sizeof(b), &st)) {
		return 0x00;
	}

	return b;
}