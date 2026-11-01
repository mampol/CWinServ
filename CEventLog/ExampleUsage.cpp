// 既存の ExampleService プロジェクトへの統合例です
// mc.exe で生成された EventMessages.h をインクルードし、同じく生成された EventMessages.rc を
// EXE プロジェクトに追加することで、実行可能ファイル自体にメッセージリソースを含めます
#include "CEventLog.h"
#include "EventMessages.h"

// 管理者権限で実行する /install コマンド（サービス登録後に実行）:
// InstallSource(L"ExampleCWinServ", L"サービス.exeへのフルパス", &err);

// 実装例：
// ExampleService のメンバーとして WinServ::CEventLog eventLog_; を定義します
// PreRunService() にて eventLog_.Open(L"ExampleCWinServ");
// ServiceWorker() にて eventLog_.Info(MSG_GENERIC, L"イベントメッセージ");
// PostRunService() にて eventLog_.Close();
 
// 管理者権限で実行する /uninstall コマンド（サービス削除後に実行）:
// UninstallSource(L"ExampleCWinServ") を呼び出します。
