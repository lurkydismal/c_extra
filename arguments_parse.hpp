#pragma once

#include <string>
#include <vector>

extern std::string g_compilationSourceDirectory;
extern std::vector< std::string > g_compileArguments;
extern std::vector< std::string > g_sources;

// Flags
extern bool g_needEditInPlace;

auto parseArguments( int _argumentCount, char** _argumentVector ) -> bool;
