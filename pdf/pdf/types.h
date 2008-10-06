#pragma once

class Object;
typedef boost::shared_ptr<Object> PObject;

//class Array;
//class Bool;
class Dictionary;
typedef boost::shared_ptr<Dictionary> PDictionary;
class Name;
typedef boost::shared_ptr<Name> PName;
//class Number;
//class Stream;
class String;
typedef boost::shared_ptr<String> PString;

class Object
{
	//TODO: type-test fns so we don't need rtti.
};

class String : public Object
{
	const char* start;
	const char* end;

public:
	String( const char* start, const char* end )
		: start( start ), end( end )
	{
	}

	bool operator==( const String& other ) const
	{
		if( end-start != other.end-other.start )
			return false;

		memcmp( start, other.start, end-start );
	}

	bool operator<( const String& other ) const
	{
		const char* a = start;
		const char* b = other.start;

		if( start == other.start && end == other.end )
			return true;

		for(;;)
		{
			if( b >= other.end )
				return false;
			if( a >= end )
				return true;
			int diff = *a - *b;
			if( diff < 0 )
				return true;
			if( diff > 0 )
				return false;
			++a;
			++b;
		}
	}
};

class Name : public Object
{
public:
	const String str;

	Name( const String& str )
		: str( str )
	{
	}
};

class Dictionary : public Object
{
	std::map<String, PObject> dict;
	typedef std::map<String, PObject>::const_iterator dict_it;
public:
	PObject Get( const Name& name )
	{
		dict_it it = dict.find( name.str );
		if( it != dict.end() )
			return it->second;
		return PObject();
	}

	void Add( const Name& key, PObject value )
	{
		dict[ key.str ] = value;
	}
};
