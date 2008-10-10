#include "pch.h"

HWND viewHwnd;
wchar_t const * viewWndClass = L"pdf-viewwnd";
PDictionary currentPage;	// todo: attach to instance of this window, so we can have >1
extern Document * doc;

extern DoubleRect GetPageMediaBox( Document * doc, Dictionary * page );

void PaintView( HWND hwnd, HDC dc, PAINTSTRUCT const * ps )
{
	if (!currentPage)
		return;

	RECT clientRect;
	::GetClientRect( hwnd, &clientRect );

	DoubleRect mediaBox = GetPageMediaBox( doc, currentPage.get() );

	double width = mediaBox.right - mediaBox.left;
	int offset = (int)(clientRect.right - width) / 2;

	::Rectangle( dc, offset + (int)mediaBox.left, (int)mediaBox.top, offset + (int)mediaBox.right, (int)mediaBox.bottom );
	
	// get the page number (hack)
	size_t pageNumber = doc->GetPageIndex( currentPage );
	char sz[64];
	sprintf( sz, "Page %u", 1 + pageNumber );

	TextOutA( dc, offset + 20, 20, sz, strlen(sz) );
}

LRESULT __stdcall ViewWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg )
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint( hwnd, &ps );

			PaintView( hwnd, dc, &ps );

			::ReleaseDC( hwnd, dc );
		}
	default:
		return ::DefWindowProc( hwnd, msg, wp, lp );
	}
}

void RegisterViewClass()
{
	WNDCLASSEX wcx = { sizeof(WNDCLASSEX), CS_OWNDC | CS_DBLCLKS, ViewWndProc, 0, 0, 
		GetModuleHandle( 0 ), 0, LoadCursor( 0, IDC_HAND ), (HBRUSH)( COLOR_APPWORKSPACE + 1 ),
		0, viewWndClass, 0 };

	ATOM hax = RegisterClassEx( &wcx );
}

void SetCurrentPage( PDictionary page )
{
	currentPage = page;
	InvalidateRect( viewHwnd, 0, true );	// todo: more subtle
}