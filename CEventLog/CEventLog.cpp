#include "CEventLog.h"
#include <string>

namespace WinServ {

    namespace {

        constexpr wchar_t kSourceRoot[] =
            L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";

        bool ResultError(DWORD code, DWORD* out) noexcept {
            if (out) *out = code;
            if (code != ERROR_SUCCESS) SetLastError(code);
            return code == ERROR_SUCCESS;
        }

        bool ValidSource(std::wstring_view source) noexcept {
            return !source.empty() && source.find_first_of(L"\\/") == std::wstring_view::npos;
        }

    } // namespace

    CEventLog::~CEventLog() {
        Close();
    }

    bool CEventLog::InstallSource(
        std::wstring_view source,
        std::wstring_view messageFile,
        DWORD* error) {
        if (!ValidSource(source) || messageFile.empty())
            return ResultError(ERROR_INVALID_PARAMETER, error);

        const std::wstring file(messageFile);
        if (file.size() < 3 || file[1] != L':' || (file[2] != L'\\' && file[2] != L'/'))
            return ResultError(ERROR_INVALID_PARAMETER, error);

        const std::wstring keyPath = std::wstring(kSourceRoot) + std::wstring(source);
        HKEY key = nullptr;
        DWORD disposition = 0;
        LSTATUS rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            keyPath.c_str(), 0,
            nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, &disposition);
        if (rc != ERROR_SUCCESS)
            return ResultError(static_cast<DWORD>(rc), error);
        if (disposition != REG_CREATED_NEW_KEY) {
            RegCloseKey(key);
            return ResultError(ERROR_ALREADY_EXISTS, error);
        }

        rc = RegSetValueExW(key, L"EventMessageFile", 0, REG_EXPAND_SZ,
                            reinterpret_cast<const BYTE*>(file.c_str()),
                            static_cast<DWORD>((file.size() + 1) * sizeof(wchar_t)));
        if (rc == ERROR_SUCCESS) {
            constexpr DWORD types = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
                                    EVENTLOG_INFORMATION_TYPE;
            rc = RegSetValueExW(key, L"TypesSupported", 0, REG_DWORD,
                                reinterpret_cast<const BYTE*>(&types), sizeof(types));
        }

        RegCloseKey(key);
        if (rc != ERROR_SUCCESS) {
            RegDeleteKeyW(HKEY_LOCAL_MACHINE, keyPath.c_str());
        }

        return ResultError(static_cast<DWORD>(rc), error);
    }

    bool CEventLog::InstallSource(
        std::wstring_view source,
        DWORD* error) {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(
            nullptr,
            path,
            MAX_PATH
        );
        return InstallSource(source, path, error);
    }

    bool CEventLog::UninstallSource(
        std::wstring_view source,
        DWORD* error) {
        if (!ValidSource(source)) return ResultError(ERROR_INVALID_PARAMETER, error);

        const std::wstring keyPath = std::wstring(kSourceRoot) + std::wstring(source);
        const LSTATUS rc = RegDeleteKeyW(HKEY_LOCAL_MACHINE, keyPath.c_str());
        return ResultError(static_cast<DWORD>(rc), error);
    }

    bool CEventLog::Open(
        std::wstring_view source,
        DWORD* error) {
        if (!ValidSource(source)) return ResultError(ERROR_INVALID_PARAMETER, error);

        std::lock_guard<std::mutex> guard(mutex_);

        if (handle_) return ResultError(ERROR_ALREADY_INITIALIZED, error);

        const std::wstring name(source);
        HANDLE h = RegisterEventSourceW(nullptr, name.c_str());
        if (!h) return ResultError(GetLastError(), error);
        handle_ = h;
        return ResultError(ERROR_SUCCESS, error);
    }

    void CEventLog::Close() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if (handle_) {
            DeregisterEventSource(handle_);
            handle_ = nullptr;
        }
    }

    bool CEventLog::Write(
        WORD type, DWORD eventId,
        std::wstring_view message,
        DWORD* error) {
        const std::wstring owned(message);
        const wchar_t* strings[] = { owned.c_str() };
        std::lock_guard<std::mutex> guard(mutex_);
        if (!handle_) return ResultError(ERROR_INVALID_HANDLE, error);
        if (!ReportEventW(handle_, type, 0, eventId, nullptr,
                          1, 0, strings, nullptr))
            return ResultError(GetLastError(), error);
        return ResultError(ERROR_SUCCESS, error);
    }

    bool CEventLog::Info(DWORD id, std::wstring_view msg, DWORD* err) {
        return Write(EVENTLOG_INFORMATION_TYPE, id, msg, err);
    }
    bool CEventLog::Warning(DWORD id, std::wstring_view msg, DWORD* err) {
        return Write(EVENTLOG_WARNING_TYPE, id, msg, err);
    }
    bool CEventLog::Error(DWORD id, std::wstring_view msg, DWORD* err) {
        return Write(EVENTLOG_ERROR_TYPE, id, msg, err);
    }

} // namespace WinServ
