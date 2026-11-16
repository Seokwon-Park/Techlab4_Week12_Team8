#pragma once

namespace WindowsPlatformCrashContext
{
    void RegisterUnhandledExceptionHandler();

    void CrashDefault();
    void CrashDivideByZero();
    void CrashNullPointer();
    void TriggerStackOverflow();
}