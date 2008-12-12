#include "pch.h"

static PDictionary GetPageInner( Document * doc, PDictionary node, size_t n, size_t a )
{
	PArray kids = node->Get<Array>( "Kids", doc->xrefTable );
	if (!kids)
		return node;

	for( std::vector<PObject>::iterator it = kids->elements.begin(); it != kids->elements.end(); it++ )
	{
		PDictionary kid = Object::ResolveIndirect_<Dictionary>( *it, doc->xrefTable );
		PNumber countP = kid->Get<Number>( "Count", doc->xrefTable );
		size_t count = countP ? countP->num : 1;

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

static size_t GetPageIndexInner( Document * doc, PDictionary node )
{
	PDictionary parent = node->Get<Dictionary>("Parent", doc->xrefTable);
	if (!parent) return 0;
	PArray children = parent->Get<Array>("Kids", doc->xrefTable);
	assert( children );
	size_t n = 0;
	for (std::vector<PObject>::const_iterator it = children->elements.begin(); it != children->elements.end(); it++)
	{
		PDictionary child = Object::ResolveIndirect_<Dictionary>( *it, doc->xrefTable );
		if (node == child)
			return n + GetPageIndexInner( doc, parent );
		PNumber count = child->Get<Number>( "Count", doc->xrefTable );
		n += count ? count->num : 1;
	}
	assert( false );
}

size_t Document::GetPageIndex( PDictionary page )
{
	return GetPageIndexInner( this, page );
}

PDictionary Document::GetNextPage(PDictionary page)
{
	return GetPage( GetPageIndex( page ) + 1 );
}

PDictionary Document::GetPrevPage(PDictionary page)
{
	return GetPage( GetPageIndex( page ) - 1 );
}

size_t Document::GetPageCount()
{
	PNumber count = pageRoot->Get<Number>( "Count", xrefTable );
	return count->num;
}