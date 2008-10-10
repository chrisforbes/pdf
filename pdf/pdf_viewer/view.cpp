#include "pch.h"

HWND viewHwnd;
wchar_t const * viewWndClass = L"pdf-viewwnd";
PDictionary currentPage;	// todo: attach to instance of this window, so we can have >1
extern Document * doc;
int offsety = 0;
int offsety2 = 0;	// hax, fix names
int haxy = 0;

#define PAGE_GAP	5
#define DROP_SHADOW_SIZE	2

extern DoubleRect GetPageMediaBox( Document * doc, Dictionary * page );

void PaintView( HWND hwnd, HDC dc, PAINTSTRUCT const * ps )
{
	if (!currentPage)
		return;

	RECT clientRect;
	::GetClientRect( hwnd, &clientRect );

	while(offsety > PAGE_GAP)
	{
		PDictionary prevPage = doc->GetPrevPage( currentPage );
		if (!prevPage)
			break;

		currentPage = prevPage;
		DoubleRect mediaBox = GetPageMediaBox( doc, prevPage.get() );

		offsety -= (mediaBox.bottom - mediaBox.top) - PAGE_GAP;
		offsety2 -= (mediaBox.bottom - mediaBox.top) - PAGE_GAP;
	}

	int y = offsety;

	if (!doc->GetPageIndex( currentPage ))
		if (y > PAGE_GAP)
			offsety = y = PAGE_GAP;

	PDictionary page = currentPage;

	while( y < clientRect.bottom )
	{
		DoubleRect mediaBox = GetPageMediaBox( doc, page.get() );

		double width = mediaBox.right - mediaBox.left;
		int offset = (int)(clientRect.right - width) / 2;

		double height = mediaBox.bottom - mediaBox.top;

		::Rectangle( dc, offset + (int)mediaBox.left, y, offset + (int)mediaBox.right, y + (int)height );
		
		// get the page number (hack)
		size_t pageNumber = doc->GetPageIndex( page );
		char sz[64];
		sprintf( sz, "Page %u", 1 + pageNumber );

		TextOutA( dc, offset + 20, y + 20, sz, strlen(sz) );

		page = doc->GetNextPage( page );
		if (!page) break;

		RECT dropshadow_right = { offset + (int)mediaBox.right, y + DROP_SHADOW_SIZE, 
			offset + (int)mediaBox.right + DROP_SHADOW_SIZE, y + (int)height + DROP_SHADOW_SIZE };

		RECT dropshadow_bottom = { offset + (int)mediaBox.left + DROP_SHADOW_SIZE, y + (int)height, 
			offset + (int)mediaBox.right, y + (int)height + DROP_SHADOW_SIZE };

		RECT dropshadow_unfail_right = { dropshadow_right.left, dropshadow_right.top - DROP_SHADOW_SIZE, dropshadow_right.right,
			dropshadow_right.top };

		HBRUSH black = (HBRUSH)::GetStockObject(BLACK_BRUSH);

		y += height;

		RECT fail = { 0, y, clientRect.right, y + PAGE_GAP };
		FillRect( dc, &fail, (HBRUSH)(COLOR_APPWORKSPACE + 1) );

		FillRect( dc, &dropshadow_right, black );
		FillRect( dc, &dropshadow_bottom, black );
		FillRect( dc, &dropshadow_unfail_right, (HBRUSH)(COLOR_APPWORKSPACE + 1) );
		// todo: unfail bottom-left corner as soon as we do horizontal scroll as well

		y += PAGE_GAP;
	}
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
			return 0;
		}

	case WM_LBUTTONDOWN:
		{
			POINTS fail = MAKEPOINTS( lp );
			haxy = fail.y;
			offsety2 = offsety;
			return 0;
		}

	case WM_MOUSEMOVE:
		{
			if (~wp & MK_LBUTTON)
				return 0;

			POINTS fail = MAKEPOINTS( lp );
			offsety = (fail.y - haxy) + offsety2;
			::InvalidateRect( hwnd, 0, true );
			return 0;
		}

	default:
		return ::DefWindowProc( hwnd, msg, wp, lp );
	}
}

void RegisterViewClass()
{
	WNDCLASSEX wcx = { sizeof(WNDCLASSEX), CS_OWNDC | CS_DBLCLKS, ViewWndProc, 0, 0, 
		GetModuleHandle( 0 ), 0, LoadCursor( 0, IDC_HAND ), 0,
		0, viewWndClass, 0 };

	ATOM hax = RegisterClassEx( &wcx );
}

void SetCurrentPage( PDictionary page )
{
	currentPage = page;
	offsety = 0;
	offsety2 = 0;
	InvalidateRect( viewHwnd, 0, true );	// todo: more subtle
}