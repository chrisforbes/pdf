#include "pch.h"

static PDictionary GetPageInner( Document * doc, PDictionary node, size_t n, size_t a )
{
	PArray kids = node->Get<Array>( "Kids", doc->xrefTable );
	if (!kids)
		return node;

	for( std::vector<PObject>::iterator it = kids->elements.begin(); it != kids->elements.end(); it++ )
	{
		PDictionary kid = boost::shared_static_cast<Dictionary>(Object::ResolveIndirect_( *it, doc->xrefTable ));
		PNumber countP = kid->Get<Number>( "Count", doc->xrefTable );
		size_t count = countP ? countP.get()->num : 1;

		if (n >= a && n < a + count)
			return GetPageInner( doc, kid, n, a );
		else
			a += count;
	}

	return PDictionary();
}

PDictionary Document::GetPage( size_t n )
{
	return GetPageInner( this, pageRoot, n, 0 );
}