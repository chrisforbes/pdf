#include "pch.h"
#include "contentStreamElement.h"

#define NOTI assert( !"NOTI" )

static void skipws( const char *& p, const char* end )
{
	while( p < end && isspace( *p ) )
		++p;
}

static bool parseDictionary( const char* s, const char* e, ContentStreamElement& elem )
{
	while( true )
	{
		const char* tokenStart;
		token t = Token( s, tokenStart, e );
		switch( t )
		{
		case token_e::Name:
			{
				ContentStreamElement* newElem = new ContentStreamElement();
				newElem->keyStart = tokenStart;
				newElem->keyEnd = s;
				newElem->next = elem.content;
				elem.content = newElem;

				if( !newElem->parse( s, e ) )
				{
					NOTI;
					elem.reset();
					return false;
				}
				s = newElem->end;
				break;
			}
		case token_e::DictEnd:
			elem.end = s;
			return true;
		default:
			NOTI;
			elem.reset();
			return false;
		}
	}
}

static bool parseArray( const char* s, const char* e, ContentStreamElement& elem )
{
	ContentStreamElement** nextPtr = &elem.content;
	while( true )
	{
		skipws( s, e );
		if( s >= e )
		{
			elem.reset();
			return false;
		}
		if( *s == ']' )
		{
			elem.end = s+1;
			return true;
		}
		ContentStreamElement* newElem = new ContentStreamElement();
		*nextPtr = newElem;
		nextPtr = &newElem->next;
		if( !newElem->parse( s, e ) )
		{
			NOTI;
			elem.reset();
			return false;
		}
		s = newElem->end;
	}
}

bool ContentStreamElement::parse( const char* s, const char* e )
{
	skipws( s, e );
	if( s >= e )
		return false;

	start = s;
	end = NULL;

	token t = Token( s, start, e );
	switch( t )
	{
	case token_e::DictStart:
		type = Elem_Dictionary;
		return parseDictionary( s, e, *this );
	case token_e::ArrayStart:
		type = Elem_Array;
		return parseArray( s, e, *this );
	case token_e::Keyword:
		end = s;
		type = Elem_Operator;
		return true;
	case token_e::Name:
		end = s;
		type = Elem_Name;
		return true;
	case token_e::String:
		end = s + 1;
		type = Elem_String;
		return true;
	case token_e::HexString:
		end = s;
		type = Elem_HexString;
		return true;
	case token_e::NumberInt:
	case token_e::NumberDouble:
		end = s;
		type = Elem_Number;
		return true;
	default:
		NOTI;
		reset();
		return false;
	}
}

static ContentStreamElement* alloc_cache = NULL;

void* ContentStreamElement::operator new( size_t n )
{
	if( alloc_cache )
	{
		ContentStreamElement* ret = alloc_cache;
		alloc_cache = alloc_cache->next;
		return ret;
	}
	else
		return ::new ContentStreamElement();
}

void ContentStreamElement::operator delete( void* ptr )
{
	ContentStreamElement* p = (ContentStreamElement*)ptr;
#ifdef DEBUG
	ContentStreamElement* k = alloc_cache;
	while( k )
	{
		assert( k != p );
		k = k->next;
	}
#endif
	p->next = alloc_cache;
	alloc_cache = p;
}
