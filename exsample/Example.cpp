// CWinServ Example only
#include "../CWinServ.h"
#include "../CEventLog/CEventLog.h"
#include "../CEventLog/EventMessages.h"

#include <Windows.h>
#include <iostream>
#include <string>

#define SERV_NAME       L"ExampleCWinServ"

class ExampleService final : public WinServ::CWinServ {

private:
    WinServ::CEventLog m_eventLog;

protected:
    bool PostInstallService() override
    {
        return WinServ::CEventLog::InstallSource(
            SERV_NAME);
    }

    bool PreRunService() override
    {
        return m_eventLog.Open(SERV_NAME);
    }

    DWORD ServiceWorker() override
    {
        m_eventLog.Info(
            MSG_GENERIC,
            L"サービスを開始しました"
        );

        WaitForStop(INFINITE);

        m_eventLog.Info(
            MSG_GENERIC,
            L"サービスを停止します"
        );

        return ERROR_SUCCESS;
    }

    bool PostRunService() override
    {
        m_eventLog.Close();
        return true;
    }

    bool PostUninstallService() override
    {
        return WinServ::CEventLog::UninstallSource(
            SERV_NAME);
    }

};

int wmain(int argc, wchar_t** argv)
{
    ExampleService service;

    service.Setup(SERV_NAME,
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
