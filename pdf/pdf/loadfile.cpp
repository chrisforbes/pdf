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
	assert( sizeof( Xref ) < 20 );
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

	m.addSection( first, count, p );

	size_t end = first + count;

	for( ; first < end ; first++ )
	{
		m.fillEntry( p, file );

		p += 20;
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

	PObject objNum = Parse( p, 0 );
	PObject objGen = Parse( p, 0 );

	if (objNum->Type() != ObjectType::Number || objGen->Type() != ObjectType::Number)
		DebugBreak();

	int num = ((Number *)objNum.get())->num;
	int gen = ((Number *)objGen.get())->num;

	if (i->objectNum != num || i->generation != gen)
		DebugBreak();

	char const * tokenStart = p;
	if (token_e::KeywordObj != Token( p, tokenStart, 0 ))
		DebugBreak();

	PObject o = Parse( p, 0 );

	token t = Token( p, tokenStart, 0 );
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

	const Xref* xref = objmap.find( objNum );
	if( xref )
	{
		if (!xref->cache)
			xref->cache = InnerParseIndirectObject( i, objmap );

		return xref->cache;
	}
	return PObject();
}

PObject Object::ResolveIndirect_( PObject p, const XrefTable & objmap )
{
	if (!p || p->Type() != ObjectType::Ref)
		return p;

	return ParseIndirectObject( (Indirect *)p.get(), objmap );
}

extern void WalkNumberTree( Document * doc, Dictionary * node, NumberTree& intoTree );

/*void LoadPageLabels( Document * doc )
{
	doc->pageLabels.clear();
	PDictionary pageLabelRoot = doc->documentCatalog->Get<Dictionary>( "PageLabels", doc->xrefTable );

	if (pageLabelRoot)
		WalkNumberTree( doc, pageLabelRoot.get(), doc->pageLabels );
}*/

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
	PObject trailerDictP = Parse( p, 0 );

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

	PDictionary pageTreeRoot = rootDict->Get<Dictionary>( "Pages", objmap );
	assert( pageTreeRoot );

	doc->pageRoot = pageTreeRoot;
	doc->documentCatalog = rootDict;

	PDictionary outlineDict = rootDict->Get<Dictionary>( "Outlines", objmap );
	if (!outlineDict)
	{
		return PDictionary();
	}

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

	PDictionary nameDict = doc->documentCatalog->Get<Dictionary>( "Names", doc->xrefTable );
	doc->nameTreeRoot = nameDict ? nameDict->Get<Dictionary>( "Dests", doc->xrefTable ) : PDictionary();
//	LoadNamedDestinations( doc );

	return doc;
}
