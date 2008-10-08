#include "pch.h"
#include "token.h"

static token InnerToken( const char*& p, const char *& /*out*/ tokenStart )
{
	for(;;)
	{
		tokenStart = p;
		switch( *p )
		{
		case ' ':
		case '\n':
		case '\r':
		case '\t':
		case '\v':
		case ')':
			++p;
			continue;
		case '%':
			while( *p != '\n' && *p != '\r' )
				++p;
			continue;
		case '<':
			if( p[1] == '<' )
			{
				p += 2;
				return token_e::DictStart;
			}
			
			while( *p++ != '>' ) {}
			return token_e::HexString;

		case '>':
			if( p[1] == '>' )
			{
				p += 2;
				return token_e::DictEnd;
			}
			DebugBreak();	// noti
		case '/':
			tokenStart = ++p;
			while( !memchr( "\t\r\v\n (){}[]<>/?%", *p, 17 ) )
				++p;
			return token_e::Name;

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '.':
		case '+':
		case '-':
			++p;
			while( memchr( "0123456789", *p, 10 ) )
				++p;

			if( *p != '.' )
				return token_e::NumberInt;

			++p;

			while( memchr( "0123456789", *p, 10 ) )
				++p;
			return token_e::NumberDouble;
		case '[':
			++p;
			return token_e::ArrayStart;
		case ']':
			++p;
			return token_e::ArrayEnd;

		case '(':
			tokenStart = ++p;
			{
				int nparens = 1;
				while( nparens )
				{
					switch( *p )
					{
					case '(':
						++nparens;
						break;
					case ')':
						--nparens;
						break;
					case '\\':
						++p;
						break;
					}
					++p;
				}
				--p;
				return token_e::String;
			}

		case '{':
		case '}':
		case '?':
			// TODO
			++p;
			return token_e::Unknown;

		default:
			while( !memchr( "\t\r\v\n (){}[]<>/?%", *p, 17 ) )
				++p;
			return token_e::Keyword;
		}
	}
}

#define MATCH_KEYWORD( k, tok )\
	if (p - start == sizeof(k) - 1)\
		if (memcmp( start, k, sizeof(k) - 1 ) == 0)\
			return tok;

token Token( const char *& p, const char *& start )
{
	token t = InnerToken( p, start );
	if (t == token_e::Keyword)
	{
		MATCH_KEYWORD( "R", token_e::Ref );
		MATCH_KEYWORD( "obj", token_e::KeywordObj );
		MATCH_KEYWORD( "endobj", token_e::KeywordEndObj );
		MATCH_KEYWORD( "stream", token_e::Stream );
		MATCH_KEYWORD( "endstream", token_e::EndStream );
		MATCH_KEYWORD( "true", token_e::True );
		MATCH_KEYWORD( "false", token_e::False );
		MATCH_KEYWORD( "null", token_e::Null );

		DebugBreak();
	}

	return t;
}