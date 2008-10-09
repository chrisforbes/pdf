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
};
