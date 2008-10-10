#pragma once

typedef std::map< size_t, PObject > NumberTree;
typedef std::map< String, PObject > NameTree;

class Document
{
public:
	XrefTable xrefTable;
	
	PDictionary documentCatalog;
	PDictionary pageRoot;
	PDictionary outlineRoot;
	PDictionary nameTreeRoot;

	MappedFile * f;
	NumberTree pageLabels;
	NameTree namedDestinations;

	Document( MappedFile * g )
		: f(g)
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
};

double ToNumber( PObject obj );	// needs a home!
