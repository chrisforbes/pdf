#pragma once
#include "../pdf/types.h"
#include "../pdf/token.h"

struct ContentStreamElement
{
	enum ContentElementType
	{
		Elem_Nothing = 0,
		Elem_Array,
		Elem_Dictionary,
		Elem_String,
		Elem_Name,
		Elem_HexString,
		Elem_Number,
		Elem_Operator,
		Elem_Ref,
	} type;

	ContentStreamElement* content;
	ContentStreamElement* next;
	const char* start;
	const char* end;
	const char* keyStart;
	const char* keyEnd;

	ContentStreamElement()
		: type( Elem_Nothing ), content( NULL ), next( NULL ), start( NULL ), end( NULL ), keyStart( NULL ), keyEnd( NULL )
	{
	}

private:
	ContentStreamElement( ContentStreamElement const& cse );
	void operator=( ContentStreamElement const & cse );

public:
	~ContentStreamElement()
	{
		reset();
	}

	void reset()
	{
		if( content )
			delete content;
		if( next )
			delete next;
		content = 0;
		next = 0;
		start = 0;
		end = 0;
		keyStart = 0;
		keyEnd = 0;
	}

	bool parse( const char* s, const char* e );

	String ToString() const
	{
		switch( type )
		{
		case Elem_String:
			return String( start, end-1 );
		default:
			return String( start, end );
		}
	}

	void* operator new( size_t n );
	void operator delete( void* ptr );
};
