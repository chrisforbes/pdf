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
typedef token_e::token_e token;

extern token Token( const char*& p, const char *& start );