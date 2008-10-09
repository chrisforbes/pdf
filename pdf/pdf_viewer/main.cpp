#include "pch.h"

static wchar_t const * wndClassName = L"pdf-appwnd";
static HWND appHwnd;
extern HWND outlineHwnd;
static Document * doc = 0;

RECT outlineRect = { 0, 0, 260, 0 };

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

int __stdcall WinMain( HINSTANCE inst, HINSTANCE, LPSTR, int showCmd )
{
	::InitCommonControls();

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

	outlineHwnd = ::CreateWindowEx( WS_EX_CLIENTEDGE, WC_TREEVIEW, L"", 
		WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_CHILD |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS, 0, 0, 1,1, appHwnd, 
		0, inst, 0 );

	::AdjustControlPlacement( appHwnd, outlineHwnd, 0, outlineRect );
	::UpdateWindow( outlineHwnd );
	::UpdateWindow( appHwnd );

	int argc;
	wchar_t ** argv = ::CommandLineToArgvW( ::GetCommandLine(), &argc );

	if (argc > 1)
	{
		doc = LoadFile( appHwnd, argv[1]);
		if (doc)
			BuildOutline( doc->outlineRoot.get(), TVI_ROOT, doc->xrefTable );
	}

	PDictionary page0 = doc->GetPage(0);
	PDictionary page1 = doc->GetPage(1);

	::LocalFree( argv );

	::ShowWindow( outlineHwnd, SW_SHOW );
	::AdjustControlPlacement( appHwnd, outlineHwnd, 0, outlineRect );
	::InvalidateRect( appHwnd, 0, true );
	::UpdateWindow( appHwnd );		// force painting of UI NOW

	MSG msg;
	while( ::GetMessage( &msg, 0, 0, 0 ) )
	{
		::TranslateMessage( &msg );
		::DispatchMessage( &msg );
	}

	return msg.wParam;
}