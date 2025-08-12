#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include "arguments_parse.hpp"
#include "rewrition_frontend.hpp"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

#if 0
class MacroNameReplacer : public PPCallbacks {
public:
    MacroNameReplacer( Rewriter& _r, SourceManager& _sm )
        : _theRewriter( _r ), _sm( _sm ) {}

    void MacroExpands( const Token& _macroNameTok,
                       const MacroDefinition& _md,
                       SourceRange _range,
                       const MacroArgs* _args ) override {
        SourceLocation l_loc = _macroNameTok.getLocation();
        StringRef l_macroName = _macroNameTok.getIdentifierInfo()->getName();

        // Replace macro usage with "MACRO_NAME"
        _theRewriter.ReplaceText( l_loc, l_macroName.size(),
                                  "\"" + l_macroName.str() + "\"" );
    }

private:
    Rewriter& _theRewriter;
    SourceManager& _sm;
};
#endif

auto main( int _argumentCount, char* _argumentVector[] ) -> int {
    int l_returnValue = EXIT_FAILURE;

    {
        if ( !parseArguments( _argumentCount, _argumentVector ) ) {
            l_returnValue = EXIT_FAILURE;

            goto EXIT;
        }

        FixedCompilationDatabase l_compilationDatabase(
            g_compilationSourceDirectory, g_compileArgs );
        ClangTool l_tool( l_compilationDatabase, g_sources );

        // TODO: #for, #repeat
#if 0
        auto l_preprocessionFactory = newFrontendActionFactory< EvaluationFrontendAction >();
        l_returnValue = l_tool.run( l_preprocessionFactory.get() );
#endif

        // TODO: iterate_struct, iterate_enum, iterate_union, iterate_arguments,
        // iterate_annotation, iterate_scope
        auto l_rewritionFactory =
            newFrontendActionFactory< RewritionFrontendAction >();
        l_returnValue = l_tool.run( l_rewritionFactory.get() );

        // TODO: constinit, consteval, constexpr
#if 0
        auto l_evaluationFactory = newFrontendActionFactory< EvaluationFrontendAction >();
        l_returnValue = l_tool.run( l_evaluationFactory.get() );
#endif
    }

EXIT:
    return ( l_returnValue );
}
