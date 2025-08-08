#pragma once

#include <string>
#include <vector>

extern std::string g_compilationSourceDirectory;
extern std::vector< std::string > g_compileArgs;
extern std::vector< std::string > g_sources;

auto parseArguments( int _argumentCount, char** _argumentVector ) -> bool;
