#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <mutex>
#include <string>
#include <string_view>

namespace WinServ {

    class CEventLog final {
    public:
        CEventLog() = default;
        ~CEventLog();
        CEventLog(const CEventLog&) = delete;
        CEventLog& operator=(const CEventLog&) = delete;

        static bool InstallSource(std::wstring_view source,
            std::wstring_view messageFile,
            DWORD* error = nullptr);
        static bool InstallSource(std::wstring_view source,
            DWORD* error = nullptr);
        static bool UninstallSource(std::wstring_view source,
            DWORD* error = nullptr);

        bool Open(std::wstring_view source, DWORD* error = nullptr);
        void Close() noexcept;

        bool Write(WORD type, DWORD eventId,
            std::wstring_view message,
            DWORD* error = nullptr);

        bool Info(DWORD eventId, std::wstring_view message, DWORD* error = nullptr);
        bool Warning(DWORD eventId, std::wstring_view message, DWORD* error = nullptr);
        bool Error(DWORD eventId, std::wstring_view message, DWORD* error = nullptr);

    private:
        std::mutex mutex_;
        HANDLE handle_ = nullptr;
    };

} // namespace WinServ
