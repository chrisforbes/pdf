#include "pch.h"
#include "textstate.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma comment( lib, "freetype.lib" )

#include "glyphcache.h"

static FT_Face face = 0;
extern PDocument doc;

extern FT_Face LoadFontFromCache( PDictionary fontDescriptor, XrefTable const & xrefTable );

void RenderSomeFail( HDC intoDC, char const * content, TextState& t, int height )
{
	assert( face && "This stuff needs to be initialized!" );

	HDC tmpDC = CreateCompatibleDC( intoDC );
	while( *content )
	{
		CachedGlyph* glyph = GetGlyph( intoDC, tmpDC, face, *content, (int)(64 * t.EffectiveFontWidth()), (int)(64 * t.EffectiveFontHeight()) );
		assert( glyph );

		int x = (int)t.m.v[4];
		int y = (int)(height - t.m.v[5] - t.rise);

		HGDIOBJ oldBitmap = SelectObject( tmpDC, glyph->bitmap );
		BitBlt( intoDC, x + glyph->left_offset, y - glyph->top_offset, glyph->width, glyph->height, tmpDC, 0, 0, SRCCOPY );
		oldBitmap = SelectObject( tmpDC, oldBitmap );

		t.m.v[4] += (glyph->advance.x / 64.0);	// what a hack, subpixel failure, etc

		t.m.v[4] += t.c * t.EffectiveFontHeight();// * t.HorizontalScale();
		if (*content == ' ')
			t.m.v[4] += t.w * t.EffectiveFontHeight();// * t.HorizontalScale();	

		++content;
	}
	DeleteDC( tmpDC );
}

static PDictionary GetResources( PDictionary page )
{
	while( page )
	{
		PDictionary resources = page->Get<Dictionary>( "Resources", doc->xrefTable );
		if( resources )
			return resources;

		page = page->Get<Dictionary>( "Parent", doc->xrefTable );
	}
	return PDictionary();
}

void BindFont( PDictionary page, TextState& t, int& nopBinds )
{
	if( !t.fontNameLen ) return;
	PDictionary resources = GetResources( page );
	if( !resources ) return;
	PDictionary fonts = resources->Get<Dictionary>( "Font", doc->xrefTable );
	if( !fonts ) return;
	PDictionary font = fonts->Get<Dictionary>( t.fontName, doc->xrefTable );
	if( !font ) return;

	//String subtype = font->Get<Name>( "Subtype", doc->xrefTable )->str;
	//String baseFont = font->Get<Name>( "BaseFont", doc->xrefTable )->str;
	PDictionary fontDescriptor = font->Get<Dictionary>( "FontDescriptor", doc->xrefTable );

	FT_Face newFace = LoadFontFromCache( fontDescriptor, doc->xrefTable );
	if( newFace == face )
		++nopBinds;
	face = newFace;
}
