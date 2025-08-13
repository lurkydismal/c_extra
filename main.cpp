#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include "arguments_parse.hpp"
#include "cextra_frontend.hpp"
#include "trace.hpp"

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
