#include "pch.h"
#include <algorithm>

double ToNumber( PObject obj )
{
	if (obj->Type() == ObjectType::Number)
		return ((Number *)obj.get())->num;
	if (obj->Type() == ObjectType::Double)
		return ((Double *)obj.get())->num;

	assert( false && "Broken rect!!" );
}

static DoubleRect RectFromArray( Array * arr )
{
	assert( arr->elements.size() == 4 );

	double left = ToNumber( arr->elements[0] );
	double right = ToNumber( arr->elements[2] );

	double top = ToNumber( arr->elements[1] );
	double bottom = ToNumber( arr->elements[3] );

	if (left > right) std::swap( left, right );		// get this hax the right way round
	if (top > bottom) std::swap( top, bottom );

	return DoubleRect( left, top, right, bottom );
}

static DoubleRect GetPageRect( Document * doc, Dictionary * page, char const ** names, size_t numNames )
{
	for( char const ** name = names; name != names + numNames; name++ )
	{
		while( page )
		{
			PArray arr = page->Get<Array>( *name, doc->xrefTable );
			if (arr)
				return RectFromArray( arr.get() );

			page = page->Get<Dictionary>( "Parent", doc->xrefTable ).get();
		}
	}

	assert( false && "Broken page!!" );
}

static char const * mediaBox[] = { "MediaBox" };
static char const * cropBox[] = { "CropBox", "MediaBox" };
static char const * bleedBox[] = { "BleedBox", "CropBox", "MediaBox" };
static char const * trimBox[] = { "TrimBox", "CropBox", "MediaBox" };
static char const * artBox[] = { "ArtBox", "CropBox", "MediaBox" };

DoubleRect GetPageMediaBox( Document * doc, Dictionary * page )
	{ return GetPageRect( doc, page, mediaBox, 1 ); }
DoubleRect GetPageCropBox( Document * doc, Dictionary * page )
	{ return GetPageRect( doc, page, cropBox, 2 ); }
DoubleRect GetPageBleedBox( Document * doc, Dictionary * page )
	{ return GetPageRect( doc, page, bleedBox, 3 ); }
DoubleRect GetPageTrimBox( Document * doc, Dictionary * page )
	{ return GetPageRect( doc, page, trimBox, 3 ); }
DoubleRect GetPageArtBox( Document * doc, Dictionary * page )
	{ return GetPageRect( doc, page, artBox, 3 ); }
