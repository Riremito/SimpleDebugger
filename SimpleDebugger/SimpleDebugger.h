#ifndef __SIMPLEDEBUGGER_H__
#define __SIMPLEDEBUGGER_H__

#include<Windows.h>
#include<ntstatus.h>
#include<string>
#include<vector>

#define ZeroClear(VAR) memset(&VAR, 0, sizeof(decltype(VAR)))

class SimpleDebugger {
private:
	std::wstring wTargetPath;
	STARTUPINFOW siTarget;
	PROCESS_INFORMATION piTarget;
	DEBUG_EVENT deTarget;
	CREATE_PROCESS_DEBUG_INFO cpdiTarget;
	EXCEPTION_DEBUG_INFO ediTarget;

	std::vector<LPVOID> vBreakPoints;
	std::vector<std::vector<BYTE>> vBreakPointCodes;

	void BreakOnEntryPoint();
	void Exception();
	bool SetBreakPoint(LPVOID lpAddress);
	bool DeleteBreakPoint(LPVOID lpAddress);
	bool ReadMemory(LPVOID lpAddress, std::vector<BYTE> &vMemory);
	bool WriteMemory(LPVOID lpAddress, std::vector<BYTE> &vMemory);
	void Break();
	void SingleStep();
	BYTE Opcode(LPVOID lpAddress);

public:
	SimpleDebugger(std::wstring wFilePath);
	~SimpleDebugger();
	bool Setup();
	void Start();
	bool Run();
};

#endif