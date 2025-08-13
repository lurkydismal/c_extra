#include "cextra_ast_consumer.hpp"

using namespace clang::ast_matchers;

CExtraASTConsumer::CExtraASTConsumer( clang::Rewriter& _r )
    : _handlerForSFunc( _r ) {
    auto l_isRecordType = qualType( hasCanonicalType( recordType() ) );

    // Match calls to sFunc(&struct, "callback")
    _matcher.addMatcher(
        callExpr(
            callee( functionDecl( hasName( "sFunc" ) ) ),
            hasArgument(
                0,
                anyOf(
                    // Record (struct)
                    unaryOperator( hasOperatorName( "&" ),
                                   hasUnaryOperand( ignoringParenImpCasts(
                                       declRefExpr( to( varDecl( hasType(
                                                        l_isRecordType ) ) ) )
                                           .bind( "addrDeclRef" ) ) ) ),

                    // Pointer to record (struct*)
                    declRefExpr(
                        to( varDecl( hasType( pointsTo( l_isRecordType ) ) ) ) )
                        .bind( "ptrDeclRef" ) ) ),
            hasArgument( 1, stringLiteral().bind( "macroStr" ) ) )
            .bind( "sFuncCall" ),
        &_handlerForSFunc );
}

void CExtraASTConsumer::HandleTranslationUnit( clang::ASTContext& _context ) {
    _matcher.matchAST( _context );
}
