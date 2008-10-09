#pragma once

typedef std::map< size_t, PObject > NumberTree;

class Document
{
public:
	XrefTable xrefTable;
	
	PDictionary documentCatalog;
	PDictionary pageRoot;
	PDictionary outlineRoot;

	MappedFile * f;
	NumberTree pageLabels;

	Document( MappedFile * f )
		: f(f)
	{
	}

	~Document()
	{
		if (f)
			delete f;
	}
};
