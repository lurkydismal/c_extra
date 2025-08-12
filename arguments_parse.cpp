#include "arguments_parse.hpp"

#include <argp.h>

std::string g_compilationSourceDirectory;
// TODO: Improve
std::vector< std::string > g_compileArguments = {
    "-std=gnu23",

    // Default options
    "-isystem",
    "/usr/lib/clang/20/include",
    "-isystem",
    "/usr/local/include",
    "-isystem",
    "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.1.1/../../../../"
    "x86_64-pc-linux-gnu/include",
    "-isystem",
    "/include",
    "-isystem",
    "/usr/include",
};

std::vector< std::string > g_sources;

// Flags
bool g_needEditInPlace = false;

constexpr const char* g_applicationIdentifier = "c_extra";
constexpr const char* g_applicationVersion = "0.0";
// TODO: Add description
constexpr const char* g_applicationDescription = "placeholder";
constexpr const char* g_applicationContactAddress = "<lurkydismal@duck.com>";

const char* argp_program_version;
const char* argp_program_bug_address;

static auto parserForOption( int _key, char* _value, struct argp_state* _state )
    -> error_t {
    error_t l_returnValue = 0;

    switch ( _key ) {
        case ARGP_KEY_ARG: {
            g_sources.emplace_back( _value );

            break;
        }

        case ARGP_KEY_END: {
            if ( g_sources.empty() ) {
                argp_error( _state, "No input(s) provided." );
            }

            if ( g_compilationSourceDirectory.empty() ) {
                g_compilationSourceDirectory = ".";
            }

            break;
        }

        default: {
            l_returnValue = ARGP_ERR_UNKNOWN;
        }
    }

    return ( l_returnValue );
}

auto parseArguments( int _argumentCount, char** _argumentVector ) -> bool {
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
                // TODO: Implement
                { "quiet", 'q', nullptr, 0, "Suppress all non-error output",
                  0 },
                // TODO: Implement
                { "dry-run", 'd', nullptr, 0,
                  "Show what would be done without making changes", 0 },
                // TODO: Implement
                { "source", 's', "DIR", 0, "Path to source directory", 1 },
                // TODO: Implement
                { "output", 'o', "DIR", 0, "Path to output directory", 1 },
                // TODO: Implement
                { "in-place", 'i', nullptr, 0, "Modify files in place", 1 },
                // TODO: Implement
                { "extension", 'e', "EXT", 0,
                  "Set extension for generated files (e.g. .gen.c)", 1 },
                // TODO: Implement
                { "stdin", 0, nullptr, 0,
                  "Read from standard input instead of file(s)", 1 },
                // TODO: Implement
                { "stdout", 0, nullptr, 0, "Write result to standard output",
                  1 },
                // TODO: Implement
                { "enable-feature", 'f', "NAME", 0,
                  "Enable a specific custom syntax/ feature", 2 },
                // TODO: Implement
                { "disable-feature", 0, "NAME", 0,
                  "Disable a specific syntax/ feature", 2 },
                // TODO: Implement
                { "define", 'D', "NAME=VAL", 0,
                  "Define macro before processing", 2 },
                // TODO: Implement
                { "undef", 'U', "NAME", 0, "Undefine macro before processing",
                  2 },
                // TODO: Implement
                { "include-path", 'I', "DIR", 0,
                  "Add directory to include search path", 2 },
                // TODO: Implement
                { "warnings-as-errors", 'W', nullptr, 0,
                  "Treat all warnings as errors", 2 },
                // TODO: Implement
                { "no-warnings", 0, nullptr, 0, "Suppress warnings", 2 },
                // TODO: Implement
                { "check-only", 'c', nullptr, 0,
                  "Parse and validate without generating output", 2 },
                // TODO: Implement
                { "dump-ast", 0, nullptr, 0, "Output parsed AST for debugging",
                  2 },
                // TODO: Implement
                { "dump-tokens", 0, nullptr, 0,
                  "Output token stream before/ after transformation", 2 },
                // TODO: Implement
                { "trace", 0, nullptr, 0, "Trace processing steps", 3 },
                // TODO: Implement
                { "profile", 0, nullptr, 0, "Print timing/performance info",
                  3 },
                // TODO: Implement
                { "internal-dump", 0, nullptr, 0,
                  "Dump raw internal LLVM/Clang structures (for dev only)", 3 },
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
    return ( l_returnValue );
}
