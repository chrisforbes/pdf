#include "pch.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#pragma comment( lib, "freetype.lib" );

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

	// build this really screwy graylevels table. this is also incorrect, as
	// lum != avg(r,g,b)
	for( int i = 0; i < 256; i++ )
		bmi.grayLevels[i].rgbBlue = bmi.grayLevels[i].rgbGreen = bmi.grayLevels[i].rgbRed = (255-i);
}

int y = 30;

void FontNewPage() { y = 30; }

void InstallEmbeddedFont( PDictionary fontDescriptor )
{
	InitFontSystem();
	if (face)
	{
		FT_Done_Face( face );
		face = 0;
	}

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

void RenderSomeFail( HDC intoDC, char const * content )
{
	assert( library && face && "This stuff needs to be initialized!" );

	const int height = 48 * 64;	// 48pt
	
	int error = FT_Set_Char_Size( face, 0, height, 72, 72 );

	FT_GlyphSlot slot = face->glyph;
	int x = 200;//, y = 200;

	while( *content )
	{
		error = FT_Load_Char( face, *content, FT_LOAD_RENDER );
		++content;

		if (error) continue;

		// draw to target
		DrawChar( intoDC, slot, x, y );

		x += slot->advance.x >> 6;	// what a hack, subpixel failure, etc
	}

	y += 50;
}