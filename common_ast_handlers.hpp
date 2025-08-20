#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <tuple>

#include "common.hpp"
#include "llvm/Support/raw_ostream.h"
#include "log.hpp"
#include "trace.hpp"

namespace ast = clang::ast_matchers;

namespace common {

struct EnumTag {};
struct RecordTag {};

template < typename Tag >
struct TypeTraits;

template <>
struct TypeTraits< RecordTag > {
    using Type = clang::RecordType;
    using Decl = clang::RecordDecl;
};

template <>
struct TypeTraits< EnumTag > {
    using Type = clang::EnumType;
    using Decl = clang::EnumDecl;
};

// Canonical type matcher (handles typedefs/ quals)
template < typename MatcherType >
inline auto isType( const MatcherType& _type ) {
    return ( qualType( hasCanonicalType( _type ) ) );
}

// Helper matchers
template < typename MatcherType >
inline auto isPointerToType( const MatcherType& _type ) {
    return ( pointsTo( isType( _type ) ) );
}

template < typename MatcherType >
inline auto isArrayOfType( const MatcherType& _type ) {
    return ( arrayType( hasElementType( isType( _type ) ) ) );
}

// First-argument possibilities:
//  - &type                 -> bind "addressDeclarationReference"
//  - type*                 -> bind "pointerDeclarationReference"
//  - Array of type         -> bind "arrayDeclarationReference" (decays to
//  pointer)
//  - getTypePointer()      -> bind "callingExpressionReference" (call
//  expression that yields type*)
//  - (type MatcherType*)expression   -> bind "castingReference" and inner nodes
//  if available

// &type -> bind "addressDeclarationReference"
template < typename MatcherType >
auto referenceType( const MatcherType& _type ) {
    return ( unaryOperator(
        ast::hasOperatorName( "&" ),
        hasUnaryOperand( ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( isType( _type ) ) ) ) )
                .bind( "addressDeclarationReference" ) ) ) ) );
}

// type* -> bind "pointerDeclarationReference"
template < typename MatcherType >
auto pointerType( const MatcherType& _type ) {
    return ( ignoringParenImpCasts(
        declRefExpr( to( varDecl( hasType( isPointerToType( _type ) ) ) ) )
            .bind( "pointerDeclarationReference" ) ) );
}

// Array of type -> bind "arrayDeclarationReference" (decays to pointer)
template < typename MatcherType >
auto arrayOfType( const MatcherType& _type ) {
    return ( ignoringParenImpCasts(
        declRefExpr( to( varDecl( hasType( isArrayOfType( _type ) ) ) ) )
            .bind( "arrayDeclarationReference" ) ) );
}

// getTypePointer() -> bind "callingExpressionReference" (call expression that
// yields type*)
template < typename MatcherType >
auto getTypePointer( const MatcherType& _type ) {
    return (
        ignoringParenImpCasts( callExpr( hasType( isPointerToType( _type ) ) )
                                   .bind( "callingExpressionReference" ) ) );
}

// (type MatcherType*)expression -> bind "castingReference" and inner nodes if
// available
template < typename MatcherType >
auto castPointerType( const MatcherType& _type ) {
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

template < typename MatcherType >
auto anyReference( const MatcherType& _type ) {
    return ( anyOf( common::referenceType( _type ),
                    common::pointerType( _type ), common::arrayOfType( _type ),
                    common::getTypePointer( _type ),
                    common::castPointerType( _type ) ) );
}

template < typename QualifierType >
auto buildUnderlyingTypeString( QualifierType _qualifierType ) -> std::string {
    traceEnter();

    std::string l_returnValue;

    const auto* l_typedefType =
        _qualifierType->template getAs< clang::TypedefType >();

    // Extract typedef
    if ( l_typedefType ) {
        l_returnValue = l_typedefType->getDecl()->getNameAsString();

    } else {
        l_returnValue = _qualifierType.getAsString();
    }

    traceExit();

    return ( l_returnValue );
}

// Build replacement text
template < typename Range, typename Builder >
auto buildReplacementText( const clang::Rewriter& _rewriter,
                           const clang::CallExpr* _callingExpression,
                           const Range& _range,
                           Builder&& _builder ) -> std::string {
    traceEnter();

    std::string l_returnValue;

    const clang::SourceManager& l_sourceManager = _rewriter.getSourceMgr();

    // Determine indentation from call location
    // Use spelling loc for column
    const clang::SourceLocation l_sourceStartLocation =
        l_sourceManager.getExpansionLoc( _callingExpression->getBeginLoc() );
    unsigned l_spellingColumnNumber =
        l_sourceManager.getExpansionColumnNumber( l_sourceStartLocation );
    l_spellingColumnNumber =
        ( ( l_spellingColumnNumber > 0 ) ? ( l_spellingColumnNumber - 1 )
                                         : ( 0 ) );
    const std::string l_indentation( l_spellingColumnNumber, ' ' );

    llvm::raw_string_ostream l_replacementTextStringStream( l_returnValue );

    for ( const auto* l_declaration : _range ) {
        std::forward< Builder >( _builder )(
            l_declaration, l_replacementTextStringStream, l_indentation );
    }

    l_replacementTextStringStream.flush();

    l_returnValue.erase( 0, l_indentation.length() );

    if ( ( !l_returnValue.empty() ) && ( l_returnValue.back() == '\n' ) ) {
        l_returnValue.pop_back();
    }

    traceExit();

    return ( l_returnValue );
}

// Replace the entire call (including semicolon) with replacement text
static void replaceText( clang::Rewriter& _rewriter,
                         const clang::CallExpr* _callingExpression,
                         const clang::StringRef _replacementText ) {
    traceEnter();

    clang::CharSourceRange l_sourceRangeToReplace;

    const clang::SourceManager& l_sourceManager = _rewriter.getSourceMgr();

    // Build source range to replace
    {
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
            const clang::SourceLocation l_afterExpansion =
                l_sourceManager.getExpansionLoc(
                    l_lastCharacterSourceLocation );

            if ( l_afterExpansion.isValid() ) {
                bool l_isInvalid = false;

                const char* l_lastCharacter = l_sourceManager.getCharacterData(
                    l_afterExpansion, &l_isInvalid );

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

        l_sourceRangeToReplace = clang::CharSourceRange::getCharRange(
            l_sourceManager.getFileLoc( _callingExpression->getBeginLoc() ),
            l_sourceManager.getFileLoc( l_sourceEndLocation ) );
    }

    {
        const clang::CharSourceRange& l_temp = l_sourceRangeToReplace;

        std::string l_sourceRangeToReplace;
        llvm::raw_string_ostream l_sourceRangeToReplaceStringStream(
            l_sourceRangeToReplace );

        l_sourceRangeToReplaceStringStream
            << "[ " << "Begin: " << "\""
            << _callingExpression->getBeginLoc().printToString(
                   l_sourceManager )
            << "\"" << ", End: " << "\""
            << _callingExpression->getEndLoc().printToString( l_sourceManager )
            << "\""
            << " ]";

        l_sourceRangeToReplaceStringStream.flush();

        logVariable( l_sourceRangeToReplace );
    }

    // Only attempt to query rewritten text/ replace if the range is
    // valid and in main file
    if ( ( !l_sourceRangeToReplace.isValid() ) ||
         ( !_rewriter.getSourceMgr().isWrittenInMainFile(
             l_sourceRangeToReplace.getBegin() ) ) ) {
        logError( "Invalid or non-main file range for rewrite." );

        goto EXIT;
    }

    _rewriter.ReplaceText( l_sourceRangeToReplace, _replacementText );

    // FIX: When expanded from macro -> always empty or not yet rewritten
#if 0
        // Debug existing rewritten text safely
    {
        const std::string l_existing =
            _rewriter.getRewrittenText( l_sourceRangeToReplace );

        logVariable( l_existing );

        if ( l_existing.empty() ) {
            logError( "Rewritten text is empty or not yet rewritten." );

        } else {
            log( "Rewritten text: " + l_existing );
        }
    }
#endif

EXIT:
    traceExit();
}

// Finds a CallExpr bound to _callName and extracts the underlying QualType
// for the first argument passed to the callback.
// Handles forms:
//   &var, var (pointer), array (decays to pointer), call returning pointer,
//   explicit C-style cast (from declref/ call/ member/ cast).
//
// Returns tuple:
//  ( calling CallExpr*,
//    resolved QualType,
//    original Type declaration (definition if available),
//    base expression text (source snippet),
//    whether a pointer was passed,
//    callback name )
//
// Error handling:
//   The function does not throw or return error codes.
//   Instead, any error results in some tuple elements being invalid:
//     - CallExpr* == nullptr       → no matching call expression
//     - QualType.isNull()          → type could not be resolved
//     - Decl* == nullptr           → original declaration missing
//     - base expression text empty → could not resolve source text
template < typename Tag, typename Checker >
auto inferCallbackArgumentContext( const ast::MatchFinder::MatchResult& _result,
                                   const clang::StringRef _callName,
                                   Checker&& _checker )
    -> std::tuple< const clang::CallExpr*,
                   const clang::QualType,
                   const typename TypeTraits< Tag >::Decl*,
                   const clang::StringRef,
                   const bool,
                   const clang::StringRef > {
    traceEnter();

    using Type = typename TypeTraits< Tag >::Type;
    using Decl = typename TypeTraits< Tag >::Decl;

    clang::QualType l_returnedQualifierType;
    Decl* l_originalDeclaration = nullptr;
    clang::StringRef l_baseExpressionText;
    bool l_pointerPassed = false;
    clang::StringRef l_callbackName;

    const clang::SourceManager& l_sourceManager = *( _result.SourceManager );
    const clang::LangOptions l_langOptions = _result.Context->getLangOpts();

    // Helper: get token-range source text for an Expr, fallback on '_fallback'
    auto l_getSourceTextOrFallback =
        [ & ]( const clang::Expr* _expression,
               const clang::StringRef _fallbackResult ) -> clang::StringRef {
        traceEnter();

        clang::StringRef l_returnValue = _fallbackResult;

        if ( !_expression ) {
            goto EXIT;
        }

        {
            clang::CharSourceRange l_sourceRange =
                clang::CharSourceRange::getTokenRange(
                    _expression->getSourceRange() );

            if ( !l_sourceRange.isValid() ) {
                goto EXIT;
            }

            clang::Expected< clang::StringRef > l_sourceText =
                clang::Lexer::getSourceText( l_sourceRange, l_sourceManager,
                                             l_langOptions );

            if ( l_sourceText ) {
                l_returnValue = l_sourceText.get();
            }
        }

    EXIT:
        traceExit();

        return ( l_returnValue );
    };

    // Helper: extract the underlying QualType from a VarDecl.
    // NOTE: If pointerPassed==true, try to return pointee type.
    auto l_getQalifierTypeFromVariable =
        [ & ]( const clang::VarDecl* _variableDeclaration,
               const bool _pointerPassed ) -> clang::QualType {
        traceEnter();

        clang::QualType l_returnValue = {};

        if ( !_variableDeclaration ) {
            goto EXIT;
        }

        {
            clang::QualType l_qualifierType = _variableDeclaration->getType();

            l_returnValue = l_qualifierType.getUnqualifiedType();

            if ( _pointerPassed ) {
                if ( l_qualifierType->isPointerType() ) {
                    l_returnValue =
                        l_qualifierType->getPointeeType().getUnqualifiedType();
                }

                auto* l_referenceType =
                    l_qualifierType->getAs< clang::ReferenceType >();

                if ( l_referenceType ) {
                    l_returnValue =
                        l_referenceType->getPointeeType().getUnqualifiedType();
                }
            }
        }

    EXIT:
        traceExit();

        return ( l_returnValue );
    };

    const auto* l_callingExpression =
        _result.Nodes.getNodeAs< clang::CallExpr >( _callName );

    logVariable( l_callingExpression );

    if ( !l_callingExpression ) {
        goto EXIT;
    }

    {
        // 2nd argument
        const auto* l_callbackNameLiteral =
            _result.Nodes.getNodeAs< clang::StringLiteral >( "callbackName" );

        logVariable( l_callbackNameLiteral );

        if ( !l_callbackNameLiteral ) {
            goto EXIT;
        }

        l_callbackName = l_callbackNameLiteral->getString();

        logVariable( l_callbackName );

        // 1st argument
        const auto* l_addressDeclarationReference =
            _result.Nodes.getNodeAs< clang::DeclRefExpr >(
                "addressDeclarationReference" );
        const auto* l_pointerDeclarationReference =
            _result.Nodes.getNodeAs< clang::DeclRefExpr >(
                "pointerDeclarationReference" );
        const auto* l_arrayDeclarationReference =
            _result.Nodes.getNodeAs< clang::DeclRefExpr >(
                "arrayDeclarationReference" );
        const auto* l_callingExpressionReference =
            _result.Nodes.getNodeAs< clang::CallExpr >(
                "callingExpressionReference" );

        const auto* l_castingReference =
            _result.Nodes.getNodeAs< clang::CStyleCastExpr >(
                "castingReference" );
        const auto* l_castingDeclarationReference =
            _result.Nodes.getNodeAs< clang::DeclRefExpr >(
                "castingDeclarationReference" );
        const auto* l_castingCallingExpression =
            _result.Nodes.getNodeAs< clang::CallExpr >(
                "castingCallingExpression" );
        const auto* l_castingMemberExpression =
            _result.Nodes.getNodeAs< clang::MemberExpr >(
                "castingMemberExpression" );

        const clang::VarDecl* l_variableDeclaration = nullptr;

        logVariable( l_variableDeclaration );

        auto l_extractDeclarationContext =
            [ & ]( const clang::DeclRefExpr* _declarationReference,
                   const bool _pointerPassed ) {
                traceEnter();

                l_variableDeclaration = llvm::dyn_cast< clang::VarDecl >(
                    _declarationReference->getDecl() );

                logVariable( l_variableDeclaration );

                if ( !l_variableDeclaration ) {
                    logError( "First argument is not a VarDecl." );

                    goto EXIT;
                }

                l_baseExpressionText = l_getSourceTextOrFallback(
                    _declarationReference,
                    l_variableDeclaration->getNameAsString() );

                logVariable( l_baseExpressionText );

                l_returnedQualifierType = l_getQalifierTypeFromVariable(
                    l_variableDeclaration, l_pointerPassed );

                logVariable( l_returnedQualifierType );

            EXIT:
                traceExit();
            };

        // Order of precedence: &tyoe, type*, array of type,
        // getTypePointer(), (type T*)expression (if casted from declref),
        // (type T*)expression.
        if ( l_addressDeclarationReference ) {
            // &var  -> base is var, pointerPassed = false
            l_pointerPassed = false;

            l_extractDeclarationContext( l_addressDeclarationReference,
                                         l_pointerPassed );

        } else if ( l_pointerDeclarationReference ) {
            // Pointer variable passed -> base is var, pointerPassed = true
            l_pointerPassed = true;

            l_extractDeclarationContext( l_pointerDeclarationReference,
                                         l_pointerPassed );

        } else if ( l_arrayDeclarationReference ) {
            // Array variable passed (decays to pointer)
            l_pointerPassed = true;

            l_extractDeclarationContext( l_arrayDeclarationReference,
                                         l_pointerPassed );

            // Extract element type from array type
            const clang::Type* l_variableDeclarationType =
                l_variableDeclaration->getType().getTypePtrOrNull();

            if ( l_variableDeclarationType ) {
                const auto* l_arrayType = llvm::dyn_cast< clang::ArrayType >(
                    l_variableDeclarationType );

                if ( l_arrayType ) {
                    l_returnedQualifierType =
                        l_arrayType->getElementType().getUnqualifiedType();

                    logVariable( l_returnedQualifierType );
                }
            }

        } else if ( l_callingExpressionReference ) {
            // Call expression that returns type*
            l_pointerPassed = true;

            l_baseExpressionText =
                l_getSourceTextOrFallback( l_callingExpressionReference, "" );

            logVariable( l_baseExpressionText );

            const clang::QualType l_qualifierType =
                l_callingExpressionReference->getType();

            if ( l_qualifierType.isNull() ) {
                logError( "Call expression has null type." );

                goto EXIT;
            }

            l_returnedQualifierType =
                l_qualifierType->getPointeeType().getUnqualifiedType();

            logVariable( l_qualifierType );

        } else if ( l_castingReference ) {
            // Explicit cast to type*
            // We matched hasDestinationType(pointer-to-type))
            l_pointerPassed = true;

            // Destination type's pointee is the type
            clang::QualType l_castingDestinationQualifierType =
                l_castingReference->getType();

            if ( l_castingDestinationQualifierType.isNull() ) {
                logError( "Cast has null destination type." );

                goto EXIT;
            }

            l_returnedQualifierType =
                l_castingDestinationQualifierType->getPointeeType()
                    .getUnqualifiedType();

            logVariable( l_returnedQualifierType );

            bool l_found = false;

            // Prefer a bound inner decl
            if ( l_castingDeclarationReference ) {
                l_variableDeclaration = llvm::dyn_cast< clang::VarDecl >(
                    l_castingDeclarationReference->getDecl() );

                logVariable( l_variableDeclaration );

                if ( l_variableDeclaration ) {
                    l_baseExpressionText = l_getSourceTextOrFallback(
                        l_castingDeclarationReference,
                        l_variableDeclaration->getNameAsString() );

                    logVariable( l_baseExpressionText );

                    l_found = true;
                }
            }

            // Else if casted - from call expression
            if ( !l_found && l_castingCallingExpression ) {
                l_baseExpressionText =
                    l_getSourceTextOrFallback( l_castingCallingExpression, "" );

                l_found = !l_baseExpressionText.empty();
            }

            // Else if casted - from member expression
            if ( !l_found && l_castingMemberExpression ) {
                l_baseExpressionText =
                    l_getSourceTextOrFallback( l_castingMemberExpression, "" );

                l_found = !l_baseExpressionText.empty();
            }

            // As a final fallback, use the whole cast expression text
            if ( !l_found ) {
                l_baseExpressionText =
                    l_getSourceTextOrFallback( l_castingReference, "" );

                l_found = !l_baseExpressionText.empty();
            }

            logVariable( l_baseExpressionText );

        } else {
            // Nothing matched
            // Should not happen if matcher and bindings are correct
            logError( "First-argument pattern not matched." );

            goto EXIT;
        }

        if ( l_returnedQualifierType.isNull() ) {
            logError( "Resolved qualifier type is null." );

            goto EXIT;
        }

        // NOTE: Messages are reported by checker
        if ( !std::forward< Checker >( _checker )( l_returnedQualifierType ) ) {
            goto EXIT;
        }

        const Type* l_type = l_returnedQualifierType->getAs< Type >();

        if ( !l_type ) {
            logError( "Failed to cast QualType to expected Type." );

            goto EXIT;
        }

        l_originalDeclaration = l_type->getOriginalDecl();

        logVariable( l_originalDeclaration );

        if ( !l_originalDeclaration ) {
            logError( "Original declaration missing on type." );

            goto EXIT;
        }

        l_originalDeclaration = l_originalDeclaration->getDefinition();

        logVariable( l_originalDeclaration );

        if ( !l_originalDeclaration ) {
            logError( std::string( "No definition for type (forward-decl): " ) +
                      l_variableDeclaration->getNameAsString() );

            goto EXIT;
        }
    }

EXIT:
    traceExit();

    return { l_callingExpression,   l_returnedQualifierType,
             l_originalDeclaration, l_baseExpressionText,
             l_pointerPassed,       l_callbackName };
}

} // namespace common
