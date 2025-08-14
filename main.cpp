#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>

#include "arguments_parse.hpp"
#include "cextra_frontend.hpp"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Job.h"
#include "clang/Driver/ToolChain.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"
#include "trace.hpp"

#if 1
using namespace clang;
class IgnoreDiagnostics : public DiagnosticConsumer {
public:
    void HandleDiagnostic( DiagnosticsEngine::Level DiagLevel,
                           const clang::Diagnostic& Info ) override;
};

void IgnoreDiagnostics::HandleDiagnostic( DiagnosticsEngine::Level DiagLevel,
                                          const clang::Diagnostic& Info ) {}

auto getDefaultIncludesFromDriver() -> std::vector< std::string > {
    using namespace clang;
    using namespace clang::driver;
    using namespace llvm;

    std::vector< std::string > Includes;

#if 0
    clang::CompilerInstance CI;

    llvm::IntrusiveRefCntPtr< clang::DiagnosticOptions > diagOpts(
        new clang::DiagnosticOptions );
    clang::TextDiagnosticPrinter diagPrinter( llvm::errs(), *diagOpts.get() );

    auto& vfs = *llvm::vfs::getRealFileSystem();
    auto* client =
        new clang::TextDiagnosticPrinter( llvm::errs(), *diagOpts.get() );

    CI.createDiagnostics( vfs, client );

    llvm::IntrusiveRefCntPtr< clang::DiagnosticsEngine > diags =
        CI.getDiagnosticsPtr();
#endif

    IgnoreDiagnostics IgnoreDiags;
    DiagnosticOptions DiagOpts;
    IntrusiveRefCntPtr< DiagnosticsEngine > diags =
        CompilerInstance::createDiagnostics( *llvm::vfs::getRealFileSystem(),
                                             DiagOpts, &IgnoreDiags,
                                             /*ShouldOwnClient=*/false );

    clang::driver::Driver driver( "clang", llvm::sys::getDefaultTargetTriple(),
                                  *diags );

    std::vector< const char* > args = { "clang", "-E", "-x",
                                        "c",     "-v", "/dev/null" };

    auto compilation = driver.BuildCompilation( args );

    if ( !compilation )
        return Includes;

    for ( const auto& job : compilation->getJobs() ) {
        if ( const auto* cmd =
                 llvm::dyn_cast< clang::driver::Command >( &job ) ) {
            const auto& cmdArgs = cmd->getArguments();
            for ( size_t i = 0; i < cmdArgs.size(); ++i ) {
                llvm::StringRef arg = cmdArgs[ i ];
                if ( arg == "-internal-isystem" && i + 1 < cmdArgs.size() ) {
                    Includes.emplace_back( cmdArgs[ i + 1 ] );
                    ++i;
                }
            }
        }
    }

    return Includes;

#if 0
    DiagnosticsEngine l_diags(
        IntrusiveRefCntPtr< DiagnosticIDs >( new DiagnosticIDs ),
        *new DiagnosticOptions );
    TextDiagnosticFormat l_diagPrinter( llvm::outs(),
                                        *new DiagnosticOptions() );
    l_diags.setClient( &l_diagPrinter, /*shouldOwnClient=*/false );

    Driver l_theDriver( _binaryName.str(), _targetTriple.str(), Diags );

    // Build the command-line as if invoking: clang -x c++ -c /dev/null (or
    // equivalent)
    opt::ArgStringList l_cC1Args;
    l_theDriver.CC1Assemble = false;
    l_theDriver.BuildCompilation( std::vector< const char* >{
        _binaryName.str().c_str(), "-x", "c",
        "-" // empty input
    } );

    // Create a Compilation object
    std::unique_ptr< Compilation > l_c(
        l_theDriver.BuildCompilation( SmallVector< const char*, 4 >{
            _binaryName.str().c_str(), "-x", "c", "-E", "-" } ) );
    if ( !l_c )
        return {};

    const JobList& l_jobs = l_c->getJobs();
    if ( l_jobs.empty() )
        return {};

    // Extract the first compilation job, get its ToolChain
    const Command& l_cmd = cast< Command >( *l_jobs.begin() );
    const ToolChain& l_tc = l_c->getDefaultToolChain();

    // Prepare args lists
    opt::ArgStringList l_dummyDriverArgs;
    opt::ArgStringList l_cC1IncludeArgs;
    l_tc.AddClangSystemIncludeArgs( *l_c->getArgs(), l_cC1IncludeArgs );

    std::vector< std::string > l_paths;
    for ( auto& l_arg : l_cC1IncludeArgs ) {
        l_paths.emplace_back( l_arg );
    }

    return l_paths;
#endif
}
#endif

auto main( int _argumentCount, char* _argumentVector[] ) -> int {
    traceEnter();

    bool l_returnValue = false;

    {
        std::vector< std::string > x = getDefaultIncludesFromDriver();

        llvm::outs() << "Vector contents: ";
        for ( const auto& val : x ) {
            llvm::outs() << val << " ";
        }
        llvm::outs() << "\n";

        l_returnValue = parseArguments( _argumentCount, _argumentVector );

        if ( !l_returnValue ) {
            goto EXIT;
        }

        // TODO: Default compile arguments from clang driver
#if 0
        auto defaults = get_default_includes_from_driver("clang");
        if (!defaults.empty()) {
            // naive dedupe: track pairs we already added
            for (size_t i = 0; i + 1 < defaults.size(); i += 2) {
                const auto &flag = defaults[i];
                const auto &path = defaults[i+1];

                bool found = false;
                for (size_t j = 0; j + 1 < g_compileArguments.size(); ++j) {
                    if (g_compileArguments[j] == flag && g_compileArguments[j+1] == path) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    g_compileArguments.push_back(flag);
                    g_compileArguments.push_back(path);
                }
            }
        }
#endif

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
