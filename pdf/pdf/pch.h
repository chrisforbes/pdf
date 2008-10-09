#pragma once

#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/static_assert.hpp>

#include "mapped_file.h"
#include "types.h"
#include "document.h"

#include "../Filter_Flate/flate.h"
