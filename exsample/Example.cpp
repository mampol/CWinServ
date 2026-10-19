// CWinServ Example only
#include "../CWinServ.h"
#include <Windows.h>
#include <iostream>
#include <string>

#ifdef _DEBUG
#pragma comment(lib, "../x64/Debug/CWinServ.lib")
#else
#pragma comment(lib, "../x64/Release/CWinServ.lib")
#endif

class ExampleService final : public WinServ::CWinServ {
protected:
    DWORD ServiceWorker() override {
        while (!IsStopRequested()) {
            DWORD wait = WaitForStop(1000);
            if (wait == WAIT_OBJECT_0) break;
            if (wait == WAIT_FAILED) return GetLastError();
        }
        return ERROR_SUCCESS;
    }
};

int wmain(int argc, wchar_t** argv)
{
    ExampleService service;

    service.Setup(L"ExampleCWinServ",
        L"Example CWinServ",
        L"Simple C++17 UTF-16 Windows service");

    if (argc == 2 && std::wstring(argv[1]) == L"/install") {
        std::wcout << L"Install ExampleCWinServ" << L'\n';
        if (service.InstallService()) {
            std::wcout << L"success" << L'\n';
            return 0;
        }
        else {
            const DWORD error = static_cast<int>(service.LastErrorCode());
            std::wcout << L"Install failed\nerror code: " << error << L'\n';
            return static_cast<int>(error);
        }
    }

    if (argc == 2 && std::wstring(argv[1]) == L"/uninstall") {
        std::wcout << L"Uninstall ExsampleCWinServ" << L'\n';
        if (service.UninstallService()) {
            std::wcout << L"success" << L'\n';
            return 0;
        }
        else {
            const DWORD error = static_cast<int>(service.LastErrorCode());
            std::wcout << L"Uninstall failed\nerror code: " << error << L'\n';
            return static_cast<int>(error);
        }
    }

    return service.RunService() ? 0 : static_cast<int>(service.LastErrorCode());
}
