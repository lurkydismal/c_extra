#include "arguments_parse.hpp"

#include <argp.h>

#include <string>

#include "trace.hpp"

std::string g_compilationSourceDirectory = ".";
std::string g_outputDirectory;
std::vector< std::string > g_compileArguments = {
    "-std=gnu23",
};
const std::vector< std::string > g_defaultIncludes = {
    // Default includes
    "-isystem",
    "/usr/lib/clang/20/include",
    "-isystem",
    "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.1.1/../../../../"
    "x86_64-pc-linux-gnu/include",
    "-isystem",
    "/usr/local/include",
    "-isystem",
    "/include",
    "-isystem",
    "/usr/include",
};
std::vector< std::string > g_sources;
std::string g_prefix = ".";
std::string g_extension;

// Flags
bool g_isQuietRun = false;
bool g_isDryRun = false;
bool g_needEditInPlace = false;
bool g_needOnlyPrintResult = false;
bool g_needDefaultIncludes = true;
bool g_isCheckOnly = false;
bool g_needTrace = false;

constexpr const char* g_applicationIdentifier = "c_extra";
constexpr const char* g_applicationVersion = "0.0";
constexpr const char* g_applicationDescription =
    "Meta-programming and advanced preprocessing for C. Outputs valid "
    "Clang/GNU-compatible C code.";
constexpr const char* g_applicationContactAddress = "<lurkydismal@duck.com>";

const char* argp_program_version;
const char* argp_program_bug_address;

enum class parserOption : int16_t {
    quiet = 'q',
    dryRun = 'd',
    sourceDirectory = 's',
    outputDirectory = 'o',
    inPlace = 'i',
    prefix = 'p',
    extension = 'e',
    printResult = 1000,
    define = 'D',
    undefine = 'U',
    include = 'I',
    disableDefaultIncludes = 1001,
    checkOnly = 'c',
    trace = 1002,
};

static auto parserForOption( int _key, char* _value, struct argp_state* _state )
    -> error_t {
    traceEnter();

    error_t l_returnValue = 0;

    switch ( _key ) {
        case ( int )parserOption::quiet: {
            g_isQuietRun = true;

            break;
        }

        case ( int )parserOption::dryRun: {
            g_isDryRun = true;

            break;
        }

        case ( int )parserOption::sourceDirectory: {
            g_compilationSourceDirectory = _value;

            break;
        }

        case ( int )parserOption::outputDirectory: {
            g_outputDirectory = _value;

            break;
        }

        case ( int )parserOption::inPlace: {
            g_needEditInPlace = true;

            break;
        }

        case ( int )parserOption::prefix: {
            g_prefix = _value;

            break;
        }

        case ( int )parserOption::extension: {
            g_extension = _value;

            break;
        }

        case ( int )parserOption::printResult: {
            g_needOnlyPrintResult = true;

            break;
        }

        case ( int )parserOption::define: {
            std::string l_compileArgument = "-D ";
            l_compileArgument += _value;

            g_compileArguments.emplace_back( l_compileArgument );

            break;
        }

        case ( int )parserOption::undefine: {
            std::string l_compileArgument = "-U ";
            l_compileArgument += _value;

            g_compileArguments.emplace_back( l_compileArgument );

            break;
        }

        case ( int )parserOption::include: {
            std::string l_compileArgument = "-I ";
            l_compileArgument += _value;

            g_compileArguments.emplace_back( l_compileArgument );

            break;
        }

        case ( int )parserOption::disableDefaultIncludes: {
            g_needDefaultIncludes = false;

            break;
        }

        case ( int )parserOption::checkOnly: {
            g_isCheckOnly = true;

            break;
        }

        case ( int )parserOption::trace: {
            g_needTrace = true;

            break;
        }

        case ARGP_KEY_ARG: {
            g_sources.emplace_back( _value );

            break;
        }

        case ARGP_KEY_END: {
            if ( g_sources.empty() ) {
                argp_error( _state, "No input(s) provided." );
            }

            if ( g_needDefaultIncludes ) {
                g_compileArguments.insert( g_compileArguments.end(),
                                           g_defaultIncludes.begin(),
                                           g_defaultIncludes.end() );
            }

            break;
        }

        default: {
            l_returnValue = ARGP_ERR_UNKNOWN;
        }
    }

    traceExit();

    return ( l_returnValue );
}

auto parseArguments( int _argumentCount, char** _argumentVector ) -> bool {
    traceEnter();

    bool l_returnValue = false;

    {
        static const std::string l_programVersion =
            ( std::string( g_applicationIdentifier ) + " " +
              g_applicationVersion );
        argp_program_version = l_programVersion.c_str();
        argp_program_bug_address = g_applicationContactAddress;

        {
            static const std::string l_description =
                ( std::string( g_applicationIdentifier ) + " - " +
                  g_applicationDescription );

            const std::vector< struct argp_option > l_options = {
                // TODO: Implement
                { "verbose", 'v', nullptr, 0, "Produce verbose output", 0 },
                { "quiet", ( int )parserOption::quiet, nullptr, 0,
                  "Suppress all non-error output", 0 },
                { "dry-run", ( int )parserOption::dryRun, nullptr, 0,
                  "Show what would be done without making changes", 0 },
                { "source", ( int )parserOption::sourceDirectory, "DIR", 0,
                  "Path to source directory", 1 },
                { "output", ( int )parserOption::outputDirectory, "DIR", 0,
                  "Path to output directory", 1 },
                { "in-place", ( int )parserOption::inPlace, nullptr, 0,
                  "Modify files in place", 1 },
                { "prefix", ( int )parserOption::prefix, "PREFIX", 0,
                  "Set prefix for generated files (e.g. prefix.filename.c)",
                  1 },
                { "extension", ( int )parserOption::extension, "EXT", 0,
                  "Set extension for generated files (e.g. filename.gen.c)",
                  1 },
                // TODO: Implement
                { "stdin", 0, nullptr, 0,
                  "Read from standard input instead of file(s)", 1 },
                // TODO: Implement
                { "stdin-stream", 0, nullptr, 0,
                  "Read stream from standard input instead of file(s)", 1 },
                { "stdout", ( int )parserOption::printResult, nullptr, 0,
                  "Write result to standard output", 1 },
                // TODO: Implement
                { "enable-feature", 'f', "NAME", 0,
                  "Enable a specific custom syntax/ feature", 2 },
                // TODO: Implement
                { "disable-feature", 0, "NAME", 0,
                  "Disable a specific syntax/ feature", 2 },
                { "define", ( int )parserOption::define, "NAME=VAL", 0,
                  "Define macro before processing", 2 },
                { "undef", ( int )parserOption::undefine, "NAME", 0,
                  "Undefine macro before processing", 2 },
                { "include-path", ( int )parserOption::include, "DIR", 0,
                  "Add directory to include search path", 2 },
                { "disable-default-includes",
                  ( int )parserOption::disableDefaultIncludes, nullptr, 0,
                  "Disable default include paths", 2 },
                // TODO: Implement
                { "warnings-as-errors", 'W', nullptr, 0,
                  "Treat all warnings as errors", 2 },
                // TODO: Implement
                { "no-warnings", 0, nullptr, 0, "Suppress warnings", 2 },
                { "check-only", ( int )parserOption::checkOnly, nullptr, 0,
                  "Parse and validate without generating output", 2 },
                // TODO: Implement
                { "dump-ast", 0, nullptr, 0, "Output parsed AST for debugging",
                  2 },
                // TODO: Implement
                { "dump-tokens", 0, nullptr, 0,
                  "Output token stream before/ after transformation", 2 },
                { "trace", ( int )parserOption::trace, nullptr, 0,
                  "Trace processing steps", 3 },
                // TODO: Implement
                { "profile", 0, "LEVEL", 0,
                  "Print timing/ performance info (summary, detailed, flame)",
                  3 },
                // TODO: Implement
                { "profile-output", 0, "FILE", 0, "Path to profile output file",
                  3 },
                // TODO: Implement
                { "internal-dump", 0, nullptr, 0,
                  "Dump raw internal LLVM/ Clang structures (for dev only)",
                  3 },
                { nullptr, 0, nullptr, 0, nullptr, 0 } };

            // [NAME] - optional
            // NAME - required
            // NAME... - at least one and more
            const char* l_arguments = "FILE...";

            struct argp l_argumentParser = {
                l_options.data(), parserForOption,
                l_arguments,      l_description.c_str(),
                nullptr,          nullptr,
                nullptr };

            l_returnValue =
                ( argp_parse( &l_argumentParser, _argumentCount,
                              _argumentVector, 0, nullptr, nullptr ) == 0 );
        }

        if ( !l_returnValue ) {
            goto EXIT;
        }

        l_returnValue = true;
    }

EXIT:
    traceExit();

    return ( l_returnValue );
}
