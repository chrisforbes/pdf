#pragma once

class Document
{
public:
	XrefTable xrefTable;
	PDictionary pageRoot;
	PDictionary outlineRoot;
	MappedFile * f;

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
