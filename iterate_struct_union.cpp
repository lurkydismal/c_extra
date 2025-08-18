#include "iterate_struct_union.hpp"

#include <clang/Lex/Lexer.h>

#include <memory>

#include "clang/AST/Expr.h"
#include "log.hpp"
#include "trace.hpp"

using namespace clang::ast_matchers;

IterateStructUnionHandler::IterateStructUnionHandler(
    clang::Rewriter& _rewriter )
    : _rewriter( _rewriter ) {
    traceEnter();

    traceExit();
}

void IterateStructUnionHandler::run( const MatchFinder::MatchResult& _result ) {
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

    // Helper: extract the underlying record QualType from a VarDecl.
    // NOTE: If pointerPassed==true, try to return pointee type.
    auto l_getRecordTypeFromVariable =
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
        _result.Nodes.getNodeAs< clang::CallExpr >( "iterateStructUnionCall" );

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

        const clang::StringRef l_callbackName =
            l_callbackNameLiteral->getString();

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

        clang::StringRef l_baseExpressionText;
        bool l_pointerPassed = false;
        clang::QualType l_recordQualifierType;
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

                l_recordQualifierType = l_getRecordTypeFromVariable(
                    l_variableDeclaration, l_pointerPassed );

                logVariable( l_recordQualifierType );

            EXIT:
                traceExit();
            };

        // Order of precedence: &struct, struct*, array of struct,
        // getStructPointer(), (struct S*)expression (if casted from declref),
        // (struct S*)expression.
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
                    l_recordQualifierType =
                        l_arrayType->getElementType().getUnqualifiedType();

                    logVariable( l_recordQualifierType );
                }
            }

        } else if ( l_callingExpressionReference ) {
            // Call expression that returns struct*
            l_pointerPassed = true;

            l_baseExpressionText =
                l_getSourceTextOrFallback( l_callingExpressionReference, "" );

            logVariable( l_baseExpressionText );

            const clang::QualType l_qualifierType =
                l_callingExpressionReference->getType();

            if ( l_qualifierType.isNull() ) {
                logError( "TODO: WRITE" );

                goto EXIT;
            }

            l_recordQualifierType =
                l_qualifierType->getPointeeType().getUnqualifiedType();

            logVariable( l_recordQualifierType );

        } else if ( l_castingReference ) {
            // Explicit cast to struct*
            // We matched hasDestinationType(pointer-to-record))
            l_pointerPassed = true;

            // Destination type's pointee is the record type
            clang::QualType l_castingDestinationQualifierType =
                l_castingReference->getType();

            if ( l_castingDestinationQualifierType.isNull() ) {
                logError( "TODO: WRITE2" );

                goto EXIT;
            }

            l_recordQualifierType =
                l_castingDestinationQualifierType->getPointeeType()
                    .getUnqualifiedType();

            logVariable( l_recordQualifierType );

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
            logError( "No matching first-argument pattern found." );

            goto EXIT;
        }

        if ( l_recordQualifierType.isNull() ) {
            logError( "Record type is null." );

            goto EXIT;
        }

        if ( ( !l_recordQualifierType->isStructureType() ) &&
             ( !l_recordQualifierType->isUnionType() ) ) {
            logError( "First arg is not a struct/union type." );

            goto EXIT;
        }

        const clang::RecordType* l_recordType =
            l_recordQualifierType->getAs< clang::RecordType >();

        if ( !l_recordType ) {
            logError( "RecordType cast failed." );

            goto EXIT;
        }

        clang::RecordDecl* l_recordOriginalDeclaration =
            l_recordType->getOriginalDecl();

        logVariable( l_recordOriginalDeclaration );

        if ( !l_recordOriginalDeclaration ) {
            logError( "Original RecordDecl missing." );

            goto EXIT;
        }

        l_recordOriginalDeclaration =
            l_recordOriginalDeclaration->getDefinition();

        logVariable( l_recordOriginalDeclaration );

        if ( !l_recordOriginalDeclaration ) {
            logError(
                std::string( "Record has no definition (forward-decl): " ) +
                l_variableDeclaration->getNameAsString() );

            goto EXIT;
        }

        // offsetof/ sizeof
        std::string l_recordTypeString;

        // Build record type string
        {
            const auto* l_typedefType =
                l_recordQualifierType->getAs< clang::TypedefType >();

            // Extract typedef
            if ( l_typedefType ) {
                l_recordTypeString =
                    l_typedefType->getDecl()->getNameAsString();

            } else {
                l_recordTypeString = l_recordQualifierType.getAsString();
            }
        }

        logVariable( l_recordTypeString );

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

            auto l_appendFieldCallToReplacementText =
                [ & ]( const clang::FieldDecl* _fieldDeclaration ) {
                    traceEnter();

                    if ( ( !_fieldDeclaration ) ||
                         ( _fieldDeclaration->isUnnamedBitField() ) ||
                         ( !_fieldDeclaration->getIdentifier() ) ) {
                        goto EXIT;
                    }

                    {
                        std::string l_fieldName =
                            _fieldDeclaration->getNameAsString();

                        logVariable( l_fieldName );

                        if ( l_fieldName.empty() ) {
                            goto EXIT;
                        }

                        std::string l_fieldType =
                            _fieldDeclaration->getType().getAsString();

                        logVariable( l_fieldType );

                        std::string l_memberAccess;

                        // Build member access
                        {
                            const std::string l_baseExpressionTextString =
                                l_baseExpressionText.str();

                            l_memberAccess =
                                ( ( l_pointerPassed )
                                      ? ( "(" + l_baseExpressionTextString +
                                          ")->" + l_fieldName )
                                      : ( l_baseExpressionTextString + "." +
                                          l_fieldName ) );
                        }

                        const std::string l_fieldReference =
                            ( "&(" + l_memberAccess + ")" );

                        logVariable( l_fieldReference );

                        // callbackName(
                        //   "fieldName",
                        //   "fieldType",
                        //   &( ( variable )->field ),
                        //   __builtin_offsetof( structType, field ),
                        //   sizeof( ( ( structType* )0 )->field ) );
                        l_replacementTextStringStream
                            << l_indent << l_callbackName << "(" << "\""
                            << l_fieldName << "\", \"" << l_fieldType << "\", "
                            << l_fieldReference << ", "
                            << "__builtin_offsetof(" << l_recordTypeString
                            << ", " << l_fieldName << "), "
                            << "sizeof(((" << l_recordTypeString << "*)0)->"
                            << l_fieldName << ")" << ");\n";
                    }

                EXIT:
                    traceExit();
                };

            for ( const clang::FieldDecl* l_fieldDeclaration :
                  l_recordOriginalDeclaration->fields() ) {
                l_appendFieldCallToReplacementText( l_fieldDeclaration );
            }

            l_replacementTextStringStream.flush();

            l_replacementText.erase( 0, l_indent.length() );

            if ( ( !l_replacementText.empty() ) &&
                 ( l_replacementText.back() == '\n' ) ) {
                l_replacementText.pop_back();
            }
        }

        logVariable( l_replacementText );

        // Replace the entire call (including semicolon) with replacement text
        {
            clang::CharSourceRange l_sourceRangeToReplace;

            {
                const clang::SourceLocation l_sourceEndLocation =
                    l_callingExpression->getEndLoc();
                // TODO:Rename
                const clang::SourceLocation l_lastCharacterSourceLocation =
                    clang::Lexer::getLocForEndOfToken( l_sourceEndLocation, 0,
                                                       l_sourceManager,
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
                            l_sourceManager.getCharacterData( l_afterSpelling,
                                                              &l_isInvalid );

                        if ( ( !l_isInvalid ) && ( l_lastCharacter ) &&
                             ( *l_lastCharacter == ';' ) ) {
                            l_isSemicolonIncluded = true;
                        }
                    }
                }

                const clang::SourceLocation l_replacementSourceEndLocation =
                    ( ( l_isSemicolonIncluded )
                          ? ( l_lastCharacterSourceLocation.getLocWithOffset(
                                1 ) )
                          : ( l_sourceEndLocation ) );

                l_sourceRangeToReplace = clang::CharSourceRange::getCharRange(
                    l_callingExpression->getBeginLoc(),
                    l_replacementSourceEndLocation );
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

            _rewriter.ReplaceText( l_sourceRangeToReplace, l_replacementText );

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
    }

EXIT:
    traceExit();
}

void IterateStructUnionHandler::addMatcher( MatchFinder& _matcher,
                                            clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateStructUnionHandler >( _rewriter );

    // Canonical record type matcher (handles typedefs/ quals)
    auto l_isRecordType = qualType( hasCanonicalType( recordType() ) );

    // Helper matchers
    auto l_pointerToRecord = pointsTo( l_isRecordType );
    auto l_arrayOfRecord = arrayType( hasElementType( l_isRecordType ) );

    // TODO: Imrpve bind names
    // First-argument possibilities:
    //  - &struct                 -> bind "addressDeclarationReference"
    //  - struct*                 -> bind "pointerDeclarationReference"
    //  - Array of struct         -> bind "arrayDeclarationReference" (decays to
    //  pointer)
    //  - getStructPointer()      -> bind "callingExpressionReference" (call
    //  expression that yields struct*)
    //  - (struct S*)expression   -> bind "castingReference" and inner nodes if
    //  available
    auto l_firstArgument = anyOf(
        // &struct
        unaryOperator(
            hasOperatorName( "&" ),
            hasUnaryOperand( ignoringParenImpCasts(
                declRefExpr( to( varDecl( hasType( l_isRecordType ) ) ) )
                    .bind( "addressDeclarationReference" ) ) ) ),

        // struct*
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_pointerToRecord ) ) ) )
                .bind( "pointerDeclarationReference" ) ),

        // Array of struct
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_arrayOfRecord ) ) ) )
                .bind( "arrayDeclarationReference" ) ),

        // getStructPointer()
        ignoringParenImpCasts( callExpr( hasType( l_pointerToRecord ) )
                                   .bind( "callingExpressionReference" ) ),

        // (struct S*)expression
        ignoringParenImpCasts(
            cStyleCastExpr(
                hasDestinationType( l_pointerToRecord ),
                hasSourceExpression( ignoringParenImpCasts( anyOf(
                    declRefExpr( to( varDecl( hasType( l_isRecordType ) ) ) )
                        .bind( "castingDeclarationReference" ),
                    callExpr().bind( "castingCallingExpression" ),
                    memberExpr().bind( "castingMemberExpression" ) ) ) ) )
                .bind( "castingReference" ) ) );

    // Match calls to:
    // iterate_struct(&struct, "callback")
    // iterate_union(&struct, "callback")
    // iterate_struct_union(&struct, "callback")
    _matcher.addMatcher(
        callExpr(
            callee( functionDecl( hasAnyName( "iterate_struct", "iterate_union",
                                              "iterate_struct_union" ) ) ),
            hasArgument( 0, l_firstArgument ),
            hasArgument( 1, stringLiteral().bind( "callbackName" ) ) )
            .bind( "iterateStructUnionCall" ),
        l_handler.release() );

    traceExit();
}
