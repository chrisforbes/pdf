#include "pch.h"
#include "token.h"
#include "parse.h"

PObject Parse( const char*& p )
{
	const char* tokenStart = 0;
	switch( Token( p, tokenStart ) )
	{
	case token_e::DictStart:
		return ParseDict( p );
	case token_e::NumberInt:
		return PNumber( new Number( tokenStart, p ) );
	default:
		return boost::shared_ptr<Object>();
	}
}

PDictionary ParseDict( const char*& p )
{
	Dictionary* dict = new Dictionary();
	const char* tokenStart = 0;
	for(;;)
	{
		switch( Token( p, tokenStart ) )
		{
		case token_e::Name:
			{
				Name name = String( tokenStart, p );
				dict->Add( name, Parse( p ) );
				break;
			}
		case token_e::DictEnd:
			return PDictionary( dict );
		default:
			return PDictionary();
		}
	}
}
