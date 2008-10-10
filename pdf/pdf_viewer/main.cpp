#include "pch.h"

static wchar_t const * wndClassName = L"pdf-appwnd";
static HWND appHwnd;
extern HWND outlineHwnd;
extern HWND viewHwnd;
Document * doc = 0;
extern wchar_t const * viewWndClass;

extern void RegisterViewClass();
extern void SetCurrentPage( PDictionary page );

RECT outlineRect = { 0, 0, 260, 0 };
RECT viewRect = { 260, 0, 0, 0 };

extern void AdjustControlPlacement( HWND parent, HWND child, WINDOWPOS * p, RECT controlAlignment );
extern void NavigateToPage( HWND appHwnd, Document * doc, NMTREEVIEW * info );

LRESULT __stdcall MainWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg )
	{
	case WM_DESTROY:
		::PostQuitMessage( 0 );
		return 0;

	case WM_WINDOWPOSCHANGING:
		{
			WINDOWPOS * pos = (WINDOWPOS *)lp;
			if ((pos->flags & SWP_NOSIZE) && (~pos->flags & SWP_SHOWWINDOW))
				return ::DefWindowProc( hwnd, msg, wp, lp );

			::AdjustControlPlacement( appHwnd, outlineHwnd, pos, outlineRect );
			::AdjustControlPlacement( appHwnd, viewHwnd, pos, viewRect );
			return ::DefWindowProc( hwnd, msg, wp, lp );
		}

	case WM_NOTIFY:
		{
			NMHDR * hdr = (NMHDR *) lp;
			if (hdr->hwndFrom == outlineHwnd)
			{
				if (hdr->code == TVN_SELCHANGED)
					::NavigateToPage( appHwnd, doc, (NMTREEVIEW *) hdr );
			}
			return ::DefWindowProc( hwnd, msg, wp, lp );
		}

	default:
		return ::DefWindowProc( hwnd, msg, wp, lp );
	}
}

extern Document * LoadFile( HWND appHwnd, wchar_t const * filename );
extern void DumpPage( Dictionary * start, const XrefTable & objmap );

int __stdcall WinMain( HINSTANCE inst, HINSTANCE, LPSTR, int showCmd )
{
	DWORD t = GetTickCount();

	::InitCommonControls();

	WNDCLASSEX wcx = { sizeof(WNDCLASSEX), CS_OWNDC | CS_DBLCLKS,
		MainWndProc, 0, 0, inst, ::LoadIcon( 0, IDI_WINLOGO ), 
		::LoadCursor( 0, IDC_ARROW ),
		(HBRUSH)(1 + COLOR_APPWORKSPACE),
		0, wndClassName, ::LoadIcon( 0, IDI_WINLOGO ) };

	::RegisterClassEx( &wcx );
	::RegisterViewClass();

	appHwnd = ::CreateWindowEx( 0, wndClassName, L"PDF Viewer", 
		WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0, inst, 0 );

	::ShowWindow( appHwnd, SW_SHOWMAXIMIZED );
	::UpdateWindow( appHwnd );

	outlineHwnd = ::CreateWindowEx( WS_EX_CLIENTEDGE, WC_TREEVIEW, L"", 
		WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_CHILD |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS, 0, 0, 1,1, appHwnd, 
		0, inst, 0 );

	viewHwnd = ::CreateWindowEx( WS_EX_CLIENTEDGE, viewWndClass, L"",
		WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, appHwnd, 
		0, inst, 0 );

	::AdjustControlPlacement( appHwnd, outlineHwnd, 0, outlineRect );
	::AdjustControlPlacement( appHwnd, viewHwnd, 0, viewRect );
	::UpdateWindow( outlineHwnd );
	::UpdateWindow( appHwnd );

	int argc;
	wchar_t ** argv = ::CommandLineToArgvW( ::GetCommandLine(), &argc );

	if (argc > 1)
	{
		doc = LoadFile( appHwnd, argv[1]);
		if (doc)
		{
			BuildOutline( doc->outlineRoot.get(), TVI_ROOT, doc->xrefTable );
			SetCurrentPage( doc->GetPage( 0 ) );
		}
	}

	::LocalFree( argv );


	::AdjustControlPlacement( appHwnd, outlineHwnd, 0, outlineRect );
	::AdjustControlPlacement( appHwnd, viewHwnd, 0, viewRect );
	::InvalidateRect( appHwnd, 0, true );
	::UpdateWindow( appHwnd );		// force painting of UI NOW

	char sz[64];
	sprintf( sz, "all: %u ms", GetTickCount() - t );
	MessageBoxA( 0, sz, "fail", 0 );

	MSG msg;
	while( ::GetMessage( &msg, 0, 0, 0 ) )
	{
		::TranslateMessage( &msg );
		::DispatchMessage( &msg );
	}

	return msg.wParam;
}