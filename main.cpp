#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>

#include "arguments_parse.hpp"
#include "cextra_frontend.hpp"
#include "trace.hpp"

auto getDefaultIncludesFromDriver( const std::string& _clangExe = "clang" )
    -> std::vector< std::string > {
    std::vector< std::string > result;

    // minimal llvm init (not always required but safe)
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllTargets();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    // Diagnostics setup
    auto DiagIDs = new clang::DiagnosticIDs();
    clang::IntrusiveRefCntPtr< clang::DiagnosticOptions > DiagOpts(
        new clang::DiagnosticOptions() );
    clang::TextDiagnosticPrinter* DiagClient =
        new clang::TextDiagnosticPrinter( llvm::errs(), &*DiagOpts );
    clang::IntrusiveRefCntPtr< clang::DiagnosticsEngine > Diags =
        new clang::DiagnosticsEngine( DiagIDs, &*DiagOpts, DiagClient );

    // Host triple
    std::string TripleStr = llvm::sys::getDefaultTargetTriple();

    // Create driver
    clang::driver::Driver Driver( _clangExe, TripleStr, *Diags );

    // Build an argv that asks the driver to produce the cc1 job that it would
    // use. We request preprocessing verbose output; driver will construct a cc1
    // command that contains -internal-isystem / -isystem entries we can parse
    // out. Include target/stdlib args if you want cross-target behavior.
    const char* argv[] = {
        _clangExe.c_str(),
        "-E", // preprocess
        "-x",
        "c", // language
        "-", // input from stdin (driver still computes include paths)
        "-v" // verbose: driver/cc1 include/path options appear in job args
    };
    ArrayRef< const char* > Args( argv,
                                  argv + sizeof( argv ) / sizeof( argv[ 0 ] ) );

    std::unique_ptr< clang::driver::Compilation > C(
        Driver.BuildCompilation( Args ) );

    if ( !C ) {
        // failed to build compilation; return empty
        return result;
    }

    const clang::driver::JobList& Jobs = C->getJobs();
    for ( const auto& J : Jobs ) {
        if ( const clang::driver::Command* Cmd =
                 llvm::dyn_cast< clang::driver::Command >( &J ) ) {
            // iterate command arguments; these are C strings
            for ( const char* A : Cmd->getArguments() ) {
                if ( !A )
                    continue;
                llvm::StringRef s( A );

                // normalize a few common forms:
                // -internal-isystem,/path  OR -internal-isystem /path
                // -isystem,/path OR -isystem /path
                // -I/path or -I /path
                if ( s.startswith( "-internal-isystem" ) ) {
                    // could be "-internal-isystem/path" or "-internal-isystem"
                    // followed by path
                    if ( s.size() > strlen( "-internal-isystem" ) ) {
                        result.push_back( "-isystem" );
                        result.push_back(
                            s.substr( strlen( "-internal-isystem" ) ).str() );
                    } else {
                        // next arg is the path (need to look ahead)
                        // NOTE: getArguments() yields a full vector; safe to
                        // iterate by index.
                    }
                } else if ( s.startswith( "-isystem" ) ) {
                    if ( s.size() > strlen( "-isystem" ) ) {
                        result.push_back( "-isystem" );
                        result.push_back(
                            s.substr( strlen( "-isystem" ) ).str() );
                    }
                } else if ( s.startswith( "-I" ) ) {
                    if ( s.size() > 2 ) {
                        result.push_back( "-I" );
                        result.push_back( s.substr( 2 ).str() );
                    }
                } else if ( s == "--resource-dir" ) {
                    // resource-dir is typically followed by a path in the args
                    // list (you may need index-based iteration to capture the
                    // next token)
                }
                // detect other flags here (e.g. -isystem-with-prefix,
                // -nostdinc, etc)
            }
        }
    }

    // A more robust implementation iterates with an index so you can capture
    // "flag separate-by-space path" forms (e.g. "-isystem" then "/path").

    return result;
}

auto main( int _argumentCount, char* _argumentVector[] ) -> int {
    traceEnter();

    bool l_returnValue = false;

    {
        l_returnValue = parseArguments( _argumentCount, _argumentVector );

        if ( !l_returnValue ) {
            goto EXIT;
        }

        // TODO: Default compile arguments from clang driver

        clang::tooling::FixedCompilationDatabase l_compilationDatabase(
            g_compilationSourceDirectory, g_compileArguments );
        clang::tooling::ClangTool l_tool( l_compilationDatabase, g_sources );

        if ( g_isCheckOnly ) {
            auto l_syntaxOnlyFrontendActionFactory =
                clang::tooling::newFrontendActionFactory<
                    clang::SyntaxOnlyAction >();

            l_returnValue =
                ( l_tool.run( l_syntaxOnlyFrontendActionFactory.get() ) == 0 );

        } else {
            // TODO: #for, #repeat
            // TODO: iterate_struct, iterate_enum, iterate_union,
            // iterate_arguments, iterate_annotation, iterate_scope
            // TODO: constinit, consteval, constexpr
            auto l_cExtraFrontendActionFactory =
                clang::tooling::newFrontendActionFactory<
                    CExtraFrontendAction >();

            l_returnValue =
                ( l_tool.run( l_cExtraFrontendActionFactory.get() ) == 0 );
        }
    }

EXIT:
    traceExit();

    return ( ( l_returnValue ) ? ( 0 ) : ( 1 ) );
}
