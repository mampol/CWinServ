[English](./README.md) | [Japanese](./README.ja.md)

# CWinServ (C++17 / UTF-16)

## Files
- `CWinServ.h`, `CWinServ.cpp`: standalone Windows service base class
- `Example.cpp`: sample service executable (`wmain`)

## Visual Studio 2022
Add the two base files and optionally the example to a Windows console-style C++ project. Configure C++17; use Unicode. The code explicitly calls Win32 `W` APIs, not `TCHAR` APIs. Link `Advapi32.lib` (Visual Studio may link it through SDK pragmas, but set it explicitly in Linker > Input).

Build as x64 or x86. Compile `Example.cpp` only for the sample program. Launch an *elevated* terminal to register the service:

```bat:install
Example.exe /uninstall
```

```bat:uninstall
Example.exe /uninstall
```

Service installation changes system state: only test on a safe development machine with appropriate privileges. `/uninstall` requires the service to be fully stopped. `DeleteService` marks the service for deletion, and removal can be deferred until handles close. The default is automatic (delayed) startup and three restart-on-failure actions with 60-second delay, matching the old implementation's intent; disabling via setters is supported.

## Implementation notes
- One service instance/process. The dispatcher uses a short-lived process-wide atomic pointer because `ServiceMain` has no caller context. Do not invoke `RunService()` concurrently with multiple instances.
- SCM control callbacks only request stop and signal a manual-reset event. ServiceWorker must *cooperate* and return. It is invoked on the service-main thread; do not detach background workers without joining them before return.
- `WaitForStop()` returns the underlying wait result (`WAIT_OBJECT_0`/`WAIT_TIMEOUT`/`WAIT_FAILED`).
- Pause/continue controls are deliberately *not* advertised or implemented; explicit pause semantics would require derived-class cooperation. Unexpected controls return `ERROR_CALL_NOT_IMPLEMENTED`.
- `OnStopRequested()`/`OnShutdownRequested()` must be fast, nonblocking and `noexcept`. Do not perform cleanup that may take time inside them.
- Service status is reported as START_PENDING -> RUNNING -> STOP_PENDING -> STOPPED (stop path). Worker completion precedes STOPPED. This minimal implementation reports a single pending checkpoint and a 30-second hint on stop; for operations taking substantially longer, add a periodic STOP_PENDING checkpoint reporter and bounded shutdown policy.
- Install errors set `LastErrorCode()`. `CreateServiceW` may succeed before a later `ChangeServiceConfig2W` fails, leaving a partially installed service. In this version no automatic rollback occurs, to avoid undoing a service unexpectedly; investigate and clean up explicitly.
- Failure actions generally apply to crashes/unexpected terminations; for non-crash error exit behavior consider `SERVICE_CONFIG_FAILURE_ACTIONS_FLAG` and the SCM semantics before enabling.
- Path passed to `Setup` must be a **bare path without surrounding quotes**. The installer quotes it. `commandOption` is an already-formatted command-line tail: callers are responsible for argument escaping.
- `SetupAccount` holds a plaintext password in memory and uses it only if a non-empty account is set. Prefer built-in accounts or managed service identities when possible. Credentials are not wiped in this minimal version.
- It omits application-specific `CNamedPipeTh`, `CWinError`, and `CEvntReg`: their implementations were not provided. Log via Event Log or add an injected logger after their APIs are examined. Installing an Event Log *source* is separate from writing events.
- `GetModuleFileNameW` grows a buffer to handle long module paths; additional Windows service path restrictions still apply.
- No Windows compile/runtime test was possible in this environment: build and exercise start/stop, boot-start, failure scenarios, and privilege restrictions in Visual Studio on Windows.
