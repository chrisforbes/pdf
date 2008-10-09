#include "pch.h"
#include "token.h"
#include "parse.h"

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
		PNumber length = dict->Get<Number>( "Length", objmap );
		return PStream( new Stream( dict, p, p+length->num ) );
	}
	else
	{
		DebugBreak();
		return PObject();
	}
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

PObject Object::ResolveIndirect_( PObject p, const XrefTable & objmap )
{
	if (!p || p->Type() != ObjectType::Ref)
		return p;

	return ParseIndirectObject( (Indirect *)p.get(), objmap );
}

void DumpPage( Dictionary * start, const XrefTable & objmap )
{
	PObject content = start->Get( "Contents", objmap );
	if( !content )
		return;

	if( content->Type() == ObjectType::Stream )
	{
		PStream contentS = boost::shared_static_cast<Stream>( content );

		const char* decodedPage = contentS->GetStreamBytes( objmap );
	}
	else if( content->Type() == ObjectType::Array )
	{
		//DebugBreak();
	}
	else
		//Invalid file
		DebugBreak();
}

void WalkPageTree( const PDictionary& start, const XrefTable & objmap )
{
	PName type = start->Get<Name>( "Type", objmap );
	if (!type)
		DebugBreak();

	if (String( "Page" ) == type->str)
	{
		//DumpPage( start.get(), objmap );
	}
	else
	{
		PArray children = start->Get<Array>( "Kids", objmap );
		if (!children)
			DebugBreak();

		std::vector< PObject >::const_iterator it;
		for( it = children->elements.begin(); it != children->elements.end(); it++ )
		{
			PDictionary child = boost::shared_static_cast<Dictionary>( Object::ResolveIndirect_( *it, objmap ) );
			WalkPageTree( child, objmap );
		}
	}
}

extern void WalkNumberTree( Document * doc, Dictionary * node, NumberTree& intoTree );

void LoadPageLabels( Document * doc )
{
	doc->pageLabels.clear();
	PDictionary pageLabelRoot = doc->documentCatalog->Get<Dictionary>( "PageLabels", doc->xrefTable );

	if (pageLabelRoot)
		WalkNumberTree( doc, pageLabelRoot.get(), doc->pageLabels );
}

extern void WalkNamedDestinationsTree( Document * doc, Dictionary * node, NameTree & intoTree );

void LoadNamedDestinations( Document * doc )
{
	doc->namedDestinations.clear();
	PDictionary dests = doc->documentCatalog->Get<Dictionary>( "Dests", doc->xrefTable );

	if (dests)
		WalkNamedDestinationsTree( doc, dests.get(), doc->namedDestinations );
}

PDictionary ReadPdfTrailerSection( MappedFile const & f, XrefTable & objmap, char const * p );

void WalkPreviousFileVersions( MappedFile const & f, XrefTable & t, PDictionary d )
{
	PObject prev = d->Get( "Prev" );
	if (!prev || prev->Type() != ObjectType::Number)
		return;

	char const * p = f.F() + ((Number *)prev.get())->num;
	p = ReadPdfXrefSection( f, p, t );
	ReadPdfTrailerSection( f, t, p );
}

PDictionary ReadPdfTrailerSection( MappedFile const & f, XrefTable & objmap, char const * p )
{
	p = GetPdfNextLine( f, p );
	PObject trailerDictP = Parse( p );

	PDictionary trailerDict = boost::shared_static_cast<Dictionary>( trailerDictP );

	WalkPreviousFileVersions( f, objmap, trailerDict );
	return trailerDict;
}

PDictionary ReadTopLevelTrailer( Document * doc, MappedFile const & f, XrefTable & objmap, char const * p )
{
	PDictionary trailerDict = ReadPdfTrailerSection( f, objmap, p );

	PDictionary rootDict = trailerDict->Get<Dictionary>( "Root", objmap );
	if (!rootDict)
	{
		MessageBox( 0, L"Root object fails", L"Fail", 0 );
		return PDictionary();
	}

	PDictionary outlineDict = rootDict->Get<Dictionary>( "Outlines", objmap );
	if (!outlineDict)
	{
		MessageBox( 0, L"No (or bogus) outline", L"fail", 0 );
		return PDictionary();
	}

	PDictionary pageTreeRoot = rootDict->Get<Dictionary>( "Pages", objmap );
	assert( pageTreeRoot );
	WalkPageTree( pageTreeRoot, objmap );

	
	doc->documentCatalog = rootDict;

	return outlineDict;
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

	doc->outlineRoot = ReadTopLevelTrailer( doc, *f, doc->xrefTable, trailer );

	LoadPageLabels( doc );
	LoadNamedDestinations( doc );

	/*wchar_t sz[512];
	wsprintf( sz, L"num xref objects: %u", doc->xrefTable->size() );
	MsgBox( sz );

	MsgBox( L"Got here" );	*/

	return doc;
}
