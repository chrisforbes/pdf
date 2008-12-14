#include "pch.h"
#include "token.h"
#include "parse.h"

PObject Parse( const char*& p, char const * end )
{
	const char* tokenStart = 0;
	token t = Token( p, tokenStart, end );
	switch( t )
	{
	case token_e::DictStart:
		return ParseDict( p, end );
	case token_e::NumberInt:
		{
			Number ret( tokenStart, p );
			const char* lookahead = p;
			if( Token( lookahead, tokenStart, end ) == token_e::NumberInt )
			{
				Number generation( tokenStart, lookahead );
				if( Token( lookahead, tokenStart, end ) == token_e::Ref )
				{
					p = lookahead;
					return PObject( new Indirect( ret, generation ) );
				}
			}
			return PObject( new Number( ret ) );
		}
	case token_e::ArrayStart:
		return ParseArray( p, end );

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

PDictionary ParseDict( const char*& p, char const * end )
{
	PDictionary dict( new Dictionary() );
	const char* tokenStart = 0;
	for(;;)
	{
		token t = Token( p, tokenStart, end );
		switch( t )
		{
		case token_e::Name:
			{
				Name name = String( tokenStart, p );
				dict->Add( name, Parse( p, end ) );
				break;
			}
		case token_e::DictEnd:
			return dict;
		default:
			return PDictionary();
		}
	}
}

PArray ParseArray( const char*& p, char const * end )
{
	PArray array( new Array() );

	for(;;)
	{
		char const * q = p;
		const char* tokenStart = 0;
		if( Token( p, tokenStart, end ) == token_e::ArrayEnd )
			return array;
		p = q;
		array->Add( Parse( p, end ) );
	}
}

// returns the operator
String ParseContent( char const *& p, char const * end, std::vector<PObject>& intoVector )
{
	while( p < end )
	{
		const char * q = p;
		const char * r = p;
		token t = Token( q, r, end );

		if (q > end || r > end)
			DebugBreak();

		if (t == token_e::Keyword)
		{
			p = q;
			return String( r, q );
		}

		intoVector.push_back( Parse( p, end ) );
	}

	// i have no idea what this funtion should have returned here, but it used to fall through
//	assert( !"WTF" );
}

size_t UnescapeString( char * dest, char const * src, char const * srcend )
{
	char * destStart = dest;

	while( src < srcend )
	{
		if (*src == '\\')
		{
			++src;
			switch( *src )
			{
			case 'n': *dest++ = '\n'; break;
			case 'r': *dest++ = '\r'; break;
			case 't': *dest++ = '\t'; break;
			case 'b': if (dest > destStart) dest--; break;	/* todo: dont fuck this up */
			case 'f': *dest++ = '\f'; break;
			case '(': *dest++ = '('; break;
			case ')': *dest++ = ')'; break;
			case '\\': *dest++ = '\\'; break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				{
					char temp[3], *pt = temp;

					char * p = (char *)src;
					while( src < p + 3 )
					{
						*pt++ = *src++;
						if (*src < '0' || *src > '7')
							break;
					}

					*dest++ = (char)strtol( temp, &p, 8 );
					--src;
				}
				break;
			default:
				*dest++ = *src;
			}
		}
		else
			*dest++ = *src;

		++src;
	}

	return dest - destStart;
}

PObject ParseDirect( const char* p, const XrefTable& objmap )
{
	PObject o = Parse( p, 0 );

	char const * tokenStart;
	token t = Token( p, tokenStart, 0 );

	if ( t == token_e::KeywordEndObj )
		return o;
	else if (t == token_e::KeywordStartXref )
		return o;
	else if ( t == token_e::Stream && o->Type() == ObjectType::Dictionary )
	{
		if( *p == '\r' )
			++p;
		if( *p != '\n' )
			DebugBreak();
		++p;

		PDictionary dict = boost::shared_static_cast<Dictionary>( o );
		PNumber length = dict->Get<Number>( "Length", objmap );

		const char* q = p;
		p += length->num;
		PStream ret( new Stream( dict, q, p ) );

		if( token_e::EndStream != Token( p, tokenStart, 0 ) )
			DebugBreak();
		if( token_e::KeywordEndObj != Token( p, tokenStart, 0 ) )
			DebugBreak();

		if( *p == '\r' )
			++p;
		if( *p == '\n' )
			++p;

		return ret;
	}
	else
	{
		DebugBreak();
		return PObject();
	}
}

PObject ParseIndirect( const char* p, Indirect* i, const XrefTable& objmap )
{
	PObject objNum = Parse( p, 0 );
	PObject objGen = Parse( p, 0 );

	if (objNum->Type() != ObjectType::Number || objGen->Type() != ObjectType::Number)
		DebugBreak();

	int indNum = ((Number *)objNum.get())->num;
	int indGen = ((Number *)objGen.get())->num;

	if (i->objectNum != indNum || i->generation != indGen)
		DebugBreak();

	char const * tokenStart = p;
	if (token_e::KeywordObj != Token( p, tokenStart, 0 ))
		DebugBreak();

	return ParseDirect( p, objmap );

}
