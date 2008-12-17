#include "pch.h"
#include "token.h"
#include "parse.h"

// hold references to the xrefstreams, so the parsed trailer dicts don't refer to free'd memory.
static std::vector< PStream > xrefStreams;

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

extern void WalkNumberTree( Document * doc, Dictionary * node, NumberTree& intoTree );

/*void LoadPageLabels( Document * doc )
{
	doc->pageLabels.clear();
	PDictionary pageLabelRoot = doc->documentCatalog->Get<Dictionary>( "PageLabels", doc->xrefTable );

	if (pageLabelRoot)
		WalkNumberTree( doc, pageLabelRoot.get(), doc->pageLabels );
}*/

PDictionary ReadPdfTrailerSection( MappedFile const & f, XrefTable & objmap, char const * p );

void WalkPreviousFileVersions( MappedFile const & f, XrefTable & objmap, PDictionary d )
{
	PNumber prev = d->Get<Number>( "Prev", objmap );
	if( !prev )
		return;

	char const * xref = f.F() + prev->num;
	//p = objmap.ReadXrefSection( f, p );
	ReadPdfTrailerSection( f, objmap, xref );
}

PDictionary ReadPdfTrailerSection( MappedFile const & f, XrefTable & objmap, char const * xref )
{
	if (::memcmp(xref, "xref", 4 ) == 0 )
	{
		xref = objmap.ReadXrefSection( xref );
		if( !xref )
			// Bogus xref section
			return PDictionary();
	}

	const char * p = GetPdfNextLine( f, xref );

	PObject trailerDictP = ParseDirect( p, objmap );

	PDictionary trailerDict = boost::shared_dynamic_cast<Dictionary>( trailerDictP );
	PStream xrefStream = boost::shared_dynamic_cast< Stream >( trailerDictP );

	if( xrefStream )
		trailerDict = xrefStream->dict;

	assert( trailerDict && "Invalid Trailer Dictionary" );

	PName type = trailerDict->Get<Name>( "Type", objmap );
	if( type && type->str == String( "XRef" ) )
	{
		// xref stream
		if( !xrefStream )
		{
			assert( !"bogus xref stream" );
			return PDictionary();
		}

		xrefStreams.push_back( xrefStream );
		size_t streamLength;
		const char* streamContent = xrefStream->GetStreamBytes( XrefTable( f ), &streamLength );
		objmap.ReadXrefStream( xrefStream, streamContent, streamLength );
	}

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

PDocument LoadFile( HWND appHwnd, wchar_t const * filename )
{
	MappedFile * f = new MappedFile( filename );
	if (!f->IsValid())
	{
		MsgBox( L"Failed opening file" );
		return PDocument();
	}

	int version = GetPdfVersion( *f );
	if (!version)
	{
		MsgBox( L"Not a PDF: bogus header" );
		return PDocument();
	}

	char const * eof = GetPdfEof( *f );
	if (!eof)
	{
		MsgBox( L"Not a PDF: bogus EOF" );
		return PDocument();
	}

	char const * xrefOffsetS = GetPdfPrevLine( *f, eof );
	char const * startXref = GetPdfPrevLine( *f, xrefOffsetS );

	if (::memcmp(startXref, "startxref", 9 ))
	{
		MsgBox( L"Bogus startxref" );
		return PDocument();
	}

	char * end = const_cast<char *>(eof);
	char const * xref = f->F() + ::strtol( xrefOffsetS, &end, 10 );
	if (!end || end == xrefOffsetS)
	{
		MsgBox( L"Bogus xref offset" );
		return PDocument();
	}

	PDocument doc( new Document( f ) );

	doc->outlineRoot = ReadTopLevelTrailer( doc.get(), *f, doc->xrefTable, xref );

	PDictionary nameDict = doc->documentCatalog->Get<Dictionary>( "Names", doc->xrefTable );
	doc->nameTreeRoot = nameDict ? nameDict->Get<Dictionary>( "Dests", doc->xrefTable ) : PDictionary();
//	LoadNamedDestinations( doc );

	return doc;
}
