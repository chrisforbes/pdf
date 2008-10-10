#include "pch.h"

void WalkNameTree( Document * doc, Dictionary * node, NameTree & intoTree )
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
			PObject child = Object::ResolveIndirect_(*it, doc->xrefTable);
			assert( child->Type() == ObjectType::Dictionary );
			WalkNameTree( doc, (Dictionary *)child.get(), intoTree );
		}
}

PObject NameTreeGetValue( Document * doc, NameTree const & tree, String key )
{
	NameTree::const_iterator it = tree.find( key );
	return (it == tree.end()) ? PObject() : it->second;
}