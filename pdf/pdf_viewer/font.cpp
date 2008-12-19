#include "pch.h"
#include "textstate.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef DEBUG
#pragma comment( lib, "freetyped.lib" )
#else
#pragma comment( lib, "freetype.lib" )
#endif

#include "glyphcache.h"

#define ZOOM(k) ((k) * zoom / 72)

static FT_Face face = 0;
extern PDocument doc;

extern FT_Face LoadFontFromCache( PDictionary fontDescriptor, XrefTable const & xrefTable );

void RenderSomeFail( HDC intoDC, HDC tmpDC, char const * content, TextState& t, int height, int zoom )
{
	assert( face && "This stuff needs to be initialized!" );

	while( *content )
	{
		CachedGlyph* glyph = GetGlyph( intoDC, tmpDC, face, *content, 
			(int)(t.EffectiveFontWidth() * zoom * 8 / 9), 
			(int)(t.EffectiveFontHeight() * zoom * 8 / 9) );
		assert( glyph );

		double x = t.m.v[4];
		int y = (int)(height - t.m.v[5] - t.rise);

		HGDIOBJ oldBitmap = SelectObject( tmpDC, glyph->bitmap );
		BitBlt( intoDC, (int)ZOOM(x) + glyph->left_offset, ZOOM(y) - glyph->top_offset,
			glyph->width, glyph->height, tmpDC, 0, 0, SRCAND ); // TODO: when we have color, we need a second blit.
		oldBitmap = SelectObject( tmpDC, oldBitmap );

		t.m.v[4] += ((float)glyph->advance.x * 9 / (8 * zoom) ); // what a hack, subpixel failure, etc

		t.m.v[4] += t.c * t.EffectiveFontHeight();
		if (*content == ' ')
			t.m.v[4] += t.w * t.EffectiveFontHeight();

		++content;
	}
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
	if( newFace )
		face = newFace;
}
