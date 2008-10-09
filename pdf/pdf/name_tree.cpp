#include "pch.h"

void WalkNamedDestinationsTree( Document * doc, Dictionary * node, NameTree & intoTree )
{
	PArray names = node->Get<Array>( "Names", doc->xrefTable );
	PArray kids = node->Get<Array>( "Kids", doc->xrefTable );

	if (names)
		for (std::vector<PObject>::const_iterator it = names->elements.begin();
			it != names->elements.end(); it += 2)
		{
			assert( (*it)->Type() == ObjectType::String );
			String str = *(String *)it[0].get();
			PObject value = it[1];
			intoTree[str] = value;
		}

	if (kids)
		for (std::vector<PObject>::const_iterator it = kids->elements.begin();
			it != kids->elements.end(); it++)
		{
			assert( (*it)->Type() == ObjectType::Dictionary );
			WalkNamedDestinationsTree( doc, (Dictionary *)it->get(), intoTree );
		}
}