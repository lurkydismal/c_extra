#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include "arguments_parse.hpp"
#include "cextra_frontend.hpp"

auto main( int _argumentCount, char* _argumentVector[] ) -> int {
    int l_returnValue = EXIT_FAILURE;

    {
        // TODO: Default compile arguments

        if ( !parseArguments( _argumentCount, _argumentVector ) ) {
            l_returnValue = EXIT_FAILURE;

            goto EXIT;
        }

        clang::tooling::FixedCompilationDatabase l_compilationDatabase(
            g_compilationSourceDirectory, g_compileArguments );
        clang::tooling::ClangTool l_tool( l_compilationDatabase, g_sources );

        // TODO: #for, #repeat
        // TODO: iterate_struct, iterate_enum, iterate_union, iterate_arguments,
        // iterate_annotation, iterate_scope
        // TODO: constinit, consteval, constexpr
        auto l_cExtraFrontendActionFactory =
            clang::tooling::newFrontendActionFactory< CExtraFrontendAction >();

        l_returnValue = l_tool.run( l_cExtraFrontendActionFactory.get() );
    }

EXIT:
    return ( l_returnValue );
}
