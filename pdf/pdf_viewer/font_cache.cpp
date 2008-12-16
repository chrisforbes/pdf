#include "pch.h"
#include "textstate.h"

#include <ft2build.h>
#include FT_FREETYPE_H

class FontCache
{
	FT_Library library;
	FT_Face face_arial;

	std::map<PDictionary, FT_Face> faces;

	FT_Face LoadCompactFont( PDictionary fontDescriptor, PStream ff3, XrefTable const & xrefTable )
	{
		FT_Face face;
		String subtype = ff3->dict->Get<Name>( "Subtype", xrefTable )->str;
		size_t streamSize = 0;
		char const * data = ff3->GetStreamBytes( xrefTable, &streamSize );

		int error = FT_New_Memory_Face( library, (FT_Byte const *)data, streamSize, 0, &face );
		if ( error )
		{
			char sz[256];
			sprintf( sz, "Freetype2 error loading from ff3: %x", error );
			MessageBoxA( 0, sz, "Fail", 0 );
		}

		return face;
	}

	FT_Face LoadTrueTypeFont( PDictionary fontDescriptor, PStream ff2, XrefTable const & xrefTable )
	{
		FT_Face face;
		size_t streamSize = 0;
		char const * data = ff2->GetStreamBytes( xrefTable, &streamSize );

		int error = FT_New_Memory_Face( library, (FT_Byte const *)data, streamSize, 0, &face );
		if ( error )
		{
			char sz[64];
			sprintf( sz, "Freetype2 error loading from ff2: %x", error );
			MessageBoxA( 0, sz, "Fail", 0 );
		}

		return face;
	}

	FT_Face LoadExternalFont( PDictionary fontDescriptor, String font )
	{
		// TODO: load the appropriate font
		return face_arial;
	}

	FT_Face FontCache::LoadFont( PDictionary fontDescriptor, XrefTable const & xrefTable )
	{
		PStream ff1 = fontDescriptor->Get<Stream>( "FontFile", xrefTable );
		if (ff1)
		{
			MessageBox( 0, L"FontFile", L"noti", 0 );
			return NULL;
		}

		PStream ff2 = fontDescriptor->Get<Stream>( "FontFile2", xrefTable );
		if (ff2)
			return LoadTrueTypeFont( fontDescriptor, ff2, xrefTable );

		PStream ff3 = fontDescriptor->Get<Stream>( "FontFile3", xrefTable );
		if (ff3)
			return LoadCompactFont( fontDescriptor, ff3, xrefTable );

		return LoadExternalFont( fontDescriptor, String( "A value that is currently ignored" ) );
	}

public:
	FontCache()
	{
		FT_Init_FreeType( &library );

		char fontPath[ MAX_PATH ];
		char* end = fontPath + GetWindowsDirectoryA( fontPath, MAX_PATH );
		assert( end != fontPath );
		if( end[-1] == '\\' || end[-1] == '/' )
			--end;
		strncpy( end, "\\fonts\\arial.ttf", fontPath + MAX_PATH - end );

		fontPath[ MAX_PATH - 1 ] = 0;

		int error = FT_New_Face( library, fontPath, 0, &face_arial );
		if ( error )
		{
			char sz[64];
			sprintf( sz, "Freetype2 error loading from external: %x", error );
			MessageBoxA( 0, sz, "Fail", 0 );
			face_arial = NULL;
		}
	}

	FT_Face GetFont( PDictionary fontDescriptor, XrefTable const & xrefTable )
	{
		std::map<PDictionary, FT_Face>::const_iterator it = faces.find( fontDescriptor );

		if( it != faces.end() )
			return it->second;

		FT_Face face = LoadFont( fontDescriptor, xrefTable );
		faces[ fontDescriptor ] = face;
		return face;
	}
};

FontCache* fontCache = NULL;

FT_Face LoadFontFromCache( PDictionary fontDescriptor, XrefTable const & xrefTable )
{
	if( !fontDescriptor )
	{
		//MessageBox( 0, L"WTF ARE YOU DOING, THERE IS NO SPOON/FONT", L"Epic Fail", 0 );
		return NULL;
	}

	if( !fontCache )
		fontCache = new FontCache();

	return fontCache->GetFont( fontDescriptor, xrefTable );
}
