#include "pch.h"
#include <algorithm>

static double ToNumber( PObject obj )
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

DoubleRect GetPageRect( Document * doc, Dictionary * page, char const ** names, size_t numNames )
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
