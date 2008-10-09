#include "pch.h"
#include "token.h"
#include "parse.h"

PObject Parse( const char*& p )
{
	const char* tokenStart = 0;
	token t = Token( p, tokenStart );
	switch( t )
	{
	case token_e::DictStart:
		return ParseDict( p );
	case token_e::NumberInt:
		{
			Number ret( tokenStart, p );
			const char* lookahead = p;
			if( Token( lookahead, tokenStart ) == token_e::NumberInt )
			{
				Number generation( tokenStart, lookahead );
				if( Token( lookahead, tokenStart ) == token_e::Ref )
				{
					p = lookahead;
					return PObject( new Indirect( ret, generation ) );
				}
			}
			return PObject( new Number( ret ) );
		}
	case token_e::ArrayStart:
		return ParseArray( p );

	case token_e::HexString:
		// TODO: actually parse this thing
		return PString();

	case token_e::String:
		return PString( new String( tokenStart, p ) );

	case token_e::True:
		return PBool( new Bool( true ) );
	case token_e::False:
		return PBool( new Bool( false ) );

	case token_e::Name:
		return PName( new Name( String( tokenStart, p ) ) );

	case token_e::NumberDouble:
		return PDouble( new Double( tokenStart, p ) );

	default:
		return PObject();
	}
}

PDictionary ParseDict( const char*& p )
{
	Dictionary* dict = new Dictionary();
	const char* tokenStart = 0;
	for(;;)
	{
		token t = Token( p, tokenStart );
		switch( t )
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

PArray ParseArray( const char*& p )
{
	PArray array( new Array() );

	for(;;)
	{
		char const * q = p;
		const char* tokenStart = 0;
		if( Token( p, tokenStart ) == token_e::ArrayEnd )
			return array;
		p = q;
		array->Add( Parse( p ) );
	}
}