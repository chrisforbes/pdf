#include "pch.h"
#include "../pdf/parse.h"

HWND viewHwnd;
wchar_t const * viewWndClass = L"pdf-viewwnd";
PDictionary currentPage;
extern Document * doc;
int offsety = 0;
int offsety2 = 0;	// hax, fix names
int haxy = 0;

#define PAGE_GAP	5
#define DROP_SHADOW_SIZE	2

extern DoubleRect GetPageMediaBox( Document * doc, Dictionary * page );

void PaintPage( HWND hwnd, HDC dc, PAINTSTRUCT const * ps, DoubleRect const& rect, int offset, PDictionary page, int y )
{
	PStream content = page->Get<Stream>( "Contents", doc->xrefTable );
	if (!content)
		return;			// multiple content streams

	size_t length;
	char const * pageContent = content->GetStreamBytes( doc->xrefTable, &length );
	char const * p = pageContent;

	size_t numOperations = 0;
	std::vector<PObject> args;

	DWORD t = GetTickCount();
	while( p < pageContent + length )
	{
		args.clear();
		String op = ParseContent( p, args );

		if (op == String("Tm"))
		{
			// text placement
			assert( args.size() == 6 );

			double _x = ToNumber( args[4] );
			double _y = ToNumber( args[5] );

			::Rectangle( dc, (int)_x - 10 + offset, (int)(y + rect.bottom - _y - 10) ,
				(int)_x + 10 + offset, (int)(y + rect.bottom - _y + 10) );
		}

		++numOperations;
	}

	t = GetTickCount() - t;

	size_t n = doc->GetPageIndex( page );
	assert( doc->GetPage( n ) == page );

	char fail[128];
	sprintf( fail, "len: %u ops: %u t: %u ms", length, numOperations, t );
	TextOutA( dc, offset + 200, y + 10, fail, strlen(fail) );
}

void ClampToStartOfDocument()
{
	while(offsety > PAGE_GAP)
	{
		PDictionary prevPage = doc->GetPrevPage( currentPage );
		if (!prevPage)
			break;

		currentPage = prevPage;
		DoubleRect mediaBox = GetPageMediaBox( doc, prevPage.get() );

		offsety -= (mediaBox.bottom - mediaBox.top) + PAGE_GAP;
		offsety2 -= (mediaBox.bottom - mediaBox.top) + PAGE_GAP;
	}

	if (!doc->GetPageIndex( currentPage ))
		if (offsety > PAGE_GAP)
			offsety = PAGE_GAP;
}

void ClampToEndOfDocument()
{
	// a problem for another day
}

void PaintView( HWND hwnd, HDC dc, PAINTSTRUCT const * ps )
{
	if (!currentPage)
		return;

	RECT clientRect;
	::GetClientRect( hwnd, &clientRect );

	ClampToStartOfDocument();
	ClampToEndOfDocument();

	int y = offsety;

	// erase any space above top of page
	RECT fail = { 0, 0, clientRect.right, offsety };
	FillRect( dc, &fail, (HBRUSH)(COLOR_APPWORKSPACE + 1) );

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
		sprintf( sz, "Page %u", pageNumber + 1 );

		TextOutA( dc, offset + 10, y + 10, sz, strlen(sz) );

		PaintPage( hwnd, dc, ps, mediaBox, offset, page, y );

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