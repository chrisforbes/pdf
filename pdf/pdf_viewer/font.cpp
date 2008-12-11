#include "pch.h"
#include "textstate.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#pragma comment( lib, "freetype.lib" )

static FT_Library library;
static FT_Face face = 0;
extern Document * doc;

static bool isInited;

struct
	{
		BITMAPINFOHEADER h;
		RGBQUAD grayLevels[256];
	} bmi = 
	{
		{ sizeof(BITMAPINFOHEADER), 0, 1, 1, 8, BI_RGB, 0, 0, 0, 0,0 }, { 0 }
	};

void InitFontSystem( void )
{
	if (isInited) return;
	FT_Init_FreeType( &library );
	isInited = true;

	// build this really screwy graylevels table. 
	// this is also incorrect, as lum != avg(r,g,b)
	for( int i = 0; i < 256; i++ )
		bmi.grayLevels[i].rgbBlue = bmi.grayLevels[i].rgbGreen = bmi.grayLevels[i].rgbRed = (255-i);
}

static PDictionary lastFontDescriptor;

void InstallEmbeddedFont( PDictionary fontDescriptor, int& nopBinds )
{
	InitFontSystem();

	if (lastFontDescriptor == fontDescriptor)
	{
		++nopBinds;
		return;
	}

	/* todo: cache multiple faces, be vaguely smart, etc */
	if (face)
	{
		FT_Done_Face( face );
		face = 0;
	}

	lastFontDescriptor = fontDescriptor;

	PStream ff3 = fontDescriptor->Get<Stream>( "FontFile3", doc->xrefTable );
	if (!ff3)
		return;	// we're done here

	String subtype = ff3->dict->Get<Name>("Subtype", doc->xrefTable)->str;

	size_t streamSize = 0;
	char const * data = ff3->GetStreamBytes( doc->xrefTable, &streamSize );

	int error = FT_New_Memory_Face( library, (FT_Byte const *)data, streamSize, 0, &face );
	if ( error )
	{
		char sz[64];
		sprintf( sz, "Freetype2 error: %x", error );
		MessageBoxA( 0, sz, "Fail", 0 );
	}
}

static void DrawChar( HDC intoDC, FT_GlyphSlot slot, int x, int y )
{
	bmi.h.biWidth = slot->bitmap.width;
	
	// what a hack: if we try to copy the entire image, GDI insists on screwing up the stride. why?
	// stared at the docs for a while, but until this is a real perf problem, i don't care.
	for( int v = 0; v < slot->bitmap.rows; v++ )
		StretchDIBits( intoDC, x - slot->bitmap_left, y - slot->bitmap_top + v, slot->bitmap.width, 1,
			0, 0, slot->bitmap.width, 1, slot->bitmap.buffer + v * slot->bitmap.width, 
			(BITMAPINFO const *) &bmi, DIB_RGB_COLORS, SRCCOPY ); 
}

void RenderSomeFail( HDC intoDC, char const * content, TextState& t, int height )
{
	assert( library && face && "This stuff needs to be initialized!" );

	int error = FT_Set_Char_Size( face, 0, (int)(64 * t.EffectiveFontHeight()), 72, 72 );

	FT_GlyphSlot slot = face->glyph;

	while( *content )
	{
		error = FT_Load_Char( face, *content, FT_LOAD_RENDER );

		if (!error)
		{
			assert( t.h == 100 );

		// draw to target
		DrawChar( intoDC, slot, t.m.v[4], height - t.m.v[5] - t.rise );

		t.m.v[4] += (slot->advance.x / 64.0);	// what a hack, subpixel failure, etc

		t.m.v[4] += t.c * t.HorizontalScale();
		if (*content == ' ')
			t.m.v[4] += t.w * t.HorizontalScale();	
		}

		++content;
	}
}