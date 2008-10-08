#include "pch.h"
#include "commctrl.h"

#include <atlbase.h>

#pragma comment( lib, "comctl32.lib" )

static wchar_t const * wndClassName = L"pdf-appwnd";
static HWND appHwnd, outlineHwnd;

RECT outlineRect = { 0, 0, 260, 0 };

void AdjustControlPlacement( HWND parent, HWND child, WINDOWPOS * p, RECT controlAlignment )
{
	// DAMN this thing is crazy

	RECT wndRect, clientRect, frameRect;
	::GetWindowRect( parent, &wndRect );
	::GetClientRect( parent, &clientRect );

	if (p)
	{
		clientRect.right += p->cx - (wndRect.right - wndRect.left);
		clientRect.bottom += p->cy - (wndRect.bottom - wndRect.top);
	}

	if (controlAlignment.left < 0) controlAlignment.left = clientRect.right - clientRect.left - controlAlignment.left;
	if (controlAlignment.right <= 0) controlAlignment.left = clientRect.right - clientRect.left - controlAlignment.right;

	if (controlAlignment.top < 0) controlAlignment.top = clientRect.bottom - clientRect.top - controlAlignment.top;
	if (controlAlignment.bottom <= 0) controlAlignment.bottom = clientRect.bottom - clientRect.top - controlAlignment.bottom;

	::SetWindowPos( child, 0, controlAlignment.left, controlAlignment.top, 
		controlAlignment.right - controlAlignment.left,
		controlAlignment.bottom - controlAlignment.top,
		SWP_NOZORDER );
}

LRESULT __stdcall MainWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg )
	{
	case WM_DESTROY:
		::PostQuitMessage( 0 );
		return 0;

	case WM_WINDOWPOSCHANGING:
		{
			::AdjustControlPlacement( appHwnd, outlineHwnd, (WINDOWPOS *)lp, outlineRect );
			return ::DefWindowProc( hwnd, msg, wp, lp );
		}

	default:
		return ::DefWindowProc( hwnd, msg, wp, lp );
	}
}

extern void LoadFile( HWND appHwnd, wchar_t const * filename );

HTREEITEM AddOutlineItem( char const * start, size_t len, bool hasChildren, HTREEITEM parent )
{
	USES_CONVERSION;
	char temp[512];
	memcpy( temp, start, len );
	temp[len] = 0;

	TV_INSERTSTRUCT is;
	::memset( &is, 0, sizeof( TV_INSERTSTRUCT ) );
	is.hParent = parent;
	is.hInsertAfter = TVI_LAST;
	is.item.mask = TVIF_TEXT | TVIF_CHILDREN;
	is.item.cChildren = hasChildren ? 1 : 0;
	is.item.pszText = A2W( temp );
	is.item.cchTextMax = len;

	return TreeView_InsertItem( outlineHwnd, &is );
}

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
		LoadFile( appHwnd, argv[1]);
	}

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