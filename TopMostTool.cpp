#include "stdafx.h"
#include "TopMostTool.h"

#define HOT_KEY_ID 100001

#define MAX_LOADSTRING 100

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名

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

	auto hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TOPMOSTTOOL));
	MSG msg;
	while(GetMessage(&msg, nullptr, 0, 0))
	{
		if(!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
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
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TOPMOSTTOOL));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_TOPMOSTTOOL);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
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

void PaintWindow(HWND hWnd, PAINTSTRUCT& ps, HDC hdc)
{
}

static NOTIFYICONDATA CreateNotifyIconData(HWND hWnd, UINT uFlags) {
	static const GUID NID_GUID = { 0x9b16b146, 0x7936, 0x46ff,{ 0x84, 0x95, 0xe3, 0x1, 0x35, 0xcd, 0xd3, 0xab } };
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = hWnd;
	nid.uVersion = NOTIFYICON_VERSION_4;
	nid.uFlags = uFlags | NIF_GUID;
	return nid;
}

static void ShowBalloon(HWND hWnd, DWORD dwInfoFlags, const wchar_t* title, const wchar_t* msg) {
	auto nid = CreateNotifyIconData(hWnd, NIF_INFO);
	nid.dwInfoFlags = dwInfoFlags | NIIF_NOSOUND;
	StringCchCopy(nid.szInfo, ARRAYSIZE(nid.szInfo), msg);
	StringCchCopy(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), title);
	nid.uTimeout = 1000;
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_CREATE:
	{
		//
		auto ret = RegisterHotKey(hWnd, HOT_KEY_ID, MOD_NOREPEAT, VK_PAUSE);
		if(!ret) {
			wprintf(L"%ls\n", ret ? L"OK" : L"NG");
		}
		//
		auto nid = CreateNotifyIconData(hWnd, NIF_ICON | NIF_TIP);
		LoadIconMetric(hInst, MAKEINTRESOURCE(IDI_SMALL), LIM_SMALL, &(nid.hIcon));
		StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), L"TopMostTool");
		ret = Shell_NotifyIcon(NIM_ADD, &nid);
		//ret = Shell_NotifyIcon(NIM_SETVERSION, &nid);
		ShowBalloon(hWnd, NIIF_INFO, L"TopMostTool", L"Pause キーで最前面にします");
		break;
	}
	case WM_DESTROY:
	{
		UnregisterHotKey(hWnd, HOT_KEY_ID);
		auto nid = CreateNotifyIconData(hWnd, NIF_ICON | NIF_TIP);
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
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		auto hdc = BeginPaint(hWnd, &ps);
		PaintWindow(hWnd, ps, hdc);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_HOTKEY:
		switch(wParam)
		{
		case HOT_KEY_ID:
		{
			auto success = false;
			auto title = L"失敗しました";
			auto msg = L"原因不明のエラーです";
			auto hwnd = GetForegroundWindow();
			if(hwnd == NULL) {
				msg = L"現在のウインドウが特定できません";
			}
			else {
				auto style = GetWindowLong(hwnd, GWL_EXSTYLE);
				if(style == 0) {
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
						fi.dwFlags = FLASHW_TIMERNOFG;
						FlashWindowEx(&fi);
						success = true;
						title = topmost ? L"解除しました" : L"最前面にしました";
						msg = L"";
						//
						auto len = GetWindowTextLength(hwnd);
						if(len > 0) {
							len += 1;
							auto buf = (WCHAR*)_malloca(len * sizeof(WCHAR));
							GetWindowText(hwnd, buf, len);
							msg = buf;
						}
					}
				}
			}
			ShowBalloon(hWnd, success ? NIIF_INFO : NIIF_ERROR, title, msg);
			break;
		}
		}
		break;
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
		return (INT_PTR)TRUE;
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
