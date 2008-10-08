#include "pch.h"

HWND outlineHwnd;

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
	//	SetWindowText( appHwnd, L"dest key = string" );
		String * destStr = (String *)dest.get();

		char * p = sz + itemTitle->Length() + sprintf( sz + itemTitle->Length(), ", dest=`" );
		memcpy( p, destStr->start, destStr->Length() );
		p[ destStr->Length() ] = '`';
		p[ 1+ destStr->Length() ] = 0;

		SetWindowTextA( appHwnd, sz );
	}
}