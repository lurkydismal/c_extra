#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/TargetParser/Host.h>

#include "arguments_parse.hpp"
#include "cextra_frontend.hpp"
#include "llvm/Option/Option.h"
#include "trace.hpp"

class IgnoreDiagnostics : public clang::DiagnosticConsumer {
public:
    void HandleDiagnostic( clang::DiagnosticsEngine::Level _diagLevel,
                           const clang::Diagnostic& _info ) override {
        traceEnter();

        traceExit();
    }
};

static inline auto getDefaultSystemIncludesFromDriver()
    -> std::vector< std::string > {
    traceEnter();

    std::vector< std::string > l_returnValue;

    llvm::vfs::InMemoryFileSystem l_vfs = llvm::vfs::InMemoryFileSystem();
    clang::DiagnosticOptions l_diagnosticsOptions;
    IgnoreDiagnostics l_ignoreDiagnostics;

    const clang::IntrusiveRefCntPtr< clang::DiagnosticsEngine > l_diagnostics =
        clang::CompilerInstance::createDiagnostics(
            l_vfs, l_diagnosticsOptions, &l_ignoreDiagnostics, false );

    clang::driver::Driver l_driver(
        "clang", llvm::sys::getDefaultTargetTriple(), *l_diagnostics );

    const auto l_compilation =
        l_driver.BuildCompilation( { "clang", "-E", "-x", "c", "/dev/null" } );

    if ( !l_compilation ) {
        goto EXIT;
    }

    for ( const clang::driver::Command& l_compilationJobCommand :
          l_compilation->getJobs() ) {
        const llvm::opt::ArgStringList& l_commandArguments =
            l_compilationJobCommand.getArguments();

        for ( size_t l_commandArgumentIndex = 0;
              ( ( l_commandArgumentIndex + 1 ) < l_commandArguments.size() );
              ++l_commandArgumentIndex ) {
            const llvm::StringRef l_argument =
                l_commandArguments[ l_commandArgumentIndex ];

            if ( l_argument == "-internal-isystem" ) {
                l_returnValue.reserve( 2 );

                l_returnValue.emplace_back( "-isystem" );
                l_returnValue.emplace_back(
                    l_commandArguments[ l_commandArgumentIndex + 1 ] );

                l_commandArgumentIndex++;
            }
        }
    }

EXIT:
    traceExit();

    return ( l_returnValue );
}

auto main( int _argumentCount, char* _argumentVector[] ) -> int {
    traceEnter();

    bool l_returnValue = false;

    {
        l_returnValue = parseArguments( _argumentCount, _argumentVector );

        if ( !l_returnValue ) {
            goto EXIT;
        }

        if ( g_needDefaultSystemIncludePaths ) {
            std::vector< std::string > l_defaultSystemIncludes =
                getDefaultSystemIncludesFromDriver();

            g_compileArguments.insert( g_compileArguments.end(),
                                       l_defaultSystemIncludes.begin(),
                                       l_defaultSystemIncludes.end() );
        }

        clang::tooling::FixedCompilationDatabase l_compilationDatabase(
            g_compilationSourceDirectory, g_compileArguments );
        clang::tooling::ClangTool l_tool( l_compilationDatabase, g_sources );

        auto l_actionFactory =
            ( ( g_isCheckOnly )
                  ? ( clang::tooling::newFrontendActionFactory<
                        clang::SyntaxOnlyAction >() )
                  // TODO: #for, #repeat
                  // TODO: iterate_struct, iterate_enum, iterate_union,
                  // iterate_arguments, iterate_annotation, iterate_scope
                  // TODO: constinit, consteval, constexpr
                  : ( clang::tooling::newFrontendActionFactory<
                        CExtraFrontendAction >() ) );

        l_returnValue = ( l_tool.run( l_actionFactory.get() ) == 0 );
    }

EXIT:
    traceExit();

    return ( ( l_returnValue ) ? ( 0 ) : ( 1 ) );
}
