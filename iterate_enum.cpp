#include "iterate_enum.hpp"

#include <clang/Lex/Lexer.h>

#include <memory>

#include "log.hpp"
#include "trace.hpp"

using namespace clang::ast_matchers;

IterateEnumHandler::IterateEnumHandler( clang::Rewriter& _rewriter )
    : _rewriter( _rewriter ) {
    traceEnter();

    traceExit();
}

void IterateEnumHandler::run( const MatchFinder::MatchResult& _result ) {
    traceEnter();

    clang::SourceManager& l_sourceManager = *( _result.SourceManager );
    clang::LangOptions l_langOptions = _result.Context->getLangOpts();

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

    // Helper: extract the underlying enum QualType from a VarDecl.
    // NOTE: If pointerPassed==true, try to return pointee type.
    auto l_getEnumTypeFromVariable =
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
        _result.Nodes.getNodeAs< clang::CallExpr >( "iterateEnumCall" );

    logVariable( l_callingExpression );

    if ( !l_callingExpression ) {
        goto EXIT;
    }

    {
        const auto* l_callbackNameLiteral =
            _result.Nodes.getNodeAs< clang::StringLiteral >( "callbackName" );

        if ( !l_callbackNameLiteral ) {
            goto EXIT;
        }

        {
            std::string l_callbackName =
                l_callbackNameLiteral->getString().str();
            logVariable( l_callbackName );

            // Bindings from matcher:
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

            clang::StringRef l_baseExpressionText;
            bool l_pointerPassed = false;
            clang::QualType l_enumQualifierType;
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
                        logError( "Does not refer to a VarDecl" );

                        goto EXIT;
                    }

                    l_baseExpressionText = l_getSourceTextOrFallback(
                        _declarationReference,
                        l_variableDeclaration->getNameAsString() );

                    logVariable( l_baseExpressionText );

                    l_enumQualifierType = l_getEnumTypeFromVariable(
                        l_variableDeclaration, l_pointerPassed );

                    logVariable( l_enumQualifierType );

                EXIT:
                    traceExit();
                };

            // Order of precedence: &struct, struct*, array of struct,
            // getStructPointer(), (struct S*)expression (if casted from
            // declref), (struct S*)expression.
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
                    const auto* l_arrayType =
                        llvm::dyn_cast< clang::ArrayType >(
                            l_variableDeclarationType );

                    if ( l_arrayType ) {
                        l_enumQualifierType =
                            l_arrayType->getElementType().getUnqualifiedType();

                        logVariable( l_enumQualifierType );
                    }
                }

            } else if ( l_callingExpressionReference ) {
                // Call expression that returns struct*
                l_pointerPassed = true;

                l_baseExpressionText = l_getSourceTextOrFallback(
                    l_callingExpressionReference, "" );

                logVariable( l_baseExpressionText );

                const clang::QualType l_qualifierType =
                    l_callingExpressionReference->getType();

                if ( l_qualifierType.isNull() ) {
                    logError( "TODO: WRITE" );

                    goto EXIT;
                }

                l_enumQualifierType =
                    l_qualifierType->getPointeeType().getUnqualifiedType();

                logVariable( l_enumQualifierType );

            } else if ( l_castingReference ) {
                // Explicit cast to struct*
                // We matched hasDestinationType(pointer-to-enum))
                l_pointerPassed = true;

                // Destination type's pointee is the enum type
                clang::QualType l_castingDestinationQualifierType =
                    l_castingReference->getType();

                if ( l_castingDestinationQualifierType.isNull() ) {
                    logError( "TODO: WRITE2" );

                    goto EXIT;
                }

                l_enumQualifierType =
                    l_castingDestinationQualifierType->getPointeeType()
                        .getUnqualifiedType();

                logVariable( l_enumQualifierType );

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
                    l_baseExpressionText = l_getSourceTextOrFallback(
                        l_castingCallingExpression, "" );

                    l_found = !l_baseExpressionText.empty();
                }

                // Else if casted - from member expression
                if ( !l_found && l_castingMemberExpression ) {
                    l_baseExpressionText = l_getSourceTextOrFallback(
                        l_castingMemberExpression, "" );

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
                logError( "No matching first-argument pattern found." );

                goto EXIT;
            }

            if ( l_enumQualifierType.isNull() ) {
                logError( "Enum type is null" );

                goto EXIT;
            }

            const clang::EnumType* l_enumType =
                l_enumQualifierType->getAs< clang::EnumType >();

            if ( !l_enumType ) {
                logError( "First argument is not an enum type" );

                goto EXIT;
            }

            clang::EnumDecl* l_enumDeclaration = l_enumType->getOriginalDecl();

            logVariable( l_enumDeclaration );

            if ( !l_enumDeclaration ) {
                logError( "Original EnumDecl missing." );

                goto EXIT;
            }

            l_enumDeclaration = l_enumDeclaration->getDefinition();

            logVariable( l_enumDeclaration );

            if ( !l_enumDeclaration ) {
                logError( "Enum has no definition (forward-decl): " +
                          l_variableDeclaration->getNameAsString() );

                goto EXIT;
            }

            // Underlying integer type of enum
            std::string l_fieldUnderlyingType;

            {
                const clang::QualType l_underlyingQualifierType =
                    l_enumDeclaration->getIntegerType();

                const auto* l_typedefType =
                    l_underlyingQualifierType->getAs< clang::TypedefType >();

                if ( l_typedefType ) {
                    l_fieldUnderlyingType =
                        l_typedefType->getDecl()->getNameAsString();

                } else {
                    l_fieldUnderlyingType =
                        l_underlyingQualifierType.getAsString();
                }
            }

            logVariable( l_fieldUnderlyingType );

            std::string l_replacementText;

            // Build replacement text
            {
                // Determine indentation from call location
                // Use spelling loc for column
                const clang::SourceLocation l_sourceStartLocation =
                    l_sourceManager.getSpellingLoc(
                        l_callingExpression->getBeginLoc() );
                uint l_spellingColumnNumber =
                    l_sourceManager.getSpellingColumnNumber(
                        l_sourceStartLocation );
                l_spellingColumnNumber = ( ( l_spellingColumnNumber > 0 )
                                               ? ( l_spellingColumnNumber - 1 )
                                               : ( 0 ) );
                const std::string l_indent( l_spellingColumnNumber, ' ' );

                llvm::raw_string_ostream l_replacementTextStringStream(
                    l_replacementText );

                for ( const clang::EnumConstantDecl* l_ec :
                      l_enumDeclaration->enumerators() ) {
                    if ( !l_ec || l_ec->getNameAsString().empty() )
                        continue;
                    std::string l_fieldName = l_ec->getNameAsString();

                    logVariable( l_fieldName );

                    if ( l_fieldName.empty() ) {
                        logError( "Field has no name." );

                        goto EXIT;
                    }

                    llvm::APSInt l_fieldValue = l_ec->getInitVal();

                    logVariable( l_fieldValue );

                    clang::SmallVector< char > l_fieldValueAsString;

                    l_fieldValue.toString( l_fieldValueAsString );

                    logVariable( l_fieldValueAsString );

                    // callbackName(
                    //   "fieldName",
                    //   "fieldType",
                    //   fieldValue,
                    //   sizeof( fieldType ) );
                    l_replacementTextStringStream
                        << l_indent << l_callbackName << "(" << "\""
                        << l_fieldName << "\", "
                        << "\"" << l_fieldUnderlyingType << "\", "
                        << "(" << l_fieldUnderlyingType << ")"
                        << l_fieldValueAsString << ", " << "sizeof("
                        << l_fieldUnderlyingType << ")" << ");\n";
                }

                l_replacementTextStringStream.flush();

                // Remove indent on first line and trailing newline
                l_replacementTextStringStream.flush();

                l_replacementText.erase( 0, l_indent.length() );

                if ( ( !l_replacementText.empty() ) &&
                     ( l_replacementText.back() == '\n' ) ) {
                    l_replacementText.pop_back();
                }
            }
            logVariable( l_replacementText );

            // Replace the entire call (including semicolon) with
            // replacement text
            {
                clang::CharSourceRange l_sourceRangeToReplace;

                {
                    const clang::SourceLocation l_sourceEndLocation =
                        l_callingExpression->getEndLoc();
                    // TODO:Rename
                    const clang::SourceLocation l_lastCharacterSourceLocation =
                        clang::Lexer::getLocForEndOfToken( l_sourceEndLocation,
                                                           0, l_sourceManager,
                                                           l_langOptions );

                    bool l_isSemicolonIncluded = false;

                    if ( l_lastCharacterSourceLocation.isValid() ) {
                        // TODO: Rename
                        const clang::SourceLocation l_afterSpelling =
                            l_sourceManager.getSpellingLoc(
                                l_lastCharacterSourceLocation );

                        if ( l_afterSpelling.isValid() ) {
                            bool l_isInvalid = false;

                            const char* l_lastCharacter =
                                l_sourceManager.getCharacterData(
                                    l_afterSpelling, &l_isInvalid );

                            if ( ( !l_isInvalid ) && ( l_lastCharacter ) &&
                                 ( *l_lastCharacter == ';' ) ) {
                                l_isSemicolonIncluded = true;
                            }
                        }
                    }

                    const clang::SourceLocation l_replacementSourceEndLocation =
                        ( ( l_isSemicolonIncluded )
                              ? ( l_lastCharacterSourceLocation
                                      .getLocWithOffset( 1 ) )
                              : ( l_sourceEndLocation ) );

                    l_sourceRangeToReplace =
                        clang::CharSourceRange::getCharRange(
                            l_callingExpression->getBeginLoc(),
                            l_replacementSourceEndLocation );
                }

                // FIX: Log this
                // logVariable( l_sourceRangeToReplace );

                // Only attempt to query rewritten text/ replace if the
                // range is valid and in main file
                if ( ( !l_sourceRangeToReplace.isValid() ) ||
                     ( !_rewriter.getSourceMgr().isWrittenInMainFile(
                         l_sourceRangeToReplace.getBegin() ) ) ) {
                    logError( "Invalid or non-main file range for rewrite." );

                    goto EXIT;
                }

                _rewriter.ReplaceText( l_sourceRangeToReplace,
                                       l_replacementText );

                // Debug existing rewritten text safely
                const std::string l_existing =
                    _rewriter.getRewrittenText( l_sourceRangeToReplace );

                if ( l_existing.empty() ) {
                    logError(
                        "Existing rewritten text is empty or not yet "
                        "rewritten." );

                } else {
                    log( "Existing rewritten text: " + l_existing );
                }
            }
        }
    }

EXIT:
    traceExit();
}

void IterateEnumHandler::addMatcher( MatchFinder& _matcher,
                                     clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateEnumHandler >( _rewriter );

    // Canonical enum type matcher (handles typedefs/ quals)
    auto l_isEnumType = qualType( hasCanonicalType( enumType() ) );

    // Helper matchers
    auto l_pointerToEnum = pointsTo( l_isEnumType );
    auto l_arrayOfEnum = arrayType( hasElementType( l_isEnumType ) );

    // First-argument possibilities:
    //  - &variable              -> bind "addressDeclarationReference"
    //  - variable*              -> bind "pointerDeclarationReference"
    //  - Array of enum          -> bind "arrayDeclarationReference" (decays to
    //  pointer)
    //  - getEnumPointer()       -> bind "callingExpressionReference" (call
    //  expression that yields enum*)
    //  - (enum S*)expression    -> bind "castingReference" and inner nodes if
    //  available
    auto l_firstArgument = anyOf(
        // &enum
        unaryOperator(
            hasOperatorName( "&" ),
            hasUnaryOperand( ignoringParenImpCasts(
                declRefExpr( to( varDecl( hasType( l_isEnumType ) ) ) )
                    .bind( "addressDeclarationReference" ) ) ) ),

        // enum*
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_pointerToEnum ) ) ) )
                .bind( "pointerDeclarationReference" ) ),

        // Array of enum
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_arrayOfEnum ) ) ) )
                .bind( "arrayDeclarationReference" ) ),

        // getEnumPointer()
        ignoringParenImpCasts( callExpr( hasType( l_pointerToEnum ) )
                                   .bind( "callingExpressionReference" ) ),

        // (enum S*)expression
        ignoringParenImpCasts(
            cStyleCastExpr(
                hasDestinationType( l_pointerToEnum ),
                hasSourceExpression( ignoringParenImpCasts( anyOf(
                    declRefExpr( to( varDecl( hasType( l_isEnumType ) ) ) )
                        .bind( "castingDeclarationReference" ),
                    callExpr().bind( "castingCallingExpression" ),
                    memberExpr().bind( "castingMemberExpression" ) ) ) ) )
                .bind( "castingReference" ) ) );

    // Match calls to iterate_enum(&enum, "callback")
    _matcher.addMatcher(
        callExpr( callee( functionDecl( hasName( "iterate_enum" ) ) ),
                  hasArgument( 0, l_firstArgument ),
                  hasArgument( 1, stringLiteral().bind( "callbackName" ) ) )
            .bind( "iterateEnumCall" ),
        l_handler.release() );

    traceExit();
}
