#include "pch.h"
#include "mapped_file.h"
#include "token.h"
#include "parse.h"

#include "commctrl.h"

// todo: awesome joke somewhere about RDF = reality distortion field



int GetPdfVersion( MappedFile const & f )
{
	if (!::memcmp( "%PDF-1.", f.F(), 7 ))
		return f.F()[7];

	if (!::memcmp( "%!PS-Adobe-", f.F(), 11 ))
		return f.F()[21];

	return 0;
}

char const * GetPdfEof( MappedFile const & f )
{
	char const * end = f.F() + f.Size() - 1024;
	char const * start = f.F() + f.Size() - 5;

	while( start > end )
		if ( !memcmp( start, "%%EOF", 5 ) )
			return start;
		else
			--start;

	return 0;
}

char const * GetPdfPrevLine( MappedFile const & f, char const * p )
{
	--p;
	if (*p == '\n') --p;
	if (*p == '\r') --p;

	while( p[-1] != '\n' && p[-1] != '\r' )
		--p;

	return p;
}

char const * GetPdfNextLine( MappedFile const & f, char const * p )
{
	while( *p != '\r' && *p != '\n' )
		++p;

	if (*p == '\r') ++p;
	if (*p == '\n') ++p;

	return p;
}

const char * ReadPdfXrefSubsection( const MappedFile& file, const char* p, XrefTable& m )
{
	const char* q = GetPdfNextLine( file, p );

	char* tokEnd = const_cast<char*>( q );
	size_t first = strtoul( p, &tokEnd, 10 );
	if( !tokEnd || tokEnd == p )
		return 0;

	p = tokEnd;
	while( *p == ' ' )
		++p;

	tokEnd = const_cast<char*>( q );
	size_t count = strtoul( p, &tokEnd, 10 );
	if( !tokEnd || tokEnd == p )
		return 0;

	p = q;

	if( m.size() < first+count )
		m.insert( m.end(), first + count - m.size(), Xref() );

	size_t end = first + count;

	for( ; first < end ; first++ )
	{
		q = GetPdfNextLine( file, p );
		tokEnd = const_cast<char*>( q );
		size_t fileOffset = strtoul( p, &tokEnd, 10 );
		if( tokEnd != p + 10 )
			return 0;

		p += 11;
		tokEnd = const_cast<char*>( q );
		int generation = strtol( p, &tokEnd, 10 );
		if( tokEnd != p + 5 )
			return 0;

		m[first].generation = generation;
		if( p[6] == 'f' )
			m[first].ptr = 0;
		else
			m[first].ptr = file.F() + fileOffset;

		p = q;
	}
	return p;
}

char const * ReadPdfXrefSection( MappedFile const & f, char const * p, XrefTable& m )
{
	p = GetPdfNextLine(f,p);	// `xref` line

	while( p && ::memcmp( p, "trailer", 7 ) != 0 )
		p = ReadPdfXrefSubsection( f, p, m );

	return p;

	//size_t currentObjIndex = 0;

	//for(;;)
	//{
	//	if (!::memcmp( p, "trailer", 7 ))
	//		return p;	// we're done

	//	char const * nextLine = GetPdfNextLine(f,p);
	//	char * q = const_cast<char *>(nextLine);

	//	if (*p == '\r' || *p == '\n')
	//	{
	//		p = nextLine;
	//		continue;
	//	}

	//	size_t a = ::strtol( p, &q, 10 );
	//	if (!q || p == q)
	//		return 0;

	//	p = q;
	//	q = const_cast<char *>(nextLine);

	//	size_t b = ::strtol( p, &q, 10 );
	//	if (!q || p == q)
	//		return 0;

	//	p = q;
	//	while( *p == ' ' ) ++p;		// eat the spaces

	//	if (*p == '\r' || *p == '\n')
	//	{
	//		currentObjIndex = a;
	//	}
	//	else
	//	{
	//		size_t objIndex = currentObjIndex++;
	//		
	//		THIS_NEEDS_I;
	//		Xref& obj = m[objIndex];
	//		
	//		if (obj.generation > b)
	//			continue;

	//		obj.ptr = *p == 'f' ? 0 : f.F() + a;
	//		obj.generation = b;
	//	}

	//	p = nextLine;
	//}
}

PObject InnerParseIndirectObject( Indirect * i, const XrefTable & objmap )
{
	const char* p = i->Resolve( objmap );

	PObject objNum = Parse( p );
	PObject objGen = Parse( p );

	if (objNum->Type() != ObjectType::Number || objGen->Type() != ObjectType::Number)
		DebugBreak();

	int num = ((Number *)objNum.get())->num;
	int gen = ((Number *)objGen.get())->num;

	if (i->objectNum != num || i->generation != gen)
		DebugBreak();

	char const * tokenStart = p;
	if (token_e::KeywordObj != Token( p, tokenStart ))
		DebugBreak();

	PObject o = Parse( p );

	// todo: attach dict to stream
	token t = Token( p, tokenStart );
	if ( t == token_e::KeywordEndObj )
		return o;
	else if ( t == token_e::Stream && o->Type() == ObjectType::Dictionary )
	{
		if( *p == '\r' )
			++p;
		if( *p != '\n' )
			DebugBreak();
		++p;

		PDictionary dict = boost::shared_static_cast<Dictionary>( o );
		Number* length = (Number*)Object::ResolveIndirect( dict->Get( "Length" ), objmap ).get();
		return PStream( new Stream( dict, p, p+length->num ) );
	}
	else
		DebugBreak();
}

PObject ParseIndirectObject( Indirect * i, const XrefTable & objmap )
{
	size_t objNum = (size_t)i->objectNum;
	if( objNum >= objmap.size() )
	{
		DebugBreak();
		return PObject();
	}

	const Xref& xref = objmap[ objNum ];

	if (!xref.cache)
		xref.cache = InnerParseIndirectObject( i, objmap );

	return xref.cache;
}

PObject Object::ResolveIndirect( PObject p, const XrefTable & objmap )
{
	if (!p || p->Type() != ObjectType::Ref)
		return p;

	return ParseIndirectObject( (Indirect *)p.get(), objmap );
}

void DumpPage( Dictionary * start, const XrefTable & objmap )
{
	PObject content = Object::ResolveIndirect( start->Get( "Contents" ), objmap );
	if( !content )
		return;

	if( content->Type() == ObjectType::Stream )
	{
		//PStream stream = boost::shared_static_cast<Stream>( content );

		//char filename[128];
		//_snprintf( filename, 128, "page%u.txt", stream->start );

		//HANDLE file = CreateFileA( filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		//DWORD numBytesWritten;
		//WriteFile( file, stream->start, stream->end - stream->start, &numBytesWritten, NULL );
		//CloseHandle( file );
	}
	else if( content->Type() == ObjectType::Array )
	{
		DebugBreak();
	}
	else
		//Invalid file
		DebugBreak();
}

void WalkPageTree( Dictionary * start, const XrefTable & objmap )
{
	Name * type = (Name *)Object::ResolveIndirect(start->Get( "Type" ), objmap).get();
	if (!type || type->Type() != ObjectType::Name)
		DebugBreak();

	if (String( "Page" ) == type->str)
	{
		DumpPage( start, objmap );
	}
	else
	{
		Array * children = (Array *)Object::ResolveIndirect( start->Get( "Kids" ), objmap ).get();
		if (!children || children->Type() != ObjectType::Array)
			DebugBreak();

		std::vector< PObject >::const_iterator it;
		for( it = children->elements.begin(); it != children->elements.end(); it++ )
		{
			Dictionary * child = (Dictionary *)Object::ResolveIndirect( *it, objmap ).get();
			WalkPageTree( child, objmap );
		}
	}
}

void ReadPdfTrailerSection( MappedFile const & f, XrefTable & objmap, char const * p, bool isTopLevel );

void WalkPreviousFileVersions( MappedFile const & f, XrefTable & t, Dictionary * d )
{
	PObject prev = d->Get( "Prev" );
	if (!prev || prev->Type() != ObjectType::Number)
		return;

	char const * p = f.F() + ((Number *)prev.get())->num;
	p = ReadPdfXrefSection( f, p, t );
	ReadPdfTrailerSection( f, t, p, false );
}

extern HTREEITEM AddOutlineItem( char const * start, size_t len, bool hasChildren, HTREEITEM parent, void * value );

void BuildOutline( Dictionary * parent, HTREEITEM parentItem, const XrefTable & objmap )
{
	Dictionary * item = (Dictionary *)Object::ResolveIndirect( parent->Get( "First" ), objmap ).get();
	
	while( item )
	{
		String * itemTitle = (String *)Object::ResolveIndirect( item->Get( "Title" ), objmap ).get();

		HTREEITEM treeItem = AddOutlineItem( 
			itemTitle->start, 
			itemTitle->end - itemTitle->start, 
			item->Get( "First" ) != 0, parentItem, 
			item );

		BuildOutline( item, treeItem, objmap );

		item = (Dictionary *)Object::ResolveIndirect( item->Get( "Next" ), objmap ).get();
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

	String * itemTitle = (String *)Object::ResolveIndirect( dict->Get( "Title" ), doc->xrefTable ).get();

	char sz[1024];
	memcpy( sz, itemTitle->start, itemTitle->Length() );
	sz[itemTitle->Length()] = 0;

	SetWindowTextA( appHwnd, sz );	// WTF, hax

	// todo: follow link

	PObject dest = Object::ResolveIndirect( dict->Get( "Dest" ), doc->xrefTable );
	if (!dest)
		SetWindowText( appHwnd, L"no dest key" );

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

void ReadPdfTrailerSection( MappedFile const & f, XrefTable & objmap, char const * p, bool isTopLevel )
{
	p = GetPdfNextLine( f, p );
	PObject trailerDictP = Parse( p );

	Dictionary * trailerDict = (Dictionary *)trailerDictP.get();

	WalkPreviousFileVersions( f, objmap, trailerDict );

	if (!isTopLevel)
		return;

	Dictionary * rootDict = (Dictionary *)Object::ResolveIndirect( trailerDict->Get( "Root" ), objmap ).get();
	if (!rootDict || rootDict->Type() != ObjectType::Dictionary)
	{
		MessageBox( 0, L"Root object fails", L"Fail", 0 );
		return;
	}

	Dictionary * outlineDict = (Dictionary *)Object::ResolveIndirect( rootDict->Get( "Outlines" ), objmap ).get();
	if (!outlineDict || outlineDict->Type() != ObjectType::Dictionary)
	{
		MessageBox( 0, L"No (or bogus) outline", L"fail", 0 );
		return;
	}

	BuildOutline( outlineDict, TVI_ROOT, objmap );
	
	Dictionary * pageTreeRoot = (Dictionary *)Object::ResolveIndirect( rootDict->Get( "Pages" ), objmap ).get();
	assert( pageTreeRoot );
	WalkPageTree( pageTreeRoot, objmap );
}

#define MsgBox( msg )\
	::MessageBox( appHwnd, msg, L"PDF Viewer", 0 )

Document * LoadFile( HWND appHwnd, wchar_t const * filename )
{
	MappedFile * f = new MappedFile( filename );
	if (!f->IsValid())
	{
		MsgBox( L"Failed opening file" );
		return 0;
	}

	int version = GetPdfVersion( *f );
	if (!version)
	{
		MsgBox( L"Not a PDF: bogus header" );
		return 0;
	}

	char const * eof = GetPdfEof( *f );
	if (!eof)
	{
		MsgBox( L"Not a PDF: bogus EOF" );
		return 0;
	}

	char const * xrefOffsetS = GetPdfPrevLine( *f, eof );
	char const * startXref = GetPdfPrevLine( *f, xrefOffsetS );

	if (::memcmp(startXref, "startxref", 9 ))
	{
		MsgBox( L"Bogus startxref" );
		return 0;
	}

	char * end = const_cast<char *>(eof);
	char const * xref = f->F() + ::strtol( xrefOffsetS, &end, 10 );
	if (!end || end == xrefOffsetS)
	{
		MsgBox( L"Bogus xref offset" );
		return 0;
	}

	if (::memcmp(xref, "xref", 4 ))
	{
		MsgBox( L"Bogus xref" );
		return 0;
	}

	Document * doc = new Document( f );

	char const * trailer = ReadPdfXrefSection( *f, xref, doc->xrefTable );
	if (!trailer)
	{
		MsgBox( L"Bogus xref section" );
		delete doc;
		return 0;
	}

	ReadPdfTrailerSection( *f, doc->xrefTable, trailer, true );

	/*wchar_t sz[512];
	wsprintf( sz, L"num xref objects: %u", doc->xrefTable->size() );
	MsgBox( sz );

	MsgBox( L"Got here" );	*/

	return doc;
}
