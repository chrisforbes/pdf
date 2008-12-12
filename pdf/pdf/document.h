#pragma once

typedef std::map< size_t, PObject > NumberTree;

class Document
{
public:
	XrefTable xrefTable;
	
	PDictionary documentCatalog;
	PDictionary pageRoot;
	PDictionary outlineRoot;
	PDictionary nameTreeRoot;

	MappedFile * f;
//	NumberTree pageLabels;

	Document( MappedFile * g )
		: f(g), xrefTable( *g )
	{
	}

	~Document()
	{
		if (f)
			delete f;
	}

	PDictionary GetPage( size_t n );
	size_t GetPageIndex( PDictionary page );
	PDictionary GetNextPage( PDictionary page );
	PDictionary GetPrevPage( PDictionary page );
	size_t GetPageCount();
};
typedef boost::shared_ptr< Document > PDocument;

double ToNumber( PObject obj );	// needs a home!
