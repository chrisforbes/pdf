#include "pch.h"
#include "../pdf/parse.h"
#include "resource.h"
#include "textstate.h"

HDC cacheDC = NULL;
HWND viewHwnd;
wchar_t const * viewWndClass = L"pdf-viewwnd";
PDictionary currentPage;
extern PDocument doc;
int offsety = 0;
int offsety2 = 0;	// hax, fix names
int haxy = 0;
HCURSOR openHand, closedHand, current;

#define PAGE_GAP	5
#define DROP_SHADOW_SIZE	2

extern DoubleRect GetPageMediaBox( Document * doc, Dictionary * page );
void SetCurrentPage( PDictionary page );	// declared later, this file

// font.cpp
extern void RenderSomeFail( HDC intoDC, char const * content, TextState& t, int height );

static void DrawString( String * str, int width, int height, TextState& t )
{
	if (!str) return;

	char sz[4096];

	assert( str->Length() < 4096 && "too damn long" );

	size_t unescapedLen = UnescapeString( sz, str->start, str->end );
	sz[unescapedLen] = 0;

	RenderSomeFail( cacheDC, sz, t, height );
}

static void RenderOperatorDebug( String& op, int width, int height, TextState& t )
{
	int oldMode = SetBkMode( cacheDC, OPAQUE );
	COLORREF oldColor = SetBkColor( cacheDC, 0x000000ff );
	COLORREF oldTextColor = SetTextColor( cacheDC, 0x00ffffff );

	RECT r = { t.m.v[4], height - t.m.v[5] };
	r.right = r.left + 50;
	r.bottom = r.top + 50;
	DrawTextA( cacheDC, op.start, op.Length(), &r, DT_LEFT );

	SetTextColor( cacheDC, oldTextColor );
	SetBkColor( cacheDC, oldColor );
	SetBkMode( cacheDC, oldMode );
}

extern void BindFont( PDictionary page, TextState& t, int& nopBinds );
extern HWND appHwnd;

// returns number of ops
static size_t PaintPageContent( int width, int height, PDictionary page, PStream content, TextState& t, size_t& numBinds, int& nopBinds )
{
	size_t length;
	char const * pageContent = content->GetStreamBytes( doc->xrefTable, &length );
	char const * p = pageContent;

	size_t numOperations = 0;
	std::vector<PObject> args;

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
			PName name = boost::shared_static_cast<Name>(args[0]);
			memcpy( t.fontName, name->str.start, name->str.Length() );
			t.fontNameLen = name->str.Length();
			t.fontName[ t.fontNameLen ] = 0;
			t.fontSize = ToNumber(args[1]);

			BindFont( page, t, nopBinds );
			++numBinds;
		}

		else if (op == String("BT"))
		{
			assert( args.size() == 0 );
			t.m = t.lm = Matrix();

			BindFont( page, t, nopBinds );
			++numBinds;
		}

		else if (op == String("Tm"))
		{
			// set text matrix explicit
			assert( args.size() == 6 );

			for( int i = 0; i < 6; i++ )
				t.m.v[i] = ToNumber( args[i] );

			// since lm.v[0] and lm.v[3] are the "real" font size,
			// we need to regen our font here

			t.lm = t.m;

			BindFont( page, t, nopBinds );
			++numBinds;
		}

		else if (op == String("T*"))
		{
			// next line based on leading
			assert( args.size() == 0 );
			t.lm.v[5] -= /*t.fontSize * */t.lm.v[3] * t.l;
			t.m = t.lm;
		}

		else if (op == String("TD"))
		{
			// next line, setting leading
			assert( args.size() == 2 );
			t.l = -ToNumber( args[1] );		// todo: check this
			t.lm.v[4] += /*t.fontSize * */t.lm.v[0] * ToNumber( args[0] );
			t.lm.v[5] += /*t.fontSize * */t.lm.v[3] * ToNumber( args[1] );
			t.m = t.lm;
		}

		else if (op == String("Td"))
		{
			// next line with explicit positioning, preserve leading
			assert( args.size() == 2 );
			t.lm.v[4] += /*t.fontSize * */t.lm.v[0] * ToNumber( args[0] );
			t.lm.v[5] += /*t.fontSize * */t.lm.v[3] * ToNumber( args[1] );
			t.m = t.lm;
		}

		else if (op == String("'"))
		{
			RenderOperatorDebug( op, width, height, t );

			assert( args.size() == 1 );
			t.lm.v[5] -= /*t.fontSize * */t.lm.v[3] * t.l;
			t.m = t.lm;
			DrawString( (String *)args[0].get(), width, height, t );
		}

		else if (op == String("\""))
		{
			RenderOperatorDebug( op, width, height, t );

			assert( args.size() == 3 );
			t.w = ToNumber( args[0] );
			t.c = ToNumber( args[1] );
			t.lm.v[5] -= /*t.fontSize * */t.lm.v[3] * t.l;
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
					t.m.v[4] -= t.EffectiveFontWidth() * k / 1000;
				}
			}
		}
/*		else if (op == String("BDC"))
			{}	// todo: begin drawing context
		else if (op == String("gs"))
			{}	// todo: change graphics state
		else if (op == String("g"))
			{}	// todo: ???
		else if (op == String("ET"))
			{}	// todo: free any text state?
		else if (op == String("EMC"))
			{}	// todo: ???
		else if (op == String("i"))
			{}	// todo: graphics op
		else if (op == String("re"))
			{}	// todo: graphics op		*/

/*		else
			DebugBreak();	*/

		++numOperations;
		while( *p == '\r' || *p == '\n' )
			++p;
	}

	return numOperations;
}

static void PaintPage( int width, int height, PDictionary page )
{
	::Rectangle( cacheDC, 0, 0, width, height );
	size_t numOperations = 0;
	size_t numBinds = 0;
	int nopBinds = 0;

	TextState t;

	DWORD time = GetTickCount();
	
	PStream content = page->Get<Stream>( "Contents", doc->xrefTable );
	if (content)
		numOperations += PaintPageContent( width, height, page, content, t, numBinds, nopBinds );

	PArray array = page->Get<Array>( "Contents", doc->xrefTable );
	if (array)
	{
		for( std::vector<PObject>::iterator it = array->elements.begin();
			it != array->elements.end(); it++ )
		{
			PStream stream = Object::ResolveIndirect_<Stream>( *it, doc->xrefTable );
			if (stream)
				numOperations += PaintPageContent( width, height, page, stream, t, numBinds, nopBinds );
		}
	}

	time = GetTickCount() - time;

	size_t n = doc->GetPageIndex( page );
	assert( doc->GetPage( n ) == page );

	HFONT oldFont = (HFONT)SelectObject( cacheDC, (HFONT)GetStockObject( DEFAULT_GUI_FONT ) );
	DeleteObject( oldFont );

	// get the page number (hack)
	size_t pageNumber = doc->GetPageIndex( page );
	char sz[64];
	sprintf( sz, "Page %u", pageNumber + 1 );
	TextOutA( cacheDC, 10, 10, sz, strlen(sz) );

	char fail[128];
	sprintf( fail, "len: %u ops: %u t: %u ms binds: %d/%u", 0u, numOperations, time, numBinds - nopBinds, numBinds);
	TextOutA( cacheDC, 200, 10, fail, strlen(fail) );
}

static std::vector<std::pair<size_t, HBITMAP>> cachedPages;
static const int PAGE_CACHE_MAX_SIZE = 4;

static void EvictCacheItem( std::pair<size_t, HBITMAP> item )
{
	DeleteObject( cachedPages[0].second );
	PDictionary page = doc->GetPage( cachedPages[0].first );
	
	PStream stream = page->Get<Stream>( "Contents", doc->xrefTable );
	if (stream)
		stream->EvictCache();

	PArray array = page->Get<Array>( "Contents", doc->xrefTable );
	if (array)
	{
		for( std::vector<PObject>::iterator it = array->elements.begin();
			it != array->elements.end(); it++ )
		{
			PStream stream = Object::ResolveIndirect_<Stream>( *it, doc->xrefTable );
			if (stream)
				stream->EvictCache();
		}
	}
}

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
			EvictCacheItem( cachedPages[0] );
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
		DoubleRect mediaBox = GetPageMediaBox( doc.get(), prevPage.get() );

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
		DoubleRect mediaBox = GetPageMediaBox( doc.get(), currentPage.get() );
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
		DoubleRect mediaBox = GetPageMediaBox( doc.get(), failPage.get() );
		PDictionary nextPage = doc->GetNextPage( failPage );

		if (!nextPage)
		{
			if (foo + mediaBox.height() + PAGE_GAP < clientRect.bottom)
			{
				int d = (int)(clientRect.bottom - (foo + mediaBox.height() + PAGE_GAP));
				offsety += d;
				offsety2 += d;
			}
			return;
		}

		foo += (int)(mediaBox.height() + PAGE_GAP);

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
		DoubleRect mediaBox = GetPageMediaBox( doc.get(), page.get() );

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

		y += (int)height;

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

wchar_t gotoPage[10];
INT_PTR __stdcall GotoPageDialogProc( HWND diagHwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			wchar_t range[10];
			wsprintf(range, L"1-%d", doc->GetPageCount());
			SetDlgItemText(diagHwnd, IDC_VALIDPAGES, range);
			SetDlgItemText(diagHwnd, IDC_PAGENUMBER, L"");
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wp))
		{
		case IDOK:
			if (!GetDlgItemText(diagHwnd, IDC_PAGENUMBER, gotoPage, 10))
				*gotoPage = 0;
		case IDCANCEL:
			EndDialog(diagHwnd, wp);
			break;
		}
		break;
	}
	return FALSE;
}

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

	case WM_KEYDOWN:
		{
			switch(wp)
			{
			case VK_F3:
				{
					SetWindowText( appHwnd, L"PDF Viewer - Running Test - Press <ESC> to cancel" );
					PDictionary page = doc->GetPage(0);
					while( page )
					{
						SetCurrentPage( page );
						UpdateWindow( hwnd );

						page = doc->GetNextPage( page );

						if (GetAsyncKeyState( VK_ESCAPE ) != 0)
						{
							SetWindowText( appHwnd, L"PDF Viewer - Test Canceled" );
							return 0;
						}
					}

					SetWindowText( appHwnd, L"PDF Viewer - Test complete" );
				}
				break;
			case VK_F4:
				{
					HINSTANCE inst = GetModuleHandle(0);
					if (DialogBox(inst, MAKEINTRESOURCE(IDD_GOTOPAGE), hwnd, GotoPageDialogProc)
						== IDOK)
					{
						if (*gotoPage != 0)
							SetCurrentPage(doc->GetPage(_wtoi(gotoPage) - 1));
					}
				}
				break;
			case VK_UP:
				offsety += WHEEL_SCROLL_PIXELS;
				::InvalidateRect( hwnd, 0, true );
				break;
			case VK_DOWN:
				offsety -= WHEEL_SCROLL_PIXELS;
				::InvalidateRect( hwnd, 0, true );
				break;
			case VK_PRIOR:
				offsety += WHEEL_SCROLL_PIXELS * 10;
				::InvalidateRect( hwnd, 0, true );
				break;
			case VK_NEXT:
				offsety -= WHEEL_SCROLL_PIXELS * 10;
				::InvalidateRect( hwnd, 0, true );
				break;
			}

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