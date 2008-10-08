#include "pch.h"
#include "token.h"
#include "parse.h"

#include "commctrl.h"

// todo: awesome joke somewhere about RDF = reality distortion field

class MappedFile
{
	HANDLE hFile;
	HANDLE hMapping;
	char const * f;
	size_t size;

public:
	MappedFile( wchar_t const * filename )
		: hFile( INVALID_HANDLE_VALUE ), hMapping( INVALID_HANDLE_VALUE ), f(0)
	{
		hFile = ::CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
		if (hFile == INVALID_HANDLE_VALUE)
			return;

		DWORD sizeHigh;
		size = ::GetFileSize( hFile, &sizeHigh );

		if (sizeHigh)
			return;

		hMapping = ::CreateFileMapping( hFile, 0, PAGE_READONLY, 0, size, 0 );
		if (hMapping == INVALID_HANDLE_VALUE)
			return;

		f = (char const *)::MapViewOfFile( hMapping, FILE_MAP_READ, 0, 0, 0 );
	}

	bool IsValid() const
	{
		return f != 0;
	}

	char const * F() const
	{
		return f;
	}

	size_t Size() const 
	{
		return size;
	}

	~MappedFile()
	{
		if (f)
			::UnmapViewOfFile( f );
		if (hMapping != INVALID_HANDLE_VALUE)
			::CloseHandle( hMapping );
		if (hFile != INVALID_HANDLE_VALUE)
			::CloseHandle( hFile );
	}
};

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

char const * ReadPdfXrefSection( MappedFile const & f, char const * p, XrefTable& m )
{
	p = GetPdfNextLine(f,p);	// `xref` line

	size_t currentObjIndex = 0;

	for(;;)
	{
		if (!::memcmp( p, "trailer", 7 ))
			return p;	// we're done

		char const * nextLine = GetPdfNextLine(f,p);
		char * q = const_cast<char *>(nextLine);

		if (*p == '\r' || *p == '\n')
		{
			p = nextLine;
			continue;
		}

		size_t a = ::strtol( p, &q, 10 );
		if (!q || p == q)
			return 0;

		p = q;
		q = const_cast<char *>(nextLine);

		size_t b = ::strtol( p, &q, 10 );
		if (!q || p == q)
			return 0;

		p = q;
		while( *p == ' ' ) ++p;		// eat the spaces

		if (*p == '\r' || *p == '\n')
		{
			currentObjIndex = a;
		}
		else
		{
			size_t objIndex = currentObjIndex++;
			
			Xref& obj = m[objIndex];
			
			if (obj.generation > b)
				continue;

			obj.ptr = *p == 'f' ? 0 : f.F() + a;
			obj.generation = b;
		}

		p = nextLine;
	}
}

PObject InnerParseIndirectObject( Indirect * i, char const * p )
{
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

	if (token_e::KeywordEndObj != Token( p, tokenStart ))
		DebugBreak();

	return o;
}

PObject ParseIndirectObject( Indirect * i, char const * p, XrefTable & objmap )
{
	XrefTable::iterator it = objmap.find( i->objectNum );
	// `it` is guaranteed to exist if we're here

	if (it == objmap.end())
		return PObject();

	if (!it->second.cache)
		it->second.cache = InnerParseIndirectObject( i, p );

	return it->second.cache;
}

PObject Object::ResolveIndirect( PObject p, XrefTable &t )
{
	if (!p || p->Type() != ObjectType::Ref)
		return p;

	Indirect * i = (Indirect *)p.get();
	char const * ptr = i->Resolve( t );

	return ParseIndirectObject( i, ptr, t );
}

static size_t physPageNumber;

void WalkPageTree( Dictionary * start, XrefTable & t )
{
	Name * type = (Name *)Object::ResolveIndirect(start->Get( "Type" ), t).get();
	if (!type || type->Type() != ObjectType::Name)
		DebugBreak();

	if (String( "Page" ) == type->str)
	{
		// leaf
		char sz[64];
		sprintf( sz, "Physical page %u\n", ++physPageNumber );
		OutputDebugStringA( sz );
	}
	else
	{
		Array * children = (Array *)Object::ResolveIndirect( start->Get( "Kids" ), t ).get();
		if (!children || children->Type() != ObjectType::Array)
			DebugBreak();

		std::vector< PObject >::const_iterator it;
		for( it = children->elements.begin(); it != children->elements.end(); it++ )
		{
			Dictionary * child = (Dictionary *)Object::ResolveIndirect( *it, t ).get();
			WalkPageTree( child, t );
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

extern HTREEITEM AddOutlineItem( char const * start, size_t len, bool hasChildren, HTREEITEM parent );

void BuildOutline( Dictionary * parent, HTREEITEM parentItem, XrefTable & objmap )
{
	Dictionary * item = (Dictionary *)Object::ResolveIndirect( parent->Get( "First" ), objmap ).get();
	
	while( item )
	{
		String * itemTitle = (String *)Object::ResolveIndirect( item->Get( "Title" ), objmap ).get();

		HTREEITEM treeItem = AddOutlineItem( 
			itemTitle->start, 
			itemTitle->end - itemTitle->start, 
			item->Get( "First" ) != 0, parentItem );

		BuildOutline( item, treeItem, objmap );

		item = (Dictionary *)Object::ResolveIndirect( item->Get( "Next" ), objmap ).get();
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
	//WalkPageTree( pageTreeRoot, objmap );
}

#define MsgBox( msg )\
	::MessageBox( appHwnd, msg, L"PDF Viewer", 0 )

void LoadFile( HWND appHwnd, wchar_t const * filename )
{
	MappedFile f( filename );
	if (!f.IsValid())
	{
		MsgBox( L"Failed opening file" );
		return;
	}

	int version = GetPdfVersion( f );
	if (!version)
	{
		MsgBox( L"Not a PDF: bogus header" );
		return;
	}

	char const * eof = GetPdfEof( f );
	if (!eof)
	{
		MsgBox( L"Not a PDF: bogus EOF" );
		return;
	}

	char const * xrefOffsetS = GetPdfPrevLine( f, eof );
	char const * startXref = GetPdfPrevLine( f, xrefOffsetS );

	if (::memcmp(startXref, "startxref", 9 ))
	{
		MsgBox( L"Bogus startxref" );
		return;
	}

	char * end = const_cast<char *>(eof);
	char const * xref = f.F() + ::strtol( xrefOffsetS, &end, 10 );
	if (!end || end == xrefOffsetS)
	{
		MsgBox( L"Bogus xref offset" );
		return;
	}

	if (::memcmp(xref, "xref", 4 ))
	{
		MsgBox( L"Bogus xref" );
		return;
	}

	XrefTable objmap;

	char const * trailer = ReadPdfXrefSection( f, xref, objmap );
	if (!trailer)
	{
		MsgBox( L"Bogus xref section" );
		return;
	}

	ReadPdfTrailerSection( f, objmap, trailer, true );

	wchar_t sz[512];
	wsprintf( sz, L"num xref objects: %u", objmap.size() );
	MsgBox( sz );

	MsgBox( L"Got here" );
}
