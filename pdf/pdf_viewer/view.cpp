#include "pch.h"
#include "../pdf/parse.h"

HDC cacheDC = NULL;
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

void PaintPage( int width, int height, PDictionary page )
{
	::Rectangle( cacheDC, 0, 0, width, height );
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
		String op = ParseContent( p, pageContent + length, args );

		if (op == String("Tm"))
		{
			// text placement
			assert( args.size() == 6 );

			double _x = ToNumber( args[4] );
			double _y = ToNumber( args[5] );

			::Rectangle( cacheDC, (int)_x - 10, (int)(height - _y - 10) ,
				(int)_x + 10, (int)(height - _y + 10) );
		}

		++numOperations;
	}

	t = GetTickCount() - t;

	size_t n = doc->GetPageIndex( page );
	assert( doc->GetPage( n ) == page );

	// get the page number (hack)
	size_t pageNumber = doc->GetPageIndex( page );
	char sz[64];
	sprintf( sz, "Page %u", pageNumber + 1 );
	TextOutA( cacheDC, 10, 10, sz, strlen(sz) );

	char fail[128];
	sprintf( fail, "len: %u ops: %u t: %u ms", length, numOperations, t );
	TextOutA( cacheDC, 200, 10, fail, strlen(fail) );
}

static std::vector<std::pair<size_t, HBITMAP>> cachedPages;
static const int PAGE_CACHE_MAX_SIZE = 4;

void PaintPageFromCache( HWND hwnd, HDC dc, DoubleRect const& rect, int offset, PDictionary page, int y )
{
	HBITMAP cacheBitmap = NULL;
	size_t pageNum = doc->GetPageIndex( page );
	for( size_t i = 0 ; i < cachedPages.size() ; i++ )
	{
		if( cachedPages[i].first == pageNum )
		{
			cacheBitmap = cachedPages[i].second;
			cachedPages.erase( cachedPages.begin() + i );
			cachedPages.push_back( std::pair<size_t, HBITMAP>( pageNum, cacheBitmap ) );
			break;
		}
	}

	if( !cacheBitmap )
	{
		cacheBitmap = CreateCompatibleBitmap( dc, (int)rect.width(), (int)rect.height() );
		SelectObject( cacheDC, cacheBitmap );
		cachedPages.push_back( std::pair<size_t, HBITMAP>( pageNum, cacheBitmap ) );

		if( cachedPages.size() >= PAGE_CACHE_MAX_SIZE )
		{
			DeleteObject( cachedPages[0].second );
			cachedPages.erase( cachedPages.begin() );
		}
		PaintPage( (int)rect.width(), (int)rect.height(), page );
	}
	else
		SelectObject( cacheDC, cacheBitmap );

	BitBlt( dc, offset, y, (int)rect.width(), (int)rect.height(), cacheDC, 0, 0, SRCCOPY );
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

		offsety -= (int)mediaBox.height() + PAGE_GAP;
		offsety2 -= (int)mediaBox.height() + PAGE_GAP;
	}

	if (!doc->GetPageIndex( currentPage ))
		if (offsety > PAGE_GAP)
			offsety = PAGE_GAP;
}

void ClampToEndOfDocument()
{
	while( true )
	{
		DoubleRect mediaBox = GetPageMediaBox( doc, currentPage.get() );
		if( offsety >= -mediaBox.height() )
			break;

		PDictionary nextPage = doc->GetNextPage( currentPage );
		if (!nextPage)
			break;

		offsety += (int)mediaBox.height() + PAGE_GAP;
		offsety2 +=(int)mediaBox.height() + PAGE_GAP;

		currentPage = nextPage;
	}

	// a blatant hack

	PDictionary failPage = currentPage;
	RECT clientRect;
	GetClientRect( viewHwnd, &clientRect );
	int foo = offsety;

	while( true )
	{
		DoubleRect mediaBox = GetPageMediaBox( doc, failPage.get() );
		PDictionary nextPage = doc->GetNextPage( failPage );

		if (!nextPage)
		{
			if (foo + mediaBox.height() + PAGE_GAP < clientRect.bottom)
			{
				int d = clientRect.bottom - (foo + mediaBox.height() + PAGE_GAP);
				offsety += d;
				offsety2 += d;
			}
			return;
		}

		foo += mediaBox.height() + PAGE_GAP;

		if (foo > clientRect.bottom)
			return;
		failPage = nextPage;
	}
}

void PaintView( HWND hwnd, HDC dc, PAINTSTRUCT const * ps )
{
	if (!currentPage)
		return;

	RECT clientRect;
	::GetClientRect( hwnd, &clientRect );

	ClampToEndOfDocument();				// ordering matters here!
	ClampToStartOfDocument();

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

		PaintPageFromCache( hwnd, dc, mediaBox, offset, page, y );

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

		page = doc->GetNextPage( page );
		if (!page) break;
	}
}

static const int WHEEL_SCROLL_PIXELS = 40;

LRESULT __stdcall ViewWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg )
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint( hwnd, &ps );
			if( !cacheDC )
				cacheDC = CreateCompatibleDC( dc );
			PaintView( hwnd, dc, &ps );
			::ReleaseDC( hwnd, dc );
			return 0;
		}

	case WM_LBUTTONDOWN:
		{
			SetFocus( hwnd );
			POINTS fail = MAKEPOINTS( lp );
			haxy = fail.y;
			offsety2 = offsety;
			return 0;
		}

	case WM_MOUSEWHEEL:
		{
			int scrollAmount = ((int)wp) >> 16;
			UINT uScroll;
			if( !SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, &uScroll, 0 ) )
				uScroll = 3;
			if( !uScroll )
				return 0;

			offsety += WHEEL_SCROLL_PIXELS * (int)uScroll * scrollAmount / WHEEL_DELTA;
			::InvalidateRect( hwnd, 0, true );

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