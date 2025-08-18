#pragma once

#include <string>
#include <vector>

extern std::string g_compilationSourceDirectory;
extern std::string g_outputDirectory;
extern std::vector< std::string > g_compileArguments;
extern std::vector< std::string > g_sources;
extern std::string g_prefix;
extern std::string g_extension;

// Flags
extern bool g_isVerboseRun;
extern bool g_isQuietRun;
extern bool g_isDryRun;
extern bool g_needEditInPlace;
extern bool g_needOnlyPrintResult;
extern bool g_needDefaultIncludePaths;
extern bool g_needDefaultSystemIncludePaths;
extern bool g_needWarningsAsErrors;
extern bool g_isCheckOnly;
extern bool g_needTrace;

auto parseArguments( int _argumentCount, char** _argumentVector ) -> bool;
