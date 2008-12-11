#include "pch.h"
#include "..\pdf\parse.h"

HWND outlineHwnd;

extern void SetCurrentPage( PDictionary page );
extern PObject NameTreeGetValue( Document * doc, String key );

HTREEITEM AddOutlineItem( char const * start, size_t len, bool hasChildren, HTREEITEM parent, void * value )
{
	USES_CONVERSION;
	char temp[512];
	size_t escaped_len;
	escaped_len = UnescapeString( temp, start, start + len );
	temp[escaped_len] = 0;
	
	TV_INSERTSTRUCT is;
	::memset( &is, 0, sizeof( TV_INSERTSTRUCT ) );
	is.hParent = parent;
	is.hInsertAfter = TVI_LAST;
	is.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM;
	is.item.cChildren = hasChildren ? 1 : 0;
	is.item.pszText = A2W( temp );
	is.item.cchTextMax = escaped_len;
	is.item.lParam = (LPARAM)value;

	return TreeView_InsertItem( outlineHwnd, &is );
}

void BuildOutline( Dictionary * parent, HTREEITEM parentItem, const XrefTable & objmap )
{
	if (!parent)
		return;

	Dictionary * item = parent->Get<Dictionary>( "First", objmap ).get();
	
	while( item )
	{
		String * itemTitle = item->Get<String>( "Title", objmap ).get();

		HTREEITEM treeItem = AddOutlineItem( 
			itemTitle->start, 
			itemTitle->end - itemTitle->start, 
			(bool)item->Get( "First" ), parentItem, 
			item );

		item = item->Get<Dictionary>( "Next", objmap ).get();
	}
}

void ExpandNode( Document * doc, NMTREEVIEW * info )
{
	if (!doc || !info->itemNew.hItem)
		return;

	/* blatant hack. if we didnt get spurious messages, we wouldnt need this. */
	/* thanks, commctrl guys! */

	if (TreeView_GetChild( outlineHwnd, info->itemNew.hItem ))
		return;	// already populated

	// grab the fields we actually care about (value ptr, whether we should have children)
	TVITEM item;
	memset( &item, 0, sizeof( item ) );
	item.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_CHILDREN;
	item.hItem = info->itemNew.hItem;

	TreeView_GetItem( outlineHwnd, &item );

	// it's possible that lParam = 0 here, since the tree has a builtin root node
	if (item.lParam && item.cChildren)
		BuildOutline( (Dictionary *)item.lParam, item.hItem, doc->xrefTable );
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
		dest = Object::ResolveIndirect_( NameTreeGetValue( doc, *s ), doc->xrefTable );
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