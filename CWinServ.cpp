/*
    CWinServ.cpp
    copyright 2026- mampol.
*/
#include "CWinServ.h"
#include <utility>
#include <vector>

namespace WinServ {

    namespace {

        std::atomic<CWinServ*> g_instance{nullptr};

        struct ScHandle {
            SC_HANDLE value = nullptr;
            explicit ScHandle(SC_HANDLE h = nullptr) noexcept : value(h) {}
            ~ScHandle() { if (value) CloseServiceHandle(value); }

            ScHandle(const ScHandle&) = delete;
            ScHandle& operator=(const ScHandle&) = delete;
            explicit operator bool() const noexcept { return value != nullptr; }
        };

        std::wstring ModulePath() {
            std::vector<wchar_t> buffer(MAX_PATH);
            for (;;) {
                DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (len == 0) return {};
                if (len < buffer.size()) return std::wstring(buffer.data(), len);
                if (buffer.size() >= 32768) return {};
                buffer.resize(buffer.size() * 2);
            }
        }

        std::wstring BinaryCommandLine(const std::wstring& path, const std::wstring& options) {
            std::wstring cmd = L"\"" + path + L"\"";
            if (!options.empty()) cmd += L" " + options;
            return cmd;
        }
    }

    void CWinServ::Setup(std::wstring name, std::wstring display, std::wstring description,
                         std::wstring binary, std::wstring options) {
        serviceName_ = std::move(name);
        displayName_ = std::move(display);
        description_ = std::move(description);
        binaryPath_ = std::move(binary);
        commandOption_ = std::move(options);
    }

    void CWinServ::SetupAccount(std::wstring account, std::wstring password) {
        account_ = std::move(account);
        password_ = std::move(password);
    }

    bool CWinServ::Fail(DWORD code) noexcept {
        lastError_.store(code);
        SetLastError(code);
        return false;
    }

    bool CWinServ::IsConfigured() const noexcept {
        return !serviceName_.empty() && !displayName_.empty();
    }

    bool CWinServ::InstallService() {
        lastError_ = NO_ERROR;

        if (!IsConfigured())
            return Fail(ERROR_INVALID_PARAMETER);

        if (!PreInstallService())
            return Fail(ERROR_CANCELLED);

        const std::wstring path = binaryPath_.empty() ? ModulePath() : binaryPath_;
        if (path.empty() || path.find(L'"') != std::wstring::npos)
            return Fail(ERROR_INVALID_PARAMETER);

        std::wstring cmd = BinaryCommandLine(path, commandOption_);

        ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
        if (!scm) return Fail(GetLastError());

        const wchar_t* account = account_.empty() ? nullptr : account_.c_str();
        const wchar_t* password = account_.empty() ? nullptr : password_.c_str();

        ScHandle service(CreateServiceW(scm.value,
            serviceName_.c_str(),
            displayName_.c_str(),
            SERVICE_CHANGE_CONFIG |
            SERVICE_QUERY_STATUS |
            SERVICE_START |
            DELETE,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            cmd.c_str(), nullptr, nullptr, nullptr,
            account, password));
        if (!service) return Fail(GetLastError());

        auto rollback = [&service, this](DWORD error) -> bool
            {
                DeleteService(service.value);
                return Fail(error);
            };

        SERVICE_DESCRIPTIONW desc{const_cast<LPWSTR>(description_.c_str())};
        if (!ChangeServiceConfig2W(service.value, SERVICE_CONFIG_DESCRIPTION, &desc))
            return rollback(GetLastError());

        SERVICE_DELAYED_AUTO_START_INFO delay{delayedAutoStart_ ? TRUE : FALSE};
        if (!ChangeServiceConfig2W(service.value, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delay))
            return rollback(GetLastError());

        if (restartOnFailure_) {
            SC_ACTION actions[3] = {
                {SC_ACTION_RESTART, 60000},
                {SC_ACTION_RESTART, 60000},
                {SC_ACTION_RESTART, 60000}
            };
            SERVICE_FAILURE_ACTIONSW failure{};
            failure.dwResetPeriod = 86400;
            failure.cActions = 3;
            failure.lpsaActions = actions;
            if (!ChangeServiceConfig2W(service.value,
                SERVICE_CONFIG_FAILURE_ACTIONS, &failure))
                return rollback(GetLastError());
        }

        if (!PostInstallService())
            return Fail(ERROR_CANCELLED);

        return true;
    }

    bool CWinServ::UninstallService() {
        lastError_ = NO_ERROR;

        if (serviceName_.empty())
            return Fail(ERROR_INVALID_PARAMETER);

        if (!PreUninstallService())
            return Fail(ERROR_CANCELLED);

        ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm) return Fail(GetLastError());

        ScHandle service(OpenServiceW(scm.value,
            serviceName_.c_str(),
            SERVICE_QUERY_STATUS |
            DELETE));
        if (!service) return Fail(GetLastError());

        SERVICE_STATUS_PROCESS state{};
        DWORD needed = 0;
        if (!QueryServiceStatusEx(service.value, SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&state), sizeof(state), &needed))
            return Fail(GetLastError());

        if (state.dwCurrentState != SERVICE_STOPPED)
            return Fail(ERROR_SERVICE_NOT_ACTIVE);

        if (!DeleteService(service.value))
            return Fail(GetLastError());

        if (!PostUninstallService())
            return Fail(ERROR_CANCELLED);

        return true;
    }

    bool CWinServ::RunService() {
        lastError_ = NO_ERROR;

        if (!IsConfigured())
            return Fail(ERROR_INVALID_PARAMETER);

        if (runEntered_.exchange(true))
            return Fail(ERROR_ALREADY_EXISTS);

        CWinServ* expected = nullptr;
        if (!g_instance.compare_exchange_strong(expected, this)) {
            runEntered_ = false;
            return Fail(ERROR_ALREADY_EXISTS);
        }

        SERVICE_TABLE_ENTRYW table[] = {
            { const_cast<LPWSTR>(serviceName_.c_str()), &CWinServ::ServiceMain },
            { nullptr, nullptr }
        };

        const BOOL success = StartServiceCtrlDispatcherW(table);
        const DWORD err = success ? NO_ERROR : GetLastError();
        g_instance.store(nullptr);
        runEntered_ = false;
        if (!success) return Fail(err);

        return lastError_ == NO_ERROR;
    }

    void WINAPI CWinServ::ServiceMain(DWORD, LPWSTR*) {
        if (auto* self = g_instance.load()) self->ServiceMainImpl();
    }

    bool CWinServ::ReportStatus(DWORD state, DWORD error, DWORD waitHint) noexcept {
        std::lock_guard<std::mutex> lock(statusMutex_);
        if (!statusHandle_) return false;

        if (state == SERVICE_RUNNING && stopRequested_ &&
            status_.dwCurrentState == SERVICE_STOP_PENDING)
            return true;

        status_.dwCurrentState = state;
        status_.dwWin32ExitCode = error;
        status_.dwWaitHint = waitHint;
        status_.dwControlsAccepted = (state == SERVICE_RUNNING) ?
            (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN) : 0;
        if (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING) {
            ++status_.dwCheckPoint;
        }
        else {
            status_.dwCheckPoint = 0;
        }
        if (!SetServiceStatus(statusHandle_, &status_)) {
            lastError_ = GetLastError();
            return false;
        }
        return true;
    }

    void CWinServ::ServiceMainImpl() noexcept {
        stopRequested_ = false;

        statusHandle_ = RegisterServiceCtrlHandlerExW(serviceName_.c_str(), &CWinServ::HandlerEx, this);
        if (!statusHandle_) {
            lastError_ = GetLastError();
            return;
        }

        status_ = {};
        status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        if (!ReportStatus(SERVICE_START_PENDING, NO_ERROR, 10000)) return;

        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stopEvent_) {
            DWORD err = GetLastError(); lastError_ = err;
            ReportStatus(SERVICE_STOPPED, err); return;
        }

        DWORD result = ERROR_SUCCESS;
        bool started = false;
        try {
            if (!PreRunService()) {
                result = ERROR_CANCELLED;
            }
            else {
                started = true;
            }
        }
        catch (...) {
            result = ERROR_UNHANDLED_EXCEPTION;
        }

        if (started) {
            if (stopRequested_) {
                ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 30000);
            }
            else if (!ReportStatus(SERVICE_RUNNING)) {
                result = lastError_;
            }
            if (result == ERROR_SUCCESS) {
                try {
                    result = ServiceWorker();
                }
                catch (...) {
                    result = ERROR_UNHANDLED_EXCEPTION;
                }
            }
            try {
                if (!PostRunService() && result == ERROR_SUCCESS) {
                    result = ERROR_CANCELLED;
                }
            }
            catch (...) {
                if (result == ERROR_SUCCESS) result = ERROR_UNHANDLED_EXCEPTION;
            }
        }
        stopRequested_ = true;
        ReportStatus(SERVICE_STOPPED, result);
        if (result != ERROR_SUCCESS) lastError_ = result;
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }

    DWORD WINAPI CWinServ::HandlerEx(DWORD control, DWORD, void*, void* context) {
        auto* self = static_cast<CWinServ*>(context);
        return self ? self->HandleControl(control) : ERROR_INVALID_HANDLE;
    }

    DWORD CWinServ::HandleControl(DWORD control) noexcept {
        switch (control) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            if (stopRequested_.exchange(true))
                return NO_ERROR;

            ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 30000);

            if (stopEvent_)
                SetEvent(stopEvent_);

            if (control == SERVICE_CONTROL_STOP) {
                OnStopRequested();
            }
            else {
                OnShutdownRequested();
            }
            return NO_ERROR;
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
    }

    bool CWinServ::IsStopRequested() const noexcept { return stopRequested_.load(); }

    DWORD CWinServ::WaitForStop(DWORD milliseconds) const noexcept {
        HANDLE evt = stopEvent_;
        return evt ? WaitForSingleObject(evt, milliseconds) : WAIT_FAILED;
    }

} // namespace WinServ
