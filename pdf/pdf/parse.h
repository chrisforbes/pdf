#pragma once

#include "types.h"

PObject Parse( const char*& p );
PDictionary ParseDict( const char*& p );
PArray ParseArray( const char*& p );
String ParseContent( const char *& p, std::vector<PObject>& intoVector );