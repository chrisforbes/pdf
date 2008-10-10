#include "pch.h"

HWND outlineHwnd;

extern void SetCurrentPage( PDictionary page );

HTREEITEM AddOutlineItem( char const * start, size_t len, bool hasChildren, HTREEITEM parent, void * value )
{
	USES_CONVERSION;
	char temp[512];
	memcpy( temp, start, len );
	temp[len] = 0;

	TV_INSERTSTRUCT is;
	::memset( &is, 0, sizeof( TV_INSERTSTRUCT ) );
	is.hParent = parent;
	is.hInsertAfter = TVI_LAST;
	is.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM;
	is.item.cChildren = hasChildren ? 1 : 0;
	is.item.pszText = A2W( temp );
	is.item.cchTextMax = len;
	is.item.lParam = (LPARAM)value;

	return TreeView_InsertItem( outlineHwnd, &is );
}

void BuildOutline( Dictionary * parent, HTREEITEM parentItem, const XrefTable & objmap )
{
	Dictionary * item = parent->Get<Dictionary>( "First", objmap ).get();
	
	while( item )
	{
		String * itemTitle = item->Get<String>( "Title", objmap ).get();

		HTREEITEM treeItem = AddOutlineItem( 
			itemTitle->start, 
			itemTitle->end - itemTitle->start, 
			(bool)item->Get( "First" ), parentItem, 
			item );

		BuildOutline( item, treeItem, objmap );

		item = item->Get<Dictionary>( "Next", objmap ).get();
	}
}

void NavigateToPage( HWND appHwnd, Document * doc, NMTREEVIEW * info )
{
	if (!info->itemNew.hItem)
		return;
	
	Dictionary * dict = (Dictionary *)info->itemNew.lParam;

	if (!dict)
		return;

	PObject dest = dict->Get( "Dest", doc->xrefTable );
	if (!dest)
		return;

	if (dest->Type() == ObjectType::String)
	{
		String * s = (String *)dest.get();
		dest = Object::ResolveIndirect_( doc->namedDestinations[*s], doc->xrefTable );
	}

	PArray destArray;

	if (dest->Type() == ObjectType::Dictionary)
	{
		PDictionary d = boost::shared_static_cast<Dictionary>(dest);
		//TODO: Implement link action
		//For now handle everything as GoTo (here be Raptors)
		//d->Get<Name>("S", doc->xrefTable);
		destArray = d->Get<Array>("D", doc->xrefTable);
	}
	else if (dest->Type() == ObjectType::Array)
		destArray = boost::shared_static_cast<Array>(dest);

	if (destArray)
	{
		if (destArray->elements.empty()) return;
		
		PDictionary page = boost::shared_static_cast<Dictionary>(
			Object::ResolveIndirect_(destArray->elements[0], doc->xrefTable));

		SetCurrentPage( page );
	}
}