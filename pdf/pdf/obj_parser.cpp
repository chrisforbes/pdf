#include "pch.h"
#include "token.h"
#include "parse.h"

PObject Parse( const char*& start )
{
	const char* end = 0;
	switch( Token( start, end ) )
	{
	case token_e::DictStart:
		return ParseDict( start = end );
	default:
		return boost::shared_ptr<Object>();
	}
}

PDictionary ParseDict( const char*& start )
{
	Dictionary* dict = new Dictionary();
	const char* end = 0;
	for(;;)
	{
		switch( Token( start, end ) )
		{
		case token_e::Name:
			{
				Name name = String( start, end );
				start = end;
				PObject value = Parse( start );
				dict->Add( name, value );
				break;
			}
		case token_e::DictEnd:
			{
				start = end;
				return PDictionary( dict );
			}
		default:
			{
				return PDictionary();
			}
		}
	}
}
