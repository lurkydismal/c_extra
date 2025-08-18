#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include "log.hpp"
#include "trace.hpp"

namespace ast = clang::ast_matchers;

namespace common {

// Canonical record type matcher (handles typedefs/ quals)
template < typename MatcherType >
inline auto isType( MatcherType _type ) {
    return ( qualType( hasCanonicalType( _type ) ) );
}

// Helper matchers
template < typename MatcherType >
inline auto isPointerToType( MatcherType _type ) {
    return ( pointsTo( isType( _type ) ) );
}

template < typename MatcherType >
inline auto isArrayOfType( MatcherType _type ) {
    return ( arrayType( hasElementType( isType( _type ) ) ) );
}

// First-argument possibilities:
//  - &type                 -> bind "addressDeclarationReference"
//  - type*                 -> bind "pointerDeclarationReference"
//  - Array of type         -> bind "arrayDeclarationReference" (decays to
//  pointer)
//  - getStructPointer()      -> bind "callingExpressionReference" (call
//  expression that yields type*)
//  - (type MatcherType*)expression   -> bind "castingReference" and inner nodes
//  if available

// &type -> bind "addressDeclarationReference"
template < typename MatcherType >
auto referenceType( MatcherType _type ) {
    return ( unaryOperator(
        ast::hasOperatorName( "&" ),
        hasUnaryOperand( ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( isType( _type ) ) ) ) )
                .bind( "addressDeclarationReference" ) ) ) ) );
}

// type* -> bind "pointerDeclarationReference"
template < typename MatcherType >
auto pointerType( MatcherType _type ) {
    return ( ignoringParenImpCasts(
        declRefExpr( to( varDecl( hasType( isPointerToType( _type ) ) ) ) )
            .bind( "pointerDeclarationReference" ) ) );
}

// Array of type -> bind "arrayDeclarationReference" (decays to pointer)
template < typename MatcherType >
auto arrayOfType( MatcherType _type ) {
    return ( ignoringParenImpCasts(
        declRefExpr( to( varDecl( hasType( isArrayOfType( _type ) ) ) ) )
            .bind( "arrayDeclarationReference" ) ) );
}

// getTypePointer() -> bind "callingExpressionReference" (call expression that
// yields type*)
template < typename MatcherType >
auto getTypePointer( MatcherType _type ) {
    return (
        ignoringParenImpCasts( callExpr( hasType( isPointerToType( _type ) ) )
                                   .bind( "callingExpressionReference" ) ) );
}

// (type MatcherType*)expression -> bind "castingReference" and inner nodes if
// available
template < typename MatcherType >
auto castType( MatcherType _type ) {
    return ( ignoringParenImpCasts(
        cStyleCastExpr(
            hasDestinationType( isPointerToType( _type ) ),
            hasSourceExpression( ignoringParenImpCasts( anyOf(
                declRefExpr( to( varDecl( hasType( isType( _type ) ) ) ) )
                    .bind( "castingDeclarationReference" ),
                ast::callExpr().bind( "castingCallingExpression" ),
                ast::memberExpr().bind( "castingMemberExpression" ) ) ) ) )
            .bind( "castingReference" ) ) );
}

// Build replacement text
template < typename Range, typename Builder >
auto buildReplacementText( const clang::Rewriter& _rewriter,
                           const clang::CallExpr* _callingExpression,
                           const Range& _range,
                           Builder&& _builder ) -> std::string {
    traceEnter();

    std::string l_replacementText;

    const clang::SourceManager& l_sourceManager = _rewriter.getSourceMgr();

    // Determine indentation from call location
    // Use spelling loc for column
    const clang::SourceLocation l_sourceStartLocation =
        l_sourceManager.getSpellingLoc( _callingExpression->getBeginLoc() );
    unsigned l_spellingColumnNumber =
        l_sourceManager.getSpellingColumnNumber( l_sourceStartLocation );
    l_spellingColumnNumber =
        ( ( l_spellingColumnNumber > 0 ) ? ( l_spellingColumnNumber - 1 )
                                         : ( 0 ) );
    const std::string l_indent( l_spellingColumnNumber, ' ' );

    llvm::raw_string_ostream l_replacementTextStringStream( l_replacementText );

    for ( const auto* l_declaration : _range ) {
        std::forward< Builder >( _builder )(
            l_declaration, l_replacementTextStringStream, l_indent );
    }

    l_replacementTextStringStream.flush();

    l_replacementText.erase( 0, l_indent.length() );

    if ( ( !l_replacementText.empty() ) &&
         ( l_replacementText.back() == '\n' ) ) {
        l_replacementText.pop_back();
    }

    traceExit();

    return ( l_replacementText );
}

// Replace the entire call (including semicolon) with replacement text
static void replaceText( clang::Rewriter& _rewriter,
                         const clang::CallExpr* _callingExpression,
                         const clang::StringRef _replacementText ) {
    traceEnter();

    clang::CharSourceRange l_sourceRangeToReplace;

    {
        const clang::SourceManager& l_sourceManager = _rewriter.getSourceMgr();
        const clang::LangOptions& l_langOptions = _rewriter.getLangOpts();

        const clang::SourceLocation l_sourceEndLocation =
            _callingExpression->getEndLoc();
        // TODO:Rename
        const clang::SourceLocation l_lastCharacterSourceLocation =
            clang::Lexer::getLocForEndOfToken( l_sourceEndLocation, 0,
                                               l_sourceManager, l_langOptions );

        bool l_isSemicolonIncluded = false;

        if ( l_lastCharacterSourceLocation.isValid() ) {
            // TODO: Rename
            const clang::SourceLocation l_afterSpelling =
                l_sourceManager.getSpellingLoc( l_lastCharacterSourceLocation );

            if ( l_afterSpelling.isValid() ) {
                bool l_isInvalid = false;

                const char* l_lastCharacter = l_sourceManager.getCharacterData(
                    l_afterSpelling, &l_isInvalid );

                if ( ( !l_isInvalid ) && ( l_lastCharacter ) &&
                     ( *l_lastCharacter == ';' ) ) {
                    l_isSemicolonIncluded = true;
                }
            }
        }

        const clang::SourceLocation l_replacementSourceEndLocation =
            ( ( l_isSemicolonIncluded )
                  ? ( l_lastCharacterSourceLocation.getLocWithOffset( 1 ) )
                  : ( l_sourceEndLocation ) );

        l_sourceRangeToReplace = clang::CharSourceRange::getCharRange(
            _callingExpression->getBeginLoc(), l_replacementSourceEndLocation );
    }

    // FIX: Log this
    // logVariable( l_sourceRangeToReplace );

    // Only attempt to query rewritten text/ replace if the range is
    // valid and in main file
    if ( ( !l_sourceRangeToReplace.isValid() ) ||
         ( !_rewriter.getSourceMgr().isWrittenInMainFile(
             l_sourceRangeToReplace.getBegin() ) ) ) {
        logError( "Invalid or non-main file range for rewrite." );

        goto EXIT;
    }

    {
        _rewriter.ReplaceText( l_sourceRangeToReplace, _replacementText );

        // Debug existing rewritten text safely
        const std::string l_existing =
            _rewriter.getRewrittenText( l_sourceRangeToReplace );

        if ( l_existing.empty() ) {
            logError(
                "Existing rewritten text is empty or not yet rewritten." );

        } else {
            log( "Existing rewritten text: " + l_existing );
        }
    }

EXIT:
    traceExit();
}

} // namespace common
