#include "pch.h"

static wchar_t const * wndClassName = L"pdf-appwnd";
static HWND appHwnd;

LRESULT __stdcall MainWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg )
	{
	case WM_DESTROY:
		::PostQuitMessage( 0 );
		return 0;

	default:
		return ::DefWindowProc( hwnd, msg, wp, lp );
	}
}

extern void LoadFile( HWND appHwnd, wchar_t const * filename );

int __stdcall WinMain( HINSTANCE inst, HINSTANCE, LPSTR, int showCmd )
{
	WNDCLASSEX wcx = { sizeof(WNDCLASSEX), CS_OWNDC | CS_DBLCLKS,
		MainWndProc, 0, 0, inst, ::LoadIcon( 0, IDI_WINLOGO ), 
		::LoadCursor( 0, IDC_ARROW ),
		(HBRUSH)(1 + COLOR_APPWORKSPACE),
		0, wndClassName, ::LoadIcon( 0, IDI_WINLOGO ) };

	::RegisterClassEx( &wcx );

	appHwnd = ::CreateWindowEx( 0, wndClassName, L"PDF Viewer", 
		WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0, inst, 0 );

	::ShowWindow( appHwnd, showCmd );
	::UpdateWindow( appHwnd );

	int argc;
	wchar_t ** argv = ::CommandLineToArgvW( ::GetCommandLine(), &argc );

	if (argc > 1)
	{
		LoadFile( appHwnd, argv[1]);
	}

	::LocalFree( argv );

	MSG msg;
	while( ::GetMessage( &msg, 0, 0, 0 ) )
	{
		::TranslateMessage( &msg );
		::DispatchMessage( &msg );
	}

	return msg.wParam;
}