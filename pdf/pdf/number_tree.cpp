#include "pch.h"

void WalkNumberTree( Document * doc, Dictionary * node, NumberTree& intoTree )
{
	PArray nums = node->Get<Array>( "Nums", doc->xrefTable );
	PArray kids = node->Get<Array>( "Kids", doc->xrefTable );

	if (nums)
		for( std::vector<PObject>::const_iterator it = nums->elements.begin();
			it != nums->elements.end(); it+=2 )
		{
			assert((*it)->Type() == ObjectType::Number);
			int n = ((Number *)it->get())->num;
			PObject value = it[1];

			intoTree[n] = value;
		}

	if (kids)
		for( std::vector<PObject>::const_iterator it = kids->elements.begin();
			it != kids->elements.end(); it++ )
		{
			PObject child = Object::ResolveIndirect_(*it, doc->xrefTable);
			assert(child->Type() == ObjectType::Dictionary);
			WalkNumberTree( doc, (Dictionary *)child.get(), intoTree );
		}
}