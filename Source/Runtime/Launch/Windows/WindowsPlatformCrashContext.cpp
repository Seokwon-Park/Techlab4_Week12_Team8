#include "EnginePCH.h"
#include "WindowsPlatformCrashContext.h"

#include <filesystem>
#include <DbgHelp.h>
#include <cwchar>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <process.h>

#pragma comment(lib, "dbghelp.lib")

namespace
{
    wchar_t DumpDirectory[MAX_PATH]{};
    volatile LONG DumpStarted = 0;

    // 시작 시 준비하고 프로세스가 끝날 때까지 유지한다. 크래시 경로에서는
    // 스레드 생성이나 요청 저장 공간의 동적 할당을 하지 않는다.
    HANDLE DumpRequestedEvent = nullptr;
    HANDLE DumpThread = nullptr;
    unsigned DumpThreadId = 0;
    DWORD CrashThreadId = 0;
    EXCEPTION_POINTERS* CrashExceptionPointers = nullptr;

    constexpr DWORD CrtInvalidParameterCode = 0xE0000002;
    constexpr DWORD PureCallCode = 0xE0000003;
    constexpr DWORD TerminateCode = 0xE0000004;
    constexpr DWORD AbortCode = 0xE0000005;

    bool WriteMinidump(DWORD ExceptionThreadId, EXCEPTION_POINTERS* ExceptionPointers)
    {
        SYSTEMTIME CrashTime{};
        ::GetLocalTime(&CrashTime);

        wchar_t DumpPath[MAX_PATH]{};
        if (::_snwprintf_s(
            DumpPath, _countof(DumpPath), _TRUNCATE,
            L"%ls\\Hitori-%lu-%04hu-%02hu-%02hu_%02hu-%02hu-%02hu.dmp",
            DumpDirectory, ::GetCurrentProcessId(),
            CrashTime.wYear, CrashTime.wMonth, CrashTime.wDay,
            CrashTime.wHour, CrashTime.wMinute, CrashTime.wSecond) < 0)
        {
            return false;
        }

        const HANDLE DumpFile = ::CreateFileW(DumpPath, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (DumpFile == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        MINIDUMP_EXCEPTION_INFORMATION ExceptionInfo{};
        ExceptionInfo.ThreadId = ExceptionThreadId;
        ExceptionInfo.ExceptionPointers = ExceptionPointers;
        ExceptionInfo.ClientPointers = FALSE;

        const BOOL Written = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), DumpFile, MiniDumpNormal, &ExceptionInfo, nullptr, nullptr);

        ::CloseHandle(DumpFile);

        if (!Written)
        {
            ::DeleteFileW(DumpPath);
            return false;
        }

        wchar_t Message[MAX_PATH + 128]{};
        const int MessageLength = ::swprintf_s(
            Message,
            _countof(Message),
            L"Hitori가 예기치 않게 종료됩니다.\n\n덤프 저장 위치:\n%ls",
            DumpPath);

        ::MessageBoxW(
            nullptr,
            MessageLength >= 0 ? Message : L"Hitori가 크래시했습니다. 덤프를 저장했습니다.",
            L"Hitori Crash Report",
            MB_OK | MB_ICONERROR);

        return true;

    }

    unsigned __stdcall DumpThreadMain(void*)
    {
        if (::WaitForSingleObject(DumpRequestedEvent, INFINITE) != WAIT_OBJECT_0)
        {
            return 1;
        }

        if (!WriteMinidump(CrashThreadId, CrashExceptionPointers))
        {
            ::OutputDebugStringW(L"Hitori: Could not write the crash dump.\n");
        }

        // 이 스레드의 종료가 완료 신호다. 기존 크래시 안내창도 닫힌 뒤 종료한다.
        return 0;
    }

    LONG WINAPI EngineUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionPointers)
    {
        // 덤프 스레드 자체가 실패했을 때 자기 자신을 기다리지 않는다.
        if (::GetCurrentThreadId() == DumpThreadId || !DumpThread)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // 여러 스레드가 동시에 예외를 내더라도 덤프 작성은 한 번만 시도
        if (::InterlockedCompareExchange(&DumpStarted, 1, 0) == 0)
        {
            CrashThreadId = ::GetCurrentThreadId();
            CrashExceptionPointers = ExceptionPointers;
            if (!::SetEvent(DumpRequestedEvent))
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }
        }

        // 예외 처리 프레임을 유지해 EXCEPTION_POINTERS와 그 대상의 수명을 보장한다.
        // 후속 크래시도 기다려야 첫 덤프가 끝나기 전에 프로세스를 종료하지 않는다.
        if (::WaitForSingleObject(DumpThread, INFINITE) != WAIT_OBJECT_0)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }

    // CRT 종료 경로에서도 유효한 예외 컨텍스트를 확보해 기존 덤프 작성기를 사용한다.
    // 로컬 SEH로 처리하므로 호출자의 catch (...) 유무와 관계없이 덤프를 시도한다.
    [[noreturn]] void ReportRuntimeFailure(DWORD ExceptionCode)
    {
        __try
        {
            ::RaiseException(ExceptionCode, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        }
        __except (EngineUnhandledExceptionFilter(GetExceptionInformation()))
        {
        }

        ::TerminateProcess(::GetCurrentProcess(), ExceptionCode);
        ::ExitProcess(ExceptionCode);
    }

    void __cdecl OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t)
    {
        ReportRuntimeFailure(CrtInvalidParameterCode);
    }

    void __cdecl OnPureCall()
    {
        ReportRuntimeFailure(PureCallCode);
    }

    void __cdecl OnTerminate()
    {
        ReportRuntimeFailure(TerminateCode);
    }

    void __cdecl OnAbort(int)
    {
        ReportRuntimeFailure(AbortCode);
    }

    __declspec(noinline) unsigned char CrashStackOverflow(unsigned Depth)
    {
        volatile unsigned char Frame[4096];
        const auto Index = Depth % sizeof(Frame);
        Frame[Index] = 1;

        using FRecurse = unsigned char (*)(unsigned);
        FRecurse volatile Recurse = &CrashStackOverflow;
        Recurse(Depth + 1);

        // 재귀 호출 뒤에도 현재 스택 프레임을 사용
        return Frame[Index];
    }
}

void WindowsPlatformCrashContext::RegisterUnhandledExceptionHandler()
{
    if (DumpThread)
    {
        return;
    }

    wchar_t ExePath[MAX_PATH]{};
    const DWORD Length = ::GetModuleFileNameW(nullptr, ExePath, _countof(ExePath));

    if (Length == 0 || Length >= _countof(ExePath))
    {
        ::OutputDebugStringW(L"Hitori: Could not get the executable path.\n");
        return;
    }

    const std::filesystem::path Directory =
        std::filesystem::path(ExePath).parent_path() / L"Saved" / L"Crashes";

    // WriteMinidump()가 붙이는 파일명과 널 문자까지 들어갈 공간을 확인
    constexpr size_t MaxDumpFileNameLength = _countof(L"\\Hitori-4294967295-9999-12-31_23-59-59.dmp") - 1;

    if (Directory.native().size() + MaxDumpFileNameLength >= _countof(DumpDirectory))
    {
        ::OutputDebugStringW(L"Hitori: Crash dump path is too long.\n");
        return;
    }

    std::error_code Error;
    std::filesystem::create_directories(Directory, Error);
    if (Error)
    {
        ::OutputDebugStringW(L"Hitori: Could not create the crash dump directory.\n");
        return;
    }

    ::wcscpy_s(DumpDirectory, _countof(DumpDirectory), Directory.c_str());

    DumpRequestedEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!DumpRequestedEvent)
    {
        ::OutputDebugStringW(L"Hitori: Could not create the crash dump event.\n");
        return;
    }

    DumpThread = reinterpret_cast<HANDLE>(::_beginthreadex(
        nullptr, 0, &DumpThreadMain, nullptr, 0, &DumpThreadId));
    if (!DumpThread)
    {
        ::CloseHandle(DumpRequestedEvent);
        DumpRequestedEvent = nullptr;
        ::OutputDebugStringW(L"Hitori: Could not create the crash dump thread.\n");
        return;
    }

    // 전용 스레드 준비가 완료된 뒤에만 크래시 처리기를 설치한다.
    ::SetUnhandledExceptionFilter(&EngineUnhandledExceptionFilter);
    ::_set_invalid_parameter_handler(&OnInvalidParameter);
    ::_set_purecall_handler(&OnPureCall);
    std::set_terminate(&OnTerminate);
    std::signal(SIGABRT, &OnAbort);
}

__declspec(noinline) void WindowsPlatformCrashContext::CrashDefault()
{
    constexpr DWORD TestCrashCode = 0xE0000001;
    ::RaiseException(TestCrashCode, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}

__declspec(noinline) void WindowsPlatformCrashContext::CrashDivideByZero()
{
    volatile int Dividend = 42;
    volatile int Divisor = 0;
    volatile int Result = Dividend / Divisor;
    (void)Result;
}

__declspec(noinline) void WindowsPlatformCrashContext::CrashNullPointer()
{
    volatile int* volatile Pointer = nullptr;
    *Pointer = 1;
}

void WindowsPlatformCrashContext::TriggerStackOverflow()
{
    ULONG StackGuarantee = 64 * 1024;
    if (!::SetThreadStackGuarantee(&StackGuarantee))
    {
        ::OutputDebugStringW(L"Hitori: Could not reserve stack for crash handling.\n");
        return;
    }

    CrashStackOverflow(0);
}

__declspec(noinline) void WindowsPlatformCrashContext::CrashInvalidParameter()
{
    char Buffer[2]{};
    ::strcpy_s(Buffer, _countof(Buffer), "abc");
}

__declspec(noinline) void WindowsPlatformCrashContext::CrashPureCall()
{
    // 미정의 동작에 의존하지 않고 CRT의 순수 가상 함수 오류 진입점을 호출한다.
    volatile int Result = ::_purecall();
    (void)Result;
}

__declspec(noinline) void WindowsPlatformCrashContext::CrashTerminate()
{
    std::terminate();
}

__declspec(noinline) void WindowsPlatformCrashContext::CrashAbort()
{
    std::abort();
}
