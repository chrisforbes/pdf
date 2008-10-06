#pragma once

namespace token_e
{
enum token_e
{
	NumberInt,
	NumberDouble,
	KeywordObj,
	KeywordEndObj,
	Null,
	Stream,
	EndStream,
	Xref,
	DictStart,
	DictEnd,
	ArrayStart,
	ArrayEnd,
	Name,
	True,
	False,
	String,
	Ref,
	Unknown,
	HexString,
};
}
//using token_e::token_e;

extern int Parse( const char*& p, const char *& end );