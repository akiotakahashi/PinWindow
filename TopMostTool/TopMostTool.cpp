#include "stdafx.h"
#include "TopMostTool.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::ApplicationModel;

#define WM_TRAYICON (WM_USER + 1)

// StartupTask の非同期操作の結果を UI スレッドへ渡すメッセージ。
// lParam は操作の成否 (TRUE/FALSE)、wParam は StartupTaskState (成功時のみ有効)。
#define WM_APP_STARTUP_QUERIED (WM_APP + 1)
#define WM_APP_STARTUP_ENABLE_REQUESTED (WM_APP + 2)
#define WM_APP_STARTUP_DISABLED (WM_APP + 3)

#define HOT_KEY_ID_PAUSE 1
#define HOT_KEY_ID_WIN_DEL 2

#define MAX_LOADSTRING 100

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名
BOOL startupEnabled = FALSE;

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
static std::wstring LoadResString(UINT id);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	hInst = hInstance; // グローバル変数にインスタンス処理を格納します。

	// グローバル文字列を初期化しています。
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_TOPMOSTTOOL, szWindowClass, MAX_LOADSTRING);

	// 2 つ目のプロセスはホットキーを登録できないため、起動済みである旨を通知して終了する。
	auto hMutex = CreateMutexW(nullptr, FALSE, L"Local\\59756AkioTakahashi.PinWindow");
	if(hMutex && GetLastError() == ERROR_ALREADY_EXISTS)
	{
		auto text = LoadResString(IDS_ALREADY_RUNNING);
		MessageBox(nullptr, text.c_str(), szTitle, MB_OK | MB_ICONINFORMATION);
		CloseHandle(hMutex);
		return 0;
	}

	// アプリケーションの初期化を実行します:
	MyRegisterClass(hInstance);
	if(!InitInstance(hInstance, nCmdShow))
	{
		if(hMutex) {
			CloseHandle(hMutex);
		}
		return FALSE;
	}

	MSG msg;
	while(GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if(hMutex) {
		CloseHandle(hMutex);
	}
	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DOLPHIN));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_DOLPHIN));
	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	auto hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
		nullptr, nullptr, hInstance, nullptr);
	if(!hWnd)
	{
		return FALSE;
	}
	ShowWindow(hWnd, SW_HIDE /*nCmdShow*/);
	UpdateWindow(hWnd);
	return TRUE;
}

static NOTIFYICONDATA CreateNotifyIconData(HWND hWnd, UINT uFlags = 0)
{
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = hWnd;
	nid.uID = 0;
	nid.uVersion = NOTIFYICON_VERSION_4;
	nid.uFlags = uFlags | NIF_SHOWTIP;
	return nid;
}

static BOOL ShowBalloon(HWND hWnd, DWORD dwInfoFlags, const wchar_t* title, const wchar_t* msg)
{
	auto nid = CreateNotifyIconData(hWnd, NIF_INFO);
	nid.dwInfoFlags = dwInfoFlags | NIIF_NOSOUND;
	StringCchCopy(nid.szInfo, ARRAYSIZE(nid.szInfo), msg);
	StringCchCopy(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), title);
	return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

// callback はスレッドプールのスレッドで呼び出される場合がある。
// 取得に失敗した場合は nullptr を渡す。
static void InvokeStartupAsync(std::function<void(StartupTask)> callback)
{
	try {
		auto startupTaskAsync = StartupTask::GetAsync(L"StartupAppId");
		startupTaskAsync.Completed([callback](auto&& task, auto&& status) {
			StartupTask startupTask = nullptr;
			if (status == AsyncStatus::Completed) {
				try {
					startupTask = task.GetResults();
				}
				catch (...) {
				}
			}
			callback(startupTask);
		});
	}
	catch (...) {
		callback(nullptr);
	}
}

static void QueryStartup(HWND hWnd)
{
	InvokeStartupAsync([hWnd](StartupTask task) {
		if (!task) {
			PostMessage(hWnd, WM_APP_STARTUP_QUERIED, 0, FALSE);
			return;
		}
		PostMessage(hWnd, WM_APP_STARTUP_QUERIED, static_cast<WPARAM>(task.State()), TRUE);
	});
}

static void RequestStartup(HWND hWnd)
{
	InvokeStartupAsync([hWnd](StartupTask task) {
		if (!task) {
			PostMessage(hWnd, WM_APP_STARTUP_ENABLE_REQUESTED, 0, FALSE);
			return;
		}
		try {
			task.RequestEnableAsync().Completed([hWnd](auto&& operation, auto&& status) {
				if (status != AsyncStatus::Completed) {
					PostMessage(hWnd, WM_APP_STARTUP_ENABLE_REQUESTED, 0, FALSE);
					return;
				}
				PostMessage(hWnd, WM_APP_STARTUP_ENABLE_REQUESTED, static_cast<WPARAM>(operation.GetResults()), TRUE);
			});
		}
		catch (...) {
			PostMessage(hWnd, WM_APP_STARTUP_ENABLE_REQUESTED, 0, FALSE);
		}
	});
}

static void DisableStartup(HWND hWnd)
{
	InvokeStartupAsync([hWnd](StartupTask task) {
		if (!task) {
			PostMessage(hWnd, WM_APP_STARTUP_DISABLED, 0, FALSE);
			return;
		}
		try {
			task.Disable();
		}
		catch (...) {
			PostMessage(hWnd, WM_APP_STARTUP_DISABLED, 0, FALSE);
			return;
		}
		PostMessage(hWnd, WM_APP_STARTUP_DISABLED, 0, TRUE);
	});
}

static bool IsStartupEnabledState(StartupTaskState state)
{
	return state == StartupTaskState::Enabled || state == StartupTaskState::EnabledByPolicy;
}

// 文字列リソースは UI 言語に応じて ja-JP / en-US から選択される。
static std::wstring LoadResString(UINT id)
{
	const wchar_t* p = nullptr;
	auto len = LoadStringW(hInst, id, reinterpret_cast<LPWSTR>(&p), 0);
	if(len <= 0 || !p) {
		return std::wstring();
	}
	return std::wstring(p, len);
}

static void ShowMessage(HWND hWnd, UINT textId, UINT captionId, UINT type)
{
	auto text = LoadResString(textId);
	auto caption = LoadResString(captionId);
	MessageBox(hWnd, text.c_str(), caption.c_str(), type);
}

struct AppError
{
	UINT messageId = 0;
	DWORD code = 0;     // 失敗した API の Win32 エラーコードまたは HRESULT。0 は詳細なし。
};

static std::wstring FormatSystemMessage(DWORD code)
{
	if(code == 0) {
		return std::wstring();
	}
	LPWSTR buf = nullptr;
	auto len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
	std::wstring text = (len > 0 && buf) ? std::wstring(buf, len) : std::wstring();
	LocalFree(buf);
	auto end = text.find_last_not_of(L" \r\n");
	text.erase(end == std::wstring::npos ? 0 : end + 1);
	return text;
}

static bool AddTrayIcon(HWND hWnd, AppError& error)
{
	static HICON hIcon = nullptr;
	if(!hIcon) {
		auto hr = LoadIconMetric(hInst, MAKEINTRESOURCE(IDI_DOLPHIN), LIM_SMALL, &hIcon);
		if(FAILED(hr)) {
			error = { IDS_ERR_ICON_LOAD, static_cast<DWORD>(hr) };
			return false;
		}
	}
	auto nid = CreateNotifyIconData(hWnd, NIF_MESSAGE | NIF_TIP | NIF_ICON);
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = hIcon;
	StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), szTitle);
	SetLastError(0);
	if(!Shell_NotifyIcon(NIM_ADD, &nid)) {
		error = { IDS_ERR_TRAY_ADD, GetLastError() };
		return false;
	}
	SetLastError(0);
	if(!Shell_NotifyIcon(NIM_SETVERSION, &nid)) {
		error = { IDS_ERR_TRAY_VERSION, GetLastError() };
		return false;
	}
	return true;
}

static bool InitializeApp(HWND hWnd, AppError& error)
{
	// register hot-key
	auto pauseRegistered = RegisterHotKey(hWnd, HOT_KEY_ID_PAUSE, MOD_NOREPEAT, VK_PAUSE);
	auto winDelRegistered = RegisterHotKey(hWnd, HOT_KEY_ID_WIN_DEL, MOD_NOREPEAT | MOD_WIN, VK_DELETE);
	if(!pauseRegistered && !winDelRegistered) {
		error = { IDS_ERR_HOTKEY, GetLastError() };
		return false;
	}
	// add a system tray icon
	if(!AddTrayIcon(hWnd, error)) {
		return false;
	}
	if(!pauseRegistered) {
		ShowBalloon(hWnd, NIIF_WARNING, szTitle, LoadResString(IDS_BALLOON_WIN_DEL_ONLY).c_str());
	}
	else if(!winDelRegistered) {
		ShowBalloon(hWnd, NIIF_WARNING, szTitle, LoadResString(IDS_BALLOON_PAUSE_ONLY).c_str());
	}
	else {
		ShowBalloon(hWnd, NIIF_INFO, szTitle, LoadResString(IDS_BALLOON_READY).c_str());
	}
	//
	QueryStartup(hWnd);
	//
	return true;
}

// タスクバーとデスクトップはシェルが表示順序を管理するウインドウである。
// 最前面属性を切り替えると、タスクバーが他のウインドウの背面に表示されるなどの影響があるため対象から除く。
static bool IsExcludedWindow(HWND hwnd, HWND hWndSelf)
{
	if(hwnd == hWndSelf) {
		return true;
	}
	WCHAR className[64] = {};
	if(GetClassName(hwnd, className, ARRAYSIZE(className)) == 0) {
		return false;
	}
	static const wchar_t* const EXCLUDED_CLASSES[] = {
		L"Shell_TrayWnd",
		L"Shell_SecondaryTrayWnd",
		L"Progman",
		L"WorkerW",
	};
	for(auto name : EXCLUDED_CLASSES) {
		if(wcscmp(className, name) == 0) {
			return true;
		}
	}
	return false;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// explorer.exe の再起動時に通知されるため、トレイアイコンを追加し直す。
	static const UINT WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");
	if(message == WM_TASKBARCREATED) {
		AppError error;
		AddTrayIcon(hWnd, error);
		return 0;
	}
	switch(message)
	{
	case WM_CREATE:
	{
		AppError error;
		auto ok = InitializeApp(hWnd, error);
		if(!ok) {
			auto text = LoadResString(error.messageId);
			auto detail = FormatSystemMessage(error.code);
			if(!detail.empty()) {
				text += L"\n\n" + detail;
			}
			MessageBox(hWnd, text.c_str(), szTitle, MB_OK | MB_ICONERROR);
			// -1 を返すとウインドウが破棄され、WM_DESTROY でトレイアイコンとホットキーが解放される。
			return -1;
		}
		break;
	}
	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
		break;
	}
	case WM_DESTROY:
	{
		UnregisterHotKey(hWnd, HOT_KEY_ID_PAUSE);
		UnregisterHotKey(hWnd, HOT_KEY_ID_WIN_DEL);
		auto nid = CreateNotifyIconData(hWnd);
		Shell_NotifyIcon(NIM_DELETE, &nid);
		PostQuitMessage(0);
		break;
	}
	case WM_COMMAND:
	{
		auto wmId = LOWORD(wParam);
		// 選択されたメニューの解析:
		switch(wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_AUTO_STARTUP:
			if (!startupEnabled) {
				RequestStartup(hWnd);
			}
			else {
				DisableStartup(hWnd);
			}
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	}
	case WM_APP_STARTUP_QUERIED:
		startupEnabled = lParam && IsStartupEnabledState(static_cast<StartupTaskState>(wParam));
		break;
	case WM_APP_STARTUP_ENABLE_REQUESTED:
	{
		auto state = static_cast<StartupTaskState>(wParam);
		if (lParam && IsStartupEnabledState(state)) {
			startupEnabled = TRUE;
		}
		else if (lParam && state == StartupTaskState::DisabledByUser) {
			ShowMessage(hWnd, IDS_STARTUP_DISABLED_BY_USER, IDS_APP_TITLE, MB_OK | MB_ICONINFORMATION);
		}
		else if (lParam && state == StartupTaskState::DisabledByPolicy) {
			ShowMessage(hWnd, IDS_STARTUP_DISABLED_BY_POLICY, IDS_APP_TITLE, MB_OK | MB_ICONINFORMATION);
		}
		else {
			ShowMessage(hWnd, IDS_STARTUP_ENABLE_FAILED, IDS_ERROR_CAPTION, MB_OK | MB_ICONERROR);
		}
		break;
	}
	case WM_APP_STARTUP_DISABLED:
		if (lParam) {
			startupEnabled = FALSE;
		}
		else {
			ShowMessage(hWnd, IDS_STARTUP_DISABLE_FAILED, IDS_ERROR_CAPTION, MB_OK | MB_ICONERROR);
		}
		break;
	case WM_HOTKEY:
		switch(wParam)
		{
		case HOT_KEY_ID_PAUSE:
		case HOT_KEY_ID_WIN_DEL:
		{
			auto success = false;
			UINT titleId = IDS_TOGGLE_FAILED;
			UINT msgId = IDS_TOGGLE_UNKNOWN_ERROR;
			// バルーン本文は szInfo の長さで切り詰められるため、それを超えて取得する必要はない。
			WCHAR windowText[ARRAYSIZE(NOTIFYICONDATA::szInfo)] = {};
			auto hwnd = GetForegroundWindow();
			if(hwnd == NULL) {
				msgId = IDS_TOGGLE_NO_FOREGROUND;
			}
			else if(IsExcludedWindow(hwnd, hWnd)) {
				msgId = IDS_TOGGLE_EXCLUDED;
			}
			else {
				SetLastError(0);
				auto style = GetWindowLong(hwnd, GWL_EXSTYLE);
				if(style == 0 && GetLastError() != 0) {
					msgId = IDS_TOGGLE_GET_FAILED;
				}
				else {
					auto topmost = (style & WS_EX_TOPMOST) ? true : false;
					auto ret = SetWindowPos(hwnd, topmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					if(!ret) {
						msgId = IDS_TOGGLE_SET_FAILED;
					}
					else {
						FLASHWINFO fi = {};
						fi.cbSize = sizeof(fi);
						fi.hwnd = hwnd;
						fi.uCount = 3;
						fi.dwTimeout = 0;
						fi.dwFlags = FLASHW_CAPTION;
						FlashWindowEx(&fi);
						success = true;
						titleId = topmost ? IDS_TOGGLE_UNPINNED : IDS_TOGGLE_PINNED;
						GetWindowText(hwnd, windowText, ARRAYSIZE(windowText));
					}
				}
			}
			auto title = LoadResString(titleId);
			auto msg = success ? std::wstring(windowText) : LoadResString(msgId);
			ShowBalloon(hWnd, success ? NIIF_INFO : NIIF_ERROR, title.c_str(), msg.c_str());
			break;
		}
		}
		break;
	case WM_TRAYICON:
	{
		// NOTIFYICON_VERSION_4 では LOWORD(lParam) がイベント、wParam がアンカー座標である。
		switch(LOWORD(lParam)) {
		case NIN_SELECT:
		case NIN_KEYSELECT:
		case WM_CONTEXTMENU:
		{
			POINT curPoint = { GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam) };
			auto hMenuBar = LoadMenu(hInst, MAKEINTRESOURCE(IDC_TOPMOSTTOOL));
			if(!hMenuBar) {
				break;
			}
			auto hMenu = GetSubMenu(hMenuBar, 0);
			CheckMenuItem(hMenu, IDM_AUTO_STARTUP, MF_BYCOMMAND | (startupEnabled ? MFS_CHECKED : MFS_UNCHECKED));
			// 前面にしないと、メニュー外をクリックしてもメニューが閉じない。
			SetForegroundWindow(hWnd);
			TrackPopupMenu(hMenu, 0, curPoint.x, curPoint.y, 0, hWnd, NULL);
			PostMessage(hWnd, WM_NULL, 0, 0);
			DestroyMenu(hMenuBar);
			break;
		}
		}
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch(message)
	{
	case WM_INITDIALOG:
	{
		// 書式関数 (StringCchPrintf など) は CRT の書式出力処理一式を静的リンクし、
		// exe が約 30 KB 増えるため、文字列の連結で組み立てる。
		std::wstring versionText;
		try {
			auto version = Package::Current().Id().Version();
			versionText = LoadResString(IDS_ABOUT_VERSION) + L" "
				+ std::to_wstring(version.Major) + L"." + std::to_wstring(version.Minor) + L"."
				+ std::to_wstring(version.Build) + L"." + std::to_wstring(version.Revision);
		}
		catch (...) {
			// パッケージ ID を持たない状態 (exe の直接実行) では版数を取得できない。
			versionText = LoadResString(IDS_ABOUT_VERSION_UNKNOWN);
		}
		auto text = std::wstring(szTitle) + L", " + versionText;
		SetDlgItemText(hDlg, IDC_VERSION_TEXT, text.c_str());
		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
