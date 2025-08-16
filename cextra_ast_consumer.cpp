#include "cextra_ast_consumer.hpp"

#include "iterate_struct.hpp"
#include "trace.hpp"

CExtraASTConsumer::CExtraASTConsumer( clang::Rewriter& _rewriter ) {
    traceEnter();

    // TODO: Improve to not hardcode it
    IterateStructUnionHandler::addMatcher( _matcher, _rewriter );

    traceExit();
}

void CExtraASTConsumer::HandleTranslationUnit( clang::ASTContext& _context ) {
    traceEnter();

    _matcher.matchAST( _context );

    traceExit();
}
