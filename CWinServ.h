/*
    CWinServ.h
    copyright 2026- mampol.
    note:
    One service per process. The service worker runs on the service-main thread.
    Derived ServiceWorker() must return after IsStopRequested() becomes true.
*/
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <string>

namespace WinServ {

    class CWinServ {
    public:
        CWinServ() = default;
        virtual ~CWinServ() = default;
        CWinServ(const CWinServ&) = delete;
        CWinServ& operator=(const CWinServ&) = delete;

        void Setup(std::wstring serviceName, std::wstring displayName,
                   std::wstring description, std::wstring binaryPath = L"",
                   std::wstring commandOption = L"");
        void SetupAccount(std::wstring account, std::wstring password);
        void SetDelayedAutoStart(bool enabled) noexcept { delayedAutoStart_ = enabled; }
        void SetRestartOnFailure(bool enabled) noexcept { restartOnFailure_ = enabled; }

        bool InstallService();
        bool UninstallService();
        bool RunService();
        DWORD LastErrorCode() const noexcept { return lastError_.load(); }

    protected:
        virtual bool PreInstallService() { return true; }
        virtual bool PostInstallService() { return true; }
        virtual bool PreUninstallService() { return true; }
        virtual bool PostUninstallService() { return true; }
        virtual bool PreRunService() { return true; }
        virtual bool PostRunService() { return true; }

        virtual DWORD ServiceWorker() = 0;
        virtual void OnStopRequested() noexcept {}
        virtual void OnShutdownRequested() noexcept {}

        bool IsStopRequested() const noexcept;
        DWORD WaitForStop(DWORD milliseconds = INFINITE) const noexcept;

    private:
        static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
        static DWORD WINAPI HandlerEx(DWORD control, DWORD eventType, void* eventData, void* context);
        void ServiceMainImpl() noexcept;
        DWORD HandleControl(DWORD control) noexcept;
        bool ReportStatus(DWORD state, DWORD win32ExitCode = NO_ERROR,
                          DWORD waitHint = 0) noexcept;
        bool Fail(DWORD code) noexcept;
        bool IsConfigured() const noexcept;

        std::wstring serviceName_, displayName_, description_, binaryPath_, commandOption_;
        std::wstring account_, password_;
        bool delayedAutoStart_ = true;
        bool restartOnFailure_ = true;
        std::atomic<DWORD> lastError_{NO_ERROR};
        SERVICE_STATUS_HANDLE statusHandle_ = nullptr;
        SERVICE_STATUS status_{};
        mutable std::mutex statusMutex_;
        HANDLE stopEvent_ = nullptr;
        std::atomic<bool> stopRequested_{false};
        std::atomic<bool> runEntered_{false};
    };

} // namespace WinServ
