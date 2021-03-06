#include "pch.h"
#include "../pdf/parse.h"
#include "contentStreamElement.h"
#include "resource.h"
#include "textstate.h"

HDC cacheDC = NULL;
HDC tmpDC = NULL;
HWND viewHwnd;
wchar_t const * viewWndClass = L"pdf-viewwnd";
PDictionary currentPage;
extern PDocument doc;
int offsety = 0;
int offsety2 = 0;	// hax, fix names
int haxy = 0;

int offsetx = 0;
int offsetx2 = 0;
int haxx = 0;

HCURSOR openHand, closedHand, current;

#define PAGE_GAP	5
#define DROP_SHADOW_SIZE	2

int zoom = 108;
#define ZOOM(k) ((k) * zoom / 72)

extern DoubleRect GetPageMediaBox( Document * doc, Dictionary * page );
void SetCurrentPage( PDictionary page );	// declared later, this file

// font.cpp
extern void RenderSomeFail( HDC intoDC, HDC tmpDC, char const * content, TextState& t, int height, int zoom );

// image.cpp
extern void RenderImage( HDC intoDC, PStream image, const Matrix & ctm, int pageHeight, XrefTable const & xrefTable, int zoom );

static void DrawString( String str, int width, int height, TextState& t )
{
	if (!str.start) return;

	char sz[4096];

	assert( str.Length() < 4096 && "too damn long" );

	size_t unescapedLen = UnescapeString( sz, str.start, str.end );
	sz[unescapedLen] = 0;

	RenderSomeFail( cacheDC, tmpDC, sz, t, height, zoom );
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

static double ToNumber( const ContentStreamElement& cse )
{
	char* end = const_cast< char* >( cse.end );
	double ret;
	if (!memchr( cse.start, '.', cse.end - cse.start ))
		ret = strtol( cse.start, &end, 10 );
	else
		ret = strtod( cse.start, &end );
	assert( end == cse.end && "Invalid number" );
	return ret;
}

// returns number of ops
static size_t PaintPageContent( int width, int height, PDictionary page, PStream content, TextState& t, size_t& numBinds, int& nopBinds )
{
	size_t length;
	char const * pageContent = content->GetStreamBytes( doc->xrefTable, &length );
	char const * p = pageContent;

	size_t numOperations = 0;
	const int MAX_ARGS = 16;
	ContentStreamElement args[MAX_ARGS];

	DoubleRect mediaBox = GetPageMediaBox( doc.get(), page.get() );
	Matrix ctm;
	ctm.v[0] = (double)width / (72 * (mediaBox.right - mediaBox.left));
	ctm.v[3] = (double)height / (72 * (mediaBox.bottom - mediaBox.top));
	ctm.v[4] = -(72 * (mediaBox.right - mediaBox.left)) * ctm.v[0];
	ctm.v[5] = -(72 * (mediaBox.bottom - mediaBox.top)) * ctm.v[3];

	while( p < pageContent + length )
	{
		assert( *p != '\r' && *p != '\n' );

		for( int i = 0 ; i < MAX_ARGS ; i++ )
			args[i].reset();

		int num_args = 0;
		while( true )
		{
			assert( num_args <= MAX_ARGS );
			if( !args[num_args].parse( p, pageContent+length ) )
				assert( !"WTF" );
			p = args[num_args].end;
			if( args[num_args].type == ContentStreamElement::Elem_Operator )
				break;
			++num_args;
		}

		String op = args[num_args].ToString();
		assert( op.start && op.start != op.end );

		switch( *op.start )
		{
		case 'T':
			assert( op.end == op.start + 2 );
			switch( op.start[1] )
			{
			case 'c':
				assert( num_args == 1 );
				t.c = ToNumber( args[0] );
				break;
			case 'w':
				assert( num_args == 1 );
				t.w = ToNumber( args[0] );
				break;
			case 'z':
				assert( num_args == 1 );
				t.h = ToNumber( args[0] );
				break;
			case 'L':
				assert( num_args == 1 );
				t.l = ToNumber( args[0] );
				break;
			case 's':
				assert( num_args == 1 );
				t.rise = ToNumber( args[0] );
				break;
			case 'r':
				assert( num_args == 1 );
				t.mode = (int)ToNumber( args[0] );
				break;
			case 'f':
				{
					assert( num_args == 2 );
					String name = args[0].ToString();
					memcpy( t.fontName, name.start, name.Length() );
					t.fontNameLen = name.Length();
					t.fontName[ t.fontNameLen ] = 0;
					t.fontSize = ToNumber( args[1] );

					BindFont( page, t, nopBinds );
					++numBinds;
					break;
				}
			case 'm':
				// set text matrix explicit
				assert( num_args == 6 );

				for( int i = 0; i < 6; i++ )
					t.m.v[i] = ToNumber( args[i] );

				// since lm.v[0] and lm.v[3] are the "real" font size,
				// we need to regen our font here

				t.lm = t.m;

				BindFont( page, t, nopBinds );
				++numBinds;
				break;
			case '*':
				// next line based on leading
				assert( num_args == 0 );
				t.lm.v[5] -= t.lm.v[3] * t.l;
				t.m = t.lm;
				break;
			case 'D':
				// next line, setting leading
				assert( num_args == 2 );
				t.l = -ToNumber( args[1] );		// todo: check this
				t.lm.v[4] += t.lm.v[0] * ToNumber( args[0] );
				t.lm.v[5] += t.lm.v[3] * ToNumber( args[1] );
				t.m = t.lm;
				break;
			case 'd':
				// next line with explicit positioning, preserve leading
				assert( num_args == 2 );
				t.lm.v[4] += t.lm.v[0] * ToNumber( args[0] );
				t.lm.v[5] += t.lm.v[3] * ToNumber( args[1] );
				t.m = t.lm;
				break;
			case 'J':
				{
					assert( num_args == 1 );

					for( ContentStreamElement* it = args[0].content ; it ; it = it->next )
					{
						if( it->type == ContentStreamElement::Elem_String )
							DrawString( it->ToString(), width, height, t );
						else
						{
							// todo: non-horizontal writing modes
							double k = ToNumber( *it );
							t.m.v[4] -= t.EffectiveFontWidth() * k / 1000;
						}
					}
					break;
				}
			case 'j':
				assert( num_args == 1 );
				DrawString( args[0].ToString(), width, height, t );
				break;
			}
			break;
		case 'B':
			assert( op.end >= op.start + 2 );
			switch( op.start[1] )
			{
			case 'T':
				assert( op.end == op.start + 2 );
				assert( num_args == 0 );
				t.m = t.lm = Matrix();

				BindFont( page, t, nopBinds );
				++numBinds;
				break;
			}
			break;
		case '\'':
			assert( num_args == 1 );
			t.lm.v[5] -= t.lm.v[3] * t.l;
			t.m = t.lm;
			DrawString( args[0].ToString(), width, height, t );
			break;

		case '"':
			assert( num_args == 3 );
			t.w = ToNumber( args[0] );
			t.c = ToNumber( args[1] );
			t.lm.v[5] -= t.lm.v[3] * t.l;
			t.m = t.lm;
			DrawString( args[2].ToString(), width, height, t );
			break;

		case 'D':
			assert( op.end >= op.start + 2 );
			if( op.start[1] == 'o' && op.end == op.start + 2 )
			{
				assert( args[0].type == ContentStreamElement::Elem_Name );
				String externalObjectName = args[0].ToString();
				PDictionary resources = page->Get<Dictionary>( "Resources", doc->xrefTable );
				if( !resources )
					break;
				PDictionary xobjects = resources->Get<Dictionary>( "XObject", doc->xrefTable );
				if( !xobjects )
					break;
				PStream obj = xobjects->Get<Stream>( externalObjectName, doc->xrefTable );
				if( !obj )
					break;
				PName type = obj->dict->Get<Name>( "Subtype", doc->xrefTable );
				if( !type )
					break;

				if( type->str == String( "Image" ) )
					RenderImage( cacheDC, obj, ctm, height, doc->xrefTable, zoom );
			}
		case 'c':
			if( op.end == op.start + 2 && op.start[1] == 'm' )
			{
				assert( num_args == 6 );

				for( int i = 0; i < 6; i++ )
					ctm.v[i] = ToNumber( args[i] );
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
		while( p < pageContent + length && isspace( *p ) )
			++p;
	}

	return numOperations;
}

extern size_t numGlyphs;

static void PaintPage( int width, int height, PDictionary page )
{
	::Rectangle( cacheDC, 0, 0, ZOOM(width), ZOOM(height) );
	size_t numOperations = 0;
	size_t numBinds = 0;
	int nopBinds = 0;

	TextState t;

	DWORD time = GetTickCount();
	
	PStream content = page->Get<Stream>( "Contents", doc->xrefTable );
	if (content)
		numOperations += PaintPageContent( width, height, page, content, t, numBinds, nopBinds );
	else
	{
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
	sprintf( fail, "len: %u ops: %u t: %u ms binds: %d/%u gcache: %u", 0u, numOperations, time, numBinds - nopBinds, numBinds, numGlyphs);
	TextOutA( cacheDC, 200, 10, fail, strlen(fail) );
}

static std::vector<std::pair<size_t, HBITMAP>> cachedPages;
static const int PAGE_CACHE_MAX_SIZE = 8;

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

static void PaintPageFromCache( HDC dc, DoubleRect const& rect, int offset, PDictionary page, int y )
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

	int zoomedW = ZOOM((int)rect.width());
	int zoomedH = ZOOM((int)rect.height());

	char sz[64];
	sprintf( sz, "PDF Viewer - %dx%d page", zoomedW, zoomedH );
	SetWindowTextA( appHwnd, sz );

	if( !cacheBitmap )
	{
		cacheBitmap = CreateCompatibleBitmap( dc, zoomedW, zoomedH );
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

	BitBlt( dc, offset, y, zoomedW, zoomedH, cacheDC, 0, 0, SRCCOPY );
}

static void ClearPageCache()
{
	while( cachedPages.size() )
	{
		EvictCacheItem( cachedPages[0] );
		cachedPages.erase( cachedPages.begin() );
	}
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

		offsety -= ZOOM((int)mediaBox.height()) + PAGE_GAP;
		offsety2 -=ZOOM((int)mediaBox.height()) + PAGE_GAP;
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
		if( offsety >= ZOOM(-mediaBox.height()) )
			break;

		PDictionary nextPage = doc->GetNextPage( currentPage );
		if (!nextPage)
			break;

		offsety += ZOOM((int)mediaBox.height()) + PAGE_GAP;
		offsety2 +=ZOOM((int)mediaBox.height()) + PAGE_GAP;

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
		double mbHeight = ZOOM(mediaBox.height());
		PDictionary nextPage = doc->GetNextPage( failPage );

		if (!nextPage)
		{
			if (foo + mbHeight + PAGE_GAP < clientRect.bottom)
			{
				int d = (int)(clientRect.bottom - (foo + mbHeight + PAGE_GAP));
				offsety += d;
				offsety2 += d;
			}
			return;
		}

		foo += (int)(mbHeight + PAGE_GAP);

		if (foo > clientRect.bottom)
			return;
		failPage = nextPage;
	}
}

static void PaintView( HWND hwnd, HDC dc, PAINTSTRUCT const * ps )
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

		double width = ZOOM(mediaBox.width());
		int offset = offsetx + (int)(clientRect.right - width) / 2;

		int height = ZOOM((int)mediaBox.height());

		int mbLeft = ZOOM((int)mediaBox.left);
		int mbRight = ZOOM((int)mediaBox.right);

		PaintPageFromCache( dc, mediaBox, offset, page, y );

		RECT dropshadow_right = { offset + mbRight, y + DROP_SHADOW_SIZE, 
			offset + mbRight + DROP_SHADOW_SIZE, y + height + DROP_SHADOW_SIZE };

		RECT dropshadow_bottom = { offset + mbLeft + DROP_SHADOW_SIZE, y + height, 
			offset + mbRight, y + height + DROP_SHADOW_SIZE };

		RECT dropshadow_unfail_right = { dropshadow_right.left, dropshadow_right.top - DROP_SHADOW_SIZE, dropshadow_right.right,
			dropshadow_right.top };

		HBRUSH black = (HBRUSH)::GetStockObject(BLACK_BRUSH);

		RECT left_chunk = { 0, y, offset, y + height };
		RECT right_chunk = { offset + mbRight + DROP_SHADOW_SIZE, 
			y, clientRect.right, y + height };

		y += height;

		RECT fail = { 0, y, clientRect.right, y + PAGE_GAP };
		FillRect( dc, &fail, (HBRUSH)(COLOR_APPWORKSPACE + 1) );

		FillRect( dc, &left_chunk, (HBRUSH)(COLOR_APPWORKSPACE + 1 ) );
		FillRect( dc, &right_chunk, (HBRUSH)(COLOR_APPWORKSPACE + 1 ) );

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
			wsprintf(range, L"of %d", doc->GetPageCount());
			SetDlgItemText(diagHwnd, IDC_VALIDPAGES, range);
			SetDlgItemText(diagHwnd, IDC_PAGENUMBER, L"");

			//Set focus to page number edit control
			if (GetDlgCtrlID((HWND) wp) != IDC_PAGENUMBER)
				SetFocus(GetDlgItem(diagHwnd, IDC_PAGENUMBER));
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
			{
				cacheDC = CreateCompatibleDC( dc );
				tmpDC = CreateCompatibleDC( dc );
			}
			PaintView( hwnd, dc, &ps );
			::EndPaint( hwnd, &ps );
			return 0;
		}

	case WM_LBUTTONDOWN:
		{
			SetFocus( hwnd );
			POINTS fail = MAKEPOINTS( lp );
			haxy = fail.y;
			haxx = fail.x;
			offsety2 = offsety;
			offsetx2 = offsetx;
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
					DWORD start = GetTickCount();
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
					extern size_t GetCachedGlyphCount();

					wchar_t sz[128];
					_snwprintf( sz, 128, L"PDF Viewer - Test complete - %u ms (%u glyphs)", GetTickCount() - start, GetCachedGlyphCount() );
					SetWindowText( appHwnd, sz );
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

			case VK_F8:
				{
					zoom -= 10; 
					if (zoom < 72) 
						zoom = 72;
					ClearPageCache();
					InvalidateRect( hwnd, 0, true );

					wchar_t sz[128];
					_snwprintf( sz, 128, L"PDF Viewer - Zoom=%d", zoom );
					SetWindowText( appHwnd, sz );
				}
				break;

			case VK_F9:
				{
					zoom += 10;
					ClearPageCache();
					InvalidateRect( hwnd, 0, true );

					wchar_t sz[128];
					_snwprintf( sz, 128, L"PDF Viewer - Zoom=%d", zoom );
					SetWindowText( appHwnd, sz );
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
			offsetx = (fail.x - haxx) + offsetx2;
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
