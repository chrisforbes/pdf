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
	{
		SetWindowText( appHwnd, L"(no page) - PDF Viewer" );
		return;
	}
	
	Dictionary * dict = (Dictionary *)info->itemNew.lParam;

	if (!dict)
	{
		SetWindowText( appHwnd, L"(no page [2]) - PDF Viewer" );
		return;
	}

	PString itemTitle = dict->Get<String>( "Title", doc->xrefTable );

	char sz[1024];
	memcpy( sz, itemTitle->start, itemTitle->Length() );
	sz[itemTitle->Length()] = 0;

	SetWindowTextA( appHwnd, sz );	// WTF, hax

	// todo: follow link

	PObject dest = dict->Get( "Dest", doc->xrefTable );
	if (!dest)
	{
		SetWindowText( appHwnd, L"no dest key" );
		return;
	}

	if (dest->Type() == ObjectType::Array)
	{
		// todo: navigate to referenced page
		SetWindowText( appHwnd, L"dest key = array" );
		return;
	}

	if (dest->Type() == ObjectType::String)
	{
		String * s = (String *)dest.get();

		PObject destVal = Object::ResolveIndirect_(doc->namedDestinations[*s], doc->xrefTable);
		PArray destArray;
		if (destVal->Type() == ObjectType::Dictionary)
		{
			PDictionary d = boost::shared_static_cast<Dictionary>(destVal);
			//TODO: Implement link action
			//For now handle everything as GoTo (here be Raptors)
			//d->Get<Name>("S", doc->xrefTable);
			destArray = d->Get<Array>("D", doc->xrefTable);
		}
		else if (destVal->Type() == ObjectType::Array)
			destArray = boost::shared_static_cast<Array>(destVal);

		if (destArray)
		{
			if (destArray->elements.empty()) return;
			
			PDictionary page = boost::shared_static_cast<Dictionary>(
				Object::ResolveIndirect_(destArray->elements[0], doc->xrefTable));

			SetCurrentPage( page );

			size_t pageNum = doc->GetPageIndex( page );
			sprintf(sz, "page %u", pageNum + 1);
			SetWindowTextA( appHwnd, sz );
		}
	}
}