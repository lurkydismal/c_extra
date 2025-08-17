#include "cextra_ast_consumer.hpp"

#include "iterate_arguments.hpp"
#include "iterate_enum.hpp"
#include "iterate_struct_union.hpp"
#include "trace.hpp"

CExtraASTConsumer::CExtraASTConsumer( clang::Rewriter& _rewriter ) {
    traceEnter();

    // TODO: Improve to not hardcode it
    IterateArgumentsHandler::addMatcher( _matcher, _rewriter );
    IterateEnumHandler::addMatcher( _matcher, _rewriter );
    IterateStructUnionHandler::addMatcher( _matcher, _rewriter );

    traceExit();
}

void CExtraASTConsumer::HandleTranslationUnit( clang::ASTContext& _context ) {
    traceEnter();

    _matcher.matchAST( _context );

    traceExit();
}
