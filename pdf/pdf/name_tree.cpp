#include "pch.h"

PObject NameTreeGetValueInner( Document * doc, PDictionary node, String key )
{
	PArray names = node->Get<Array>( "Names", doc->xrefTable );
	if (names)
	{
		// todo: binary search here, these elements are ordered

		std::vector<PObject>::const_iterator it;
		for( it = names->elements.begin(); it != names->elements.end(); it += 2)
		{
			if (key == *(String *)(it[0].get()))
				return it[1];
		}

		return PObject();	// not found
	}

	PArray kids = node->Get<Array>( "Kids", doc->xrefTable );
	if (kids)
	{
		// todo: binary search here, these are ordered too

		std::vector<PObject>::const_iterator it;
		for( it = kids->elements.begin(); it != kids->elements.end(); it++ )
		{
			PDictionary kid = Object::ResolveIndirect_<Dictionary>( *it, doc->xrefTable );
			PArray limits = kid->Get<Array>( "Limits", doc->xrefTable );

			if (key < *(String *)(limits->elements[0].get()))
				continue;
			if (*(String *)(limits->elements[1].get()) < key)
				continue;

			return NameTreeGetValueInner( doc, kid, key );
		}
	}

	return PObject();	// *really* not found
}

PObject NameTreeGetValue( Document * doc, String key )
{
	return NameTreeGetValueInner( doc, doc->nameTreeRoot, key );
}