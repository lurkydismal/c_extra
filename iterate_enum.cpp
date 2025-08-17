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

    clang::SourceManager& l_sm = *_result.SourceManager;
    clang::LangOptions l_lo = _result.Context->getLangOpts();

    // Helper: get token-range source text for an Expr, fallback on
    // 'fallback'
    auto l_getSourceTextOrFallback =
        [ & ]( const clang::Expr* _e,
               const std::string& _fallback ) -> std::string {
        if ( !_e ) {
            return ( _fallback );
        }

        clang::CharSourceRange l_r =
            clang::CharSourceRange::getTokenRange( _e->getSourceRange() );

        if ( !l_r.isValid() ) {
            return ( _fallback );
        }

        clang::Expected< clang::StringRef > l_txt =
            clang::Lexer::getSourceText( l_r, l_sm, l_lo );

        if ( l_txt ) {
            // TODO: Test
            traceExit();

            return ( l_txt->str() );
        }

        return ( _fallback );
    };

    // Helper: extract the underlying record QualType from a VarDecl.
    // If pointerPassed==true, try to return pointee type.
    auto l_getEnumTypeFromVar = [ & ](
                                    const clang::VarDecl* _vd,
                                    bool _pointerPassed ) -> clang::QualType {
        if ( !_vd )
            return {};

        clang::QualType l_qt = _vd->getType();

        if ( _pointerPassed ) {
            if ( l_qt->isPointerType() ) {
                return ( l_qt->getPointeeType().getUnqualifiedType() );
            }

            auto* l_rt = l_qt->getAs< clang::ReferenceType >();

            if ( l_rt ) {
                return ( ( l_rt->getPointeeType().getUnqualifiedType() ) );
            }
        }

        return ( l_qt.getUnqualifiedType() );
    };

    const auto* l_call =
        _result.Nodes.getNodeAs< clang::CallExpr >( "enumCall" );
    if ( !l_call )
        goto EXIT;

    {
        const auto* l_macroStr =
            _result.Nodes.getNodeAs< clang::StringLiteral >( "macroStr" );
        if ( !l_macroStr )
            goto EXIT;
        {
            std::string l_callbackName = l_macroStr->getString().str();
            logVariable( l_callbackName );
            log( "Found callback name: " + l_callbackName );

            // Bindings from matcher:
            const auto* l_addrDeclRef =
                _result.Nodes.getNodeAs< clang::DeclRefExpr >( "addrDeclRef" );
            const auto* l_ptrDeclRef =
                _result.Nodes.getNodeAs< clang::DeclRefExpr >( "ptrDeclRef" );
            const auto* l_arrayDeclRef =
                _result.Nodes.getNodeAs< clang::DeclRefExpr >( "arrayDeclRef" );
            const auto* l_callExprPtr =
                _result.Nodes.getNodeAs< clang::CallExpr >( "callExprPtr" );
            const auto* l_castToPtr =
                _result.Nodes.getNodeAs< clang::CStyleCastExpr >( "castToPtr" );
            const auto* l_castDeclRef =
                _result.Nodes.getNodeAs< clang::DeclRefExpr >( "castDeclRef" );
            const auto* l_castCallExpr =
                _result.Nodes.getNodeAs< clang::CallExpr >( "castCallExpr" );
            const auto* l_castMemberExpr =
                _result.Nodes.getNodeAs< clang::MemberExpr >(
                    "castMemberExpr" );

            std::string l_baseExprText;
            bool l_pointerPassed = false;
            clang::QualType l_enumQt;
            const clang::VarDecl* l_varDecl = nullptr;

            if ( l_addrDeclRef ) {
                l_pointerPassed = false;
                logVariable( l_pointerPassed );
                l_varDecl = llvm::dyn_cast< clang::VarDecl >(
                    l_addrDeclRef->getDecl() );
                if ( !l_varDecl ) {
                    logError( "addrDeclRef is not a VarDecl" );
                    goto EXIT;
                }
                logVariable( l_varDecl );
                l_baseExprText = l_getSourceTextOrFallback(
                    l_addrDeclRef, l_varDecl->getNameAsString() );
                logVariable( l_baseExprText );
                l_enumQt =
                    l_getEnumTypeFromVar( l_varDecl, /*pointerPassed=*/false );
                logVariable( l_enumQt );
            } else if ( l_ptrDeclRef ) {
                l_pointerPassed = true;
                logVariable( l_pointerPassed );
                l_varDecl =
                    llvm::dyn_cast< clang::VarDecl >( l_ptrDeclRef->getDecl() );
                if ( !l_varDecl ) {
                    logError( "ptrDeclRef is not a VarDecl" );
                    goto EXIT;
                }
                logVariable( l_varDecl );
                l_baseExprText = l_getSourceTextOrFallback(
                    l_ptrDeclRef, l_varDecl->getNameAsString() );
                logVariable( l_baseExprText );
                l_enumQt =
                    l_getEnumTypeFromVar( l_varDecl, /*pointerPassed=*/true );
                logVariable( l_enumQt );
            } else if ( l_arrayDeclRef ) {
                l_pointerPassed = true;
                logVariable( l_pointerPassed );
                l_varDecl = llvm::dyn_cast< clang::VarDecl >(
                    l_arrayDeclRef->getDecl() );
                if ( !l_varDecl ) {
                    logError( "arrayDeclRef is not a VarDecl" );
                    goto EXIT;
                }
                logVariable( l_varDecl );
                l_baseExprText = l_getSourceTextOrFallback(
                    l_arrayDeclRef, l_varDecl->getNameAsString() );
                logVariable( l_baseExprText );
                // Get element type of array
                if ( const clang::Type* l_tp =
                         l_varDecl->getType().getTypePtrOrNull() ) {
                    if ( auto* l_at =
                             llvm::dyn_cast< clang::ArrayType >( l_tp ) ) {
                        l_enumQt = l_at->getElementType().getUnqualifiedType();
                    } else {
                        l_enumQt = l_getEnumTypeFromVar(
                            l_varDecl, /*pointerPassed=*/false );
                    }
                } else {
                    l_enumQt = l_getEnumTypeFromVar( l_varDecl,
                                                     /*pointerPassed=*/false );
                }
                logVariable( l_enumQt );
            } else if ( l_callExprPtr ) {
                l_pointerPassed = true;
                logVariable( l_pointerPassed );
                l_baseExprText = l_getSourceTextOrFallback( l_callExprPtr, "" );
                logVariable( l_baseExprText );
                clang::QualType l_t = l_callExprPtr->getType();
                if ( !l_t.isNull() )
                    l_enumQt = l_t->getPointeeType().getUnqualifiedType();
                logVariable( l_enumQt );
            } else if ( l_castToPtr ) {
                l_pointerPassed = true;
                logVariable( l_pointerPassed );
                clang::QualType l_castDest = l_castToPtr->getType();
                if ( !l_castDest.isNull() )
                    l_enumQt =
                        l_castDest->getPointeeType().getUnqualifiedType();
                logVariable( l_enumQt );
                // Try to recover inner expression for base text
                if ( l_castDeclRef ) {
                    l_varDecl = llvm::dyn_cast< clang::VarDecl >(
                        l_castDeclRef->getDecl() );
                    if ( l_varDecl ) {
                        logVariable( l_varDecl );
                        l_baseExprText = l_getSourceTextOrFallback(
                            l_castDeclRef, l_varDecl->getNameAsString() );
                        logVariable( l_baseExprText );
                    }
                }
                if ( l_baseExprText.empty() && l_castCallExpr ) {
                    l_baseExprText =
                        l_getSourceTextOrFallback( l_castCallExpr, "" );
                    logVariable( l_baseExprText );
                }
                if ( l_baseExprText.empty() && l_castMemberExpr ) {
                    l_baseExprText =
                        l_getSourceTextOrFallback( l_castMemberExpr, "" );
                    logVariable( l_baseExprText );
                }
                if ( l_baseExprText.empty() ) {
                    l_baseExprText =
                        l_getSourceTextOrFallback( l_castToPtr, "" );
                    logVariable( l_baseExprText );
                }
            } else {
                logError( "No matching first-argument pattern found" );
                goto EXIT;
            }

            if ( l_enumQt.isNull() ) {
                logError( "Enum type is null" );
                goto EXIT;
            }
            {
                const clang::EnumType* l_et =
                    l_enumQt->getAs< clang::EnumType >();
                if ( !l_et ) {
                    logError( "First argument is not an enum type" );
                    goto EXIT;
                }
                clang::EnumDecl* l_ed = l_et->getOriginalDecl();
                clang::EnumDecl* l_rd = l_ed->getDefinition();
                if ( !l_rd ) {
                    logError( "Enum has no definition (forward-decl): " +
                              l_varDecl->getNameAsString() );
                    goto EXIT;
                }
                logVariable( l_rd );

                {
                    // Underlying integer type of enum
                    clang::QualType l_underQt = l_rd->getIntegerType();
                    std::string l_underTypeStr;

                    if ( auto* l_tdt =
                             l_underQt->getAs< clang::TypedefType >() ) {
                        l_underTypeStr = l_tdt->getDecl()->getNameAsString();
                    } else {
                        l_underTypeStr = l_underQt.getAsString();
                    }
                    logVariable( l_underTypeStr );

                    // Determine indent for the generated lines
                    clang::SourceLocation l_startLoc =
                        l_sm.getSpellingLoc( l_call->getBeginLoc() );
                    unsigned l_col = l_sm.getSpellingColumnNumber( l_startLoc );
                    std::string l_indent( ( l_col > 0 ) ? ( l_col - 1 ) : 0,
                                          ' ' );

                    // Generate replacement text: one callback call per
                    // enumerator
                    std::string l_replacementText;
                    llvm::raw_string_ostream l_ss( l_replacementText );
                    for ( const clang::EnumConstantDecl* l_ec :
                          l_rd->enumerators() ) {
                        if ( !l_ec || l_ec->getNameAsString().empty() )
                            continue;
                        std::string l_name = l_ec->getNameAsString();
                        llvm::APSInt l_val = l_ec->getInitVal();
                        clang::SmallVector< char > l_valStr;
                        l_val.toString( l_valStr );
                        l_ss << l_indent << l_callbackName << "(\"" << l_name
                             << "\", "
                             << "\"" << l_underTypeStr << "\", "
                             << "(" << l_underTypeStr << ")" << l_valStr
                             << ");\n";
                    }
                    l_ss.flush();

                    // Remove indent on first line and trailing newline
                    l_replacementText.erase( 0, l_indent.length() );
                    if ( !l_replacementText.empty() &&
                         l_replacementText.back() == '\n' )
                        l_replacementText.pop_back();
                    logVariable( l_replacementText );

                    // Replace the call (including semicolon if present)
                    clang::SourceLocation l_endLoc = l_call->getEndLoc();
                    clang::SourceLocation l_afterCall =
                        clang::Lexer::getLocForEndOfToken( l_endLoc, 0, l_sm,
                                                           l_lo );
                    bool l_includeSemi = false;
                    if ( l_afterCall.isValid() ) {
                        clang::SourceLocation l_afterSpelling =
                            l_sm.getSpellingLoc( l_afterCall );
                        bool l_invalid = false;
                        const char* l_c = l_sm.getCharacterData(
                            l_afterSpelling, &l_invalid );
                        if ( !l_invalid && l_c && *l_c == ';' ) {
                            l_includeSemi = true;
                        }
                    }
                    clang::SourceLocation l_replaceEnd =
                        ( l_includeSemi ? l_afterCall.getLocWithOffset( 1 )
                                        : l_endLoc );
                    clang::CharSourceRange l_toReplace =
                        clang::CharSourceRange::getCharRange(
                            l_call->getBeginLoc(), l_replaceEnd );
                    if ( !l_toReplace.isValid() ||
                         !_rewriter.getSourceMgr().isWrittenInMainFile(
                             l_toReplace.getBegin() ) ) {
                        logError( "Invalid range for rewrite" );
                        goto EXIT;
                    }
                    {
                        std::string l_existing =
                            _rewriter.getRewrittenText( l_toReplace );
                        if ( !l_existing.empty() ) {
                            logError( "Existing rewritten text: " +
                                      l_existing );
                        }
                        _rewriter.ReplaceText( l_toReplace, l_replacementText );
                        log( "performed_replace" );
                    }
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
    auto l_ptrToEnum = pointsTo( l_isEnumType );
    auto l_arrayOfEnum = arrayType( hasElementType( l_isEnumType ) );

    // First-argument possibilities:
    //  - &variable              -> bind "addrDeclRef"
    //  - variable*              -> bind "ptrDeclRef"
    //  - Array of enum          -> bind "arrayDeclRef" (decays to pointer)
    //  - getEnumPointer()       -> bind "callExprPtr" (call expression that
    //  yields enum*)
    //  - (enum S*)expression    -> bind "castToPtr" and inner nodes if
    //  available
    auto l_firstArgument = anyOf(
        // &enum
        unaryOperator(
            hasOperatorName( "&" ),
            hasUnaryOperand( ignoringParenImpCasts(
                declRefExpr( to( varDecl( hasType( l_isEnumType ) ) ) )
                    .bind( "addrDeclRef" ) ) ) ),

        // enum*
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_ptrToEnum ) ) ) )
                .bind( "ptrDeclRef" ) ),

        // Array of enum
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_arrayOfEnum ) ) ) )
                .bind( "arrayDeclRef" ) ),

        // getEnumPointer()
        ignoringParenImpCasts(
            callExpr( hasType( l_ptrToEnum ) ).bind( "callExprPtr" ) ),

        // (enum S*)expression
        ignoringParenImpCasts(
            cStyleCastExpr(
                hasDestinationType( l_ptrToEnum ),
                hasSourceExpression( ignoringParenImpCasts( anyOf(
                    declRefExpr( to( varDecl( hasType( l_isEnumType ) ) ) )
                        .bind( "castDeclRef" ),
                    callExpr().bind( "castCallExpr" ),
                    memberExpr().bind( "castMemberExpr" ) ) ) ) )
                .bind( "castToPtr" ) ) );

    // Match calls to iterate_enum(&struct, "callback")
    _matcher.addMatcher(
        callExpr( callee( functionDecl( hasName( "iterate_enum" ) ) ),
                  hasArgument( 0, l_firstArgument ),
                  hasArgument( 1, stringLiteral().bind( "macroStr" ) ) )
            .bind( "enumCall" ),
        l_handler.release() );

    // Canonical record type matcher (handles typedefs/ quals)
    auto l_isRecordType = qualType( hasCanonicalType( recordType() ) );

    // Helper matchers
    auto l_ptrToRecord = pointsTo( l_isRecordType );
    auto l_arrayOfRecord = arrayType( hasElementType( l_isRecordType ) );

    traceExit();
}
