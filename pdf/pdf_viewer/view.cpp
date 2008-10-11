#include "pch.h"
#include "../pdf/parse.h"
#include "resource.h"

HDC cacheDC = NULL;
HWND viewHwnd;
wchar_t const * viewWndClass = L"pdf-viewwnd";
PDictionary currentPage;
extern Document * doc;
int offsety = 0;
int offsety2 = 0;	// hax, fix names
int haxy = 0;
HCURSOR openHand, closedHand, current;

#define PAGE_GAP	5
#define DROP_SHADOW_SIZE	2

extern DoubleRect GetPageMediaBox( Document * doc, Dictionary * page );

struct Matrix
{
	double v[6];

	Matrix()
	{
		v[0] = 1; v[1] = 0; v[2] = 0; v[3] = 0; v[4] = 1; v[5] = 0;
	}
};

struct TextState
{
	double c, w, h, l, rise;
	int mode;
	double fontSize;

	Matrix m, lm;

	TextState()
		: c(0), w(0), h(100), l(0), rise(0), mode(0), fontSize(0), m(), lm()
	{
	}
};

static size_t UnescapeString( char * dest, char const * src, char const * srcend )
{
	char * destStart = dest;

	while( src < srcend )
	{
		if (*src == '\\')
		{
			++src;
			switch( *src )
			{
			case 'n':
				*dest++ = '\n';
				break;
			case 'r':
				*dest++ = '\r';
				break;
			case 't':
				*dest++ = '\t';
				break;
			case 'b':
				dest--;
			case 'f':
				*dest++ = '\f';
				break;
			case '(':
				*dest++ = '(';
				break;
			case ')':
				*dest++ = ')';
				break;
			case '\\':
				*dest++ = '\\';
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				{
					char temp[3], *pt = temp;

					char * p = (char *)src;
					while( src < p + 3 )
					{
						*pt++ = *src++;
						if (*src < '0' || *src > '7')
							break;
					}

					*dest++ = (char)strtol( temp, &p, 8 );
					--src;
				}
				break;
			default:
				*dest++ = *src;
			}
		}
		else
			*dest++ = *src;

		++src;
	}

	return dest - destStart;
}

static void DrawString( String * str, int width, int height, TextState& t )
{
	SIZE size;
	char sz[4096];
	size_t unescapedLen = UnescapeString( sz, str->start, str->end );
	sz[unescapedLen] = 0;

	GetTextExtentPoint32A( cacheDC, sz, unescapedLen, &size );

	TextOutA( cacheDC, (int)t.m.v[4], height - (int)t.m.v[5], sz, unescapedLen );
	t.m.v[4] += size.cx;
}

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

	TextState t;

	// select in a bogus font
	SelectObject( cacheDC, (HFONT)GetStockObject( DEFAULT_GUI_FONT ) );
	int oldMode = SetBkMode( cacheDC, TRANSPARENT );

	DWORD time = GetTickCount();
	while( p < pageContent + length )
	{
		args.clear();
		String op = ParseContent( p, pageContent + length, args );

		if (op == String("Tc"))
		{
			assert( args.size() == 1 );
			t.c = ToNumber( args[0] );
		}

		else if (op == String("Tw"))
		{
			assert( args.size() == 1 );
			t.w = ToNumber( args[0] );
		}

		else if (op == String("Tz"))
		{
			assert( args.size() == 1 );
			t.h = 100 + ToNumber( args[0] );
		}

		else if (op == String("TL"))
		{
			assert( args.size() == 1 );
			t.l = ToNumber( args[0] );
		}

		else if (op == String("Ts"))
		{
			assert( args.size() == 1 );
			t.rise = ToNumber( args[0] );
		}

		else if (op == String("Tr"))
		{
			assert( args.size() == 1 );
			t.mode = (int)ToNumber( args[0] );
		}

		else if (op == String("Tf"))
		{
			assert( args.size() == 2 );
			// todo: font name is args[0]
			t.fontSize = ToNumber( args[1] );
		}

		else if (op == String("Tm"))
		{
			// set text matrix explicit
			assert( args.size() == 6 );

			for( int i = 0; i < 6; i++ )
				t.m.v[i] = ToNumber( args[i] );

			t.lm = t.m;

	/*		::Rectangle( cacheDC, (int)t.m.v[4] - 10, (int)(height - t.m.v[5] - 10) ,
				(int)t.m.v[4] + 10, (int)(height - t.m.v[5] + 10) );	*/
		}

		else if (op == String("T*"))
		{
			// next line based on leading
			assert( args.size() == 0 );
			t.lm.v[5] += t.l;
			t.m = t.lm;
		}

		else if (op == String("TD"))
		{
			// next line, setting leading
			assert( args.size() == 2 );
			t.l = -ToNumber( args[1] );		// todo: check this
			t.lm.v[4] += ToNumber( args[0] );
			t.lm.v[5] += ToNumber( args[1] );
			t.m = t.lm;
		}

		else if (op == String("Td"))
		{
			// next line with explicit positioning, preserve leading
			assert( args.size() == 2 );
			t.lm.v[4] += ToNumber( args[0] );
			t.lm.v[5] += ToNumber( args[1] );
			t.m = t.lm;
		}

		else if (op == String("'"))
		{
			assert( args.size() == 1 );
			t.lm.v[5] += t.l;
			t.m = t.lm;
			DrawString( (String *)args[0].get(), width, height, t );
		}

		else if (op == String("\""))
		{
			assert( args.size() == 3 );
			t.w = ToNumber( args[0] );
			t.c = ToNumber( args[1] );
			t.lm.v[5] += t.l;
			t.m = t.lm;
			DrawString( (String *)args[2].get(), width, height, t );
		}

		else if (op == String("Tj"))
		{
			assert( args.size() == 1 );
			DrawString( (String *)args[0].get(), width, height, t );
		}

		else if (op == String("TJ"))
		{
			assert( args.size() == 1 );
			Array * arr = (Array *)args[0].get();

			std::vector<PObject>::const_iterator it;
			for( it = arr->elements.begin(); it != arr->elements.end(); it++ )
			{
				if ((*it)->Type() == ObjectType::String)
					DrawString( (String *)it->get(), width, height, t );
				else
				{
					// todo: non-horizontal writing modes
					double k = ToNumber( *it );
					t.m.v[4] -= k / 1000;
				}
			}
		}

		++numOperations;
	}

	time = GetTickCount() - time;

	size_t n = doc->GetPageIndex( page );
	assert( doc->GetPage( n ) == page );

	// get the page number (hack)
	size_t pageNumber = doc->GetPageIndex( page );
	char sz[64];
	sprintf( sz, "Page %u", pageNumber + 1 );
	TextOutA( cacheDC, 10, 10, sz, strlen(sz) );

	char fail[128];
	sprintf( fail, "len: %u ops: %u t: %u ms", length, numOperations, time );
	TextOutA( cacheDC, 200, 10, fail, strlen(fail) );

	SetBkMode( cacheDC, oldMode );
}

static std::vector<std::pair<size_t, HBITMAP>> cachedPages;
static const int PAGE_CACHE_MAX_SIZE = 4;

void PaintPageFromCache( HDC dc, DoubleRect const& rect, int offset, PDictionary page, int y )
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
			doc->GetPage( cachedPages[0].first )->Get<Stream>( "Contents", doc->xrefTable )->EvictCache();
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

		double width = mediaBox.width();
		int offset = (int)(clientRect.right - width) / 2;

		double height = mediaBox.height();

		PaintPageFromCache( dc, mediaBox, offset, page, y );

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

void PaintAll( HWND hwnd, HDC dc )
{
	if( !doc )
		return;
	PDictionary page = doc->GetPage( 0 );
	while( page )
	{
		DoubleRect mediaBox = GetPageMediaBox( doc, page.get() );
		PaintPageFromCache( dc, mediaBox, 0, page, 0 );

		page = doc->GetNextPage( page );
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
			//PaintAll( hwnd, dc );
			::ReleaseDC( hwnd, dc );
			return 0;
		}

	case WM_LBUTTONDOWN:
		{
			SetFocus( hwnd );
			POINTS fail = MAKEPOINTS( lp );
			haxy = fail.y;
			offsety2 = offsety;
			SetCursor( current = closedHand );
			return 0;
		}

	case WM_LBUTTONUP:
		{
			SetCursor( current = openHand );
			return 0;
		}

	case WM_SETCURSOR:
		{
			SetCursor( current );
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
	openHand = ::LoadCursor( GetModuleHandle(0), MAKEINTRESOURCE( IDC_OPENHAND ) );
	closedHand = ::LoadCursor( GetModuleHandle(0), MAKEINTRESOURCE( IDC_CLOSEDHAND ) );
	current = openHand;
	WNDCLASSEX wcx = { sizeof(WNDCLASSEX), CS_OWNDC | CS_HREDRAW | CS_VREDRAW, ViewWndProc, 0, 0, 
		GetModuleHandle( 0 ), 0, current, 0,
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