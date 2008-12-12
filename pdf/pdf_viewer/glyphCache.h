#pragma once

struct CachedGlyph
{
	HBITMAP bitmap;
	int width;
	int height;
	int top_offset;
	int left_offset;
	FT_Vector advance;

	CachedGlyph()
	{
		memset( this, 0, sizeof( this ) );
	}
};

CachedGlyph* GetGlyph( HDC intoDC, HDC tmpDC, FT_Face face, FT_ULong c, int effectiveWidth, int effectiveHeight );
