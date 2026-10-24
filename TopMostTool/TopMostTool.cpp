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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// グローバル文字列を初期化しています。
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_TOPMOSTTOOL, szWindowClass, MAX_LOADSTRING);

	// アプリケーションの初期化を実行します:
	MyRegisterClass(hInstance);
	if(!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	MSG msg;
	while(GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
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
	hInst = hInstance; // グローバル変数にインスタンス処理を格納します。
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

static bool AddTrayIcon(HWND hWnd, const wchar_t*& errormsg)
{
	static HICON hIcon = nullptr;
	if(!hIcon) {
		auto hr = LoadIconMetric(hInst, MAKEINTRESOURCE(IDI_DOLPHIN), LIM_SMALL, &hIcon);
		if(FAILED(hr)) {
			errormsg = L"アイコンが読み込めません。";
			return false;
		}
	}
	auto nid = CreateNotifyIconData(hWnd, NIF_MESSAGE | NIF_TIP | NIF_ICON);
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = hIcon;
	StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), szTitle);
	if(!Shell_NotifyIcon(NIM_ADD, &nid)) {
		errormsg = L"トレイアイコンが追加できませんでした。";
		return false;
	}
	if(!Shell_NotifyIcon(NIM_SETVERSION, &nid)) {
		errormsg = L"トレイアイコンバージョンの設定に失敗しました。";
		return false;
	}
	return true;
}

static bool InitializeApp(HWND hWnd, const wchar_t*& errormsg)
{
	// register hot-key
	auto pauseRegistered = RegisterHotKey(hWnd, HOT_KEY_ID_PAUSE, MOD_NOREPEAT, VK_PAUSE);
	auto winDelRegistered = RegisterHotKey(hWnd, HOT_KEY_ID_WIN_DEL, MOD_NOREPEAT | MOD_WIN, VK_DELETE);
	if(!pauseRegistered && !winDelRegistered) {
		errormsg = L"ホットキーの登録に失敗しました。";
		return false;
	}
	// add a system tray icon
	if(!AddTrayIcon(hWnd, errormsg)) {
		return false;
	}
	if(!pauseRegistered) {
		ShowBalloon(hWnd, NIIF_WARNING, szTitle, L"Win+Del で最前面にします（Pause は登録できませんでした）");
	}
	else if(!winDelRegistered) {
		ShowBalloon(hWnd, NIIF_WARNING, szTitle, L"Pause で最前面にします（Win+Del は登録できませんでした）");
	}
	else {
		ShowBalloon(hWnd, NIIF_INFO, szTitle, L"Pause か Win+Del で最前面にします👍");
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
		const wchar_t* errmsg = nullptr;
		AddTrayIcon(hWnd, errmsg);
		return 0;
	}
	switch(message)
	{
	case WM_CREATE:
	{
		const wchar_t* errmsg = nullptr;
		SetLastError(0);
		auto ok = InitializeApp(hWnd, errmsg);
		if(!ok) {
			auto errcode = GetLastError();
			LPVOID lpMsgBuf = NULL;
			auto len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
			if(len == 0) {
				const int SIZE = 256;
				lpMsgBuf = LocalAlloc(LMEM_FIXED, SIZE);
				wcscpy_s((wchar_t*)lpMsgBuf, SIZE / sizeof(wchar_t), L"エラー");
			}
			MessageBox(hWnd, errmsg, (const wchar_t*)lpMsgBuf, MB_OK | MB_ICONERROR);
			LocalFree(lpMsgBuf);
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
			const auto* pmsg = L"スタートアップはユーザーの操作により無効化されています。タスクマネージャーのスタートアップの設定で有効にしてください。";
			MessageBox(hWnd, pmsg, szTitle, MB_OK | MB_ICONINFORMATION);
		}
		else if (lParam && state == StartupTaskState::DisabledByPolicy) {
			const auto* pmsg = L"スタートアップはポリシーにより無効化されています。";
			MessageBox(hWnd, pmsg, szTitle, MB_OK | MB_ICONINFORMATION);
		}
		else {
			const auto* pmsg = L"スタートアップ登録に失敗しました。";
			MessageBox(hWnd, pmsg, L"エラー", MB_OK | MB_ICONERROR);
		}
		break;
	}
	case WM_APP_STARTUP_DISABLED:
		if (lParam) {
			startupEnabled = FALSE;
		}
		else {
			const auto* pmsg = L"スタートアップ解除に失敗しました。";
			MessageBox(hWnd, pmsg, L"エラー", MB_OK | MB_ICONERROR);
		}
		break;
	case WM_HOTKEY:
		switch(wParam)
		{
		case HOT_KEY_ID_PAUSE:
		case HOT_KEY_ID_WIN_DEL:
		{
			auto success = false;
			auto title = L"失敗しました";
			auto msg = L"原因不明のエラーです";
			// バルーン本文は szInfo の長さで切り詰められるため、それを超えて取得する必要はない。
			WCHAR windowText[ARRAYSIZE(NOTIFYICONDATA::szInfo)] = {};
			auto hwnd = GetForegroundWindow();
			if(hwnd == NULL) {
				msg = L"現在のウインドウが特定できません";
			}
			else if(IsExcludedWindow(hwnd, hWnd)) {
				msg = L"このウインドウは対象外です";
			}
			else {
				SetLastError(0);
				auto style = GetWindowLong(hwnd, GWL_EXSTYLE);
				if(style == 0 && GetLastError() != 0) {
					msg = L"ウインドウの情報が取得できません";
				}
				else {
					auto topmost = (style & WS_EX_TOPMOST) ? true : false;
					auto ret = SetWindowPos(hwnd, topmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					if(!ret) {
						msg = L"ウインドウの情報が設定できません";
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
						title = topmost ? L"解除しました" : L"最前面にしました";
						GetWindowText(hwnd, windowText, ARRAYSIZE(windowText));
						msg = windowText;
					}
				}
			}
			ShowBalloon(hWnd, success ? NIIF_INFO : NIIF_ERROR, title, msg);
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
		WCHAR text[64];
		try {
			auto version = Package::Current().Id().Version();
			StringCchPrintf(text, ARRAYSIZE(text), L"%s, Version %u.%u.%u.%u",
				szTitle, version.Major, version.Minor, version.Build, version.Revision);
		}
		catch (...) {
			// パッケージ ID を持たない状態 (exe の直接実行) では版数を取得できない。
			StringCchPrintf(text, ARRAYSIZE(text), L"%s, Version 不明", szTitle);
		}
		SetDlgItemText(hDlg, IDC_VERSION_TEXT, text);
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
