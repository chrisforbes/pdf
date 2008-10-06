#include "pch.h"
#include "token.h"

token Token( const char*& p, const char *& /*out*/ tokenStart )
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
			while( memchr( ".0123456789", *p, 11 ) )
				++p;
			return token_e::NumberInt;

		case '[':
			++p;
			return token_e::ArrayStart;
		case ']':
			++p;
			return token_e::ArrayEnd;
		case '{':
		case '}':
		case '(':
		case ')':
		case '?':
			// TODO
			++p;
			return token_e::Unknown;

		case 'R':
			++p;
			return token_e::Ref;

		default:
			while( !memchr( "\t\r\v\n (){}[]<>/?%", *p, 17 ) )
				++p;
			return token_e::KeywordObj;	// TODO: the right keyword token
		}
	}
}
