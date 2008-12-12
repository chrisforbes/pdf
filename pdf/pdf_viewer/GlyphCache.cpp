#include "pch.h"
#include "textstate.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "glyphcache.h"

static std::map<FT_Face, std::map<int, std::map<int, std::map<FT_ULong, CachedGlyph> > > > glyphCache;

CachedGlyph* GetGlyph( HDC intoDC, HDC tmpDC, FT_Face face, FT_ULong c, int effectiveWidth, int effectiveHeight )
{
	CachedGlyph& glyph = glyphCache[face][effectiveWidth][effectiveHeight][c];

	if( glyph.bitmap )
		return &glyph;

	FT_Error error = FT_Set_Char_Size( face, effectiveWidth, effectiveHeight, 72, 72 );
	if( error ) return NULL;

	error = FT_Load_Char( face, c, FT_LOAD_RENDER );
	if( error ) return NULL;

	FT_GlyphSlot slot = face->glyph;

	glyph.width = slot->bitmap.width;
	glyph.height = slot->bitmap.rows;
	glyph.left_offset = slot->bitmap_left;
	glyph.top_offset = slot->bitmap_top;
	glyph.advance.x = slot->advance.x;
	glyph.advance.y = slot->advance.y;

	glyph.bitmap = CreateCompatibleBitmap( intoDC, slot->bitmap.width, slot->bitmap.rows );
	assert( glyph.bitmap );
	HGDIOBJ oldBitmap = SelectObject( tmpDC, glyph.bitmap );
	for( int yy = 0 ; yy < slot->bitmap.rows ; yy++ )
		for( int xx = 0 ; xx < slot->bitmap.width ; xx++ )
			SetPixel( tmpDC, xx, yy, 0x010101 * (255 - slot->bitmap.buffer[ xx + yy * slot->bitmap.width ] ) );
	SelectObject( tmpDC, oldBitmap );
	return &glyph;
}
