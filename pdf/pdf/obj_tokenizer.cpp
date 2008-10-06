#include "pch.h"
#include "token.h"

int Parse( const char*& p, const char *& end )
{
	for(;;)
	{
		switch( *p )
		{
		case ' ':
		case '\n':
		case '\r':
		case '\t':
		case '\v':
			++p;
			continue;
		case '%':
			while( *p != '\n' && *p != '\r' )
				++p;
			continue;
		case '<':
			if( p[1] == '<' )
			{
				end = p + 2;
				return token_e::DictStart;
			}
			
			end = p;
			while( *end++ != '>' ) {}
			return token_e::HexString;

		case '>':
			if( p[1] == '>' )
			{
				end = p + 2;
				return token_e::DictEnd;
			}
			DebugBreak();	// noti
		case '/':
			end = ++p;
			while( !memchr( "\t\r\v\n (){}[]<>/?%", *end, 17 ) )
				++end;
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
			end = p;
			while( memchr( ".0123456789", *end, 11 ) )
				++end;
			return token_e::NumberInt;

		case '(':
		case '[':
		case '{':
		case '?':
		case ']':
		case ')':
		case '}':
			// TODO
			end = p + 1;
			return token_e::Unknown;

		default:
			end = p;
			while( !memchr( "\t\r\v\n (){}[]<>/?%", *end, 17 ) )
				++end;
			return token_e::KeywordObj;	// TODO: the right keyword token
		}
	}
}
