#include "pch.h"
#include "textstate.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#pragma comment( lib, "freetype.lib" )

static FT_Face face = 0;
extern PDocument doc;

struct
	{
		BITMAPINFOHEADER h;
		RGBQUAD grayLevels[256];
	} bmi = 
	{
		{ sizeof(BITMAPINFOHEADER), 0, 1, 1, 8, BI_RGB, 0, 0, 0, 0,0 }, { 0 }
	};

void InitGrayLevels()
{
	// build this really screwy graylevels table. 
	// this is also incorrect, as lum != avg(r,g,b)
	for( int i = 0; i < 256; i++ )
		bmi.grayLevels[i].rgbBlue = bmi.grayLevels[i].rgbGreen = bmi.grayLevels[i].rgbRed = (255-i);
}

static void DrawChar( HDC intoDC, FT_GlyphSlot slot, int x, int y )
{
	bmi.h.biWidth = slot->bitmap.width;
	
	// what a hack: if we try to copy the entire image, GDI insists on screwing up the stride. why?
	// stared at the docs for a while, but until this is a real perf problem, i don't care.
	for( int v = 0; v < slot->bitmap.rows; v++ )
		StretchDIBits( intoDC, x + slot->bitmap_left, y - slot->bitmap_top + v, slot->bitmap.width, 1,
			0, 0, slot->bitmap.width, 1, slot->bitmap.buffer + v * slot->bitmap.width, 
			(BITMAPINFO const *) &bmi, DIB_RGB_COLORS, SRCCOPY ); 
}

void RenderSomeFail( HDC intoDC, char const * content, TextState& t, int height )
{
	assert( face && "This stuff needs to be initialized!" );

	int error = FT_Set_Char_Size( face, (int)(64 * t.EffectiveFontWidth()), (int)(64 * t.EffectiveFontHeight()), 72, 72 );

	FT_GlyphSlot slot = face->glyph;

	while( *content )
	{
		error = FT_Load_Char( face, *content, FT_LOAD_RENDER );

		if (!error)
		{

		// draw to target
		DrawChar( intoDC, slot, t.m.v[4], height - t.m.v[5] - t.rise );

		t.m.v[4] += (slot->advance.x / 64.0);	// what a hack, subpixel failure, etc

		t.m.v[4] += t.c * t.EffectiveFontHeight();// * t.HorizontalScale();
		if (*content == ' ')
			t.m.v[4] += t.w * t.EffectiveFontHeight();// * t.HorizontalScale();	
		}

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

extern FT_Face LoadFontFromCache( PDictionary fontDescriptor, XrefTable const & xrefTable );

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

	face = LoadFontFromCache( fontDescriptor, doc->xrefTable );
}
