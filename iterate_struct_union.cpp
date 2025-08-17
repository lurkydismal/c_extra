#include "iterate_struct_union.hpp"

#include <clang/Lex/Lexer.h>

#include <memory>

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

    clang::SourceManager& l_sm = *( _result.SourceManager );
    clang::LangOptions l_lo = _result.Context->getLangOpts();

    // Helper: get token-range source text for an Expr, fallback on 'fallback'
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
    auto l_getRecordTypeFromVar = [ & ](
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
        _result.Nodes.getNodeAs< clang::CallExpr >( "iterateStructUnionCall" );

    if ( !l_call ) {
        goto EXIT;
    }

    {
        // 2nd arguemnt
        const auto* l_macroStr =
            _result.Nodes.getNodeAs< clang::StringLiteral >( "macroStr" );

        if ( !l_macroStr ) {
            goto EXIT;
        }

        std::string l_callbackName = l_macroStr->getString().str();

        logVariable( l_callbackName );

        log( std::string( "Found macro name: " ) + l_callbackName );

        // 1nd arguemnt
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
            _result.Nodes.getNodeAs< clang::MemberExpr >( "castMemberExpr" );

        // Prepare variables to fill
        std::string l_baseExprText;
        logVariable( l_baseExprText ); // log after we overwrite below as
                                       // required (no-op now)

        bool l_pointerPassed = false;
        logVariable( l_pointerPassed ); // logged below immediately after actual
                                        // assignment

        clang::QualType l_recordQt;
        const clang::VarDecl* l_varDecl = nullptr;

        // Order of precedence: explicit address-of var, pointer var, array var,
        // cast-to-pointer (if casted-from declref), callExpr returning ptr,
        // cast-to-pointer general.
        if ( l_addrDeclRef ) {
            // &var  -> base is var, pointerPassed = false
            l_pointerPassed = false;

            logVariable( l_pointerPassed );

            l_varDecl =
                llvm::dyn_cast< clang::VarDecl >( l_addrDeclRef->getDecl() );

            if ( !l_varDecl ) {
                logError( "addrDeclRef does not refer to a VarDecl" );

                goto EXIT;
            }

            logVariable( l_varDecl );

            l_baseExprText = l_getSourceTextOrFallback(
                l_addrDeclRef, l_varDecl->getNameAsString() );

            logVariable( l_baseExprText );

            l_recordQt =
                l_getRecordTypeFromVar( l_varDecl, /*pointerPassed=*/false );

            logVariable( l_recordQt );

        } else if ( l_ptrDeclRef ) {
            // pointer variable passed -> base is var, pointerPassed = true
            l_pointerPassed = true;

            logVariable( l_pointerPassed );

            l_varDecl =
                llvm::dyn_cast< clang::VarDecl >( l_ptrDeclRef->getDecl() );

            if ( !l_varDecl ) {
                logError( "ptrDeclRef does not refer to a VarDecl" );

                goto EXIT;
            }

            logVariable( l_varDecl );

            l_baseExprText = l_getSourceTextOrFallback(
                l_ptrDeclRef, l_varDecl->getNameAsString() );

            logVariable( l_baseExprText );

            l_recordQt =
                l_getRecordTypeFromVar( l_varDecl, /*pointerPassed=*/true );

            logVariable( l_recordQt );

        } else if ( l_arrayDeclRef ) {
            // array variable passed (decays to pointer)
            l_pointerPassed = true;

            logVariable( l_pointerPassed );

            l_varDecl =
                llvm::dyn_cast< clang::VarDecl >( l_arrayDeclRef->getDecl() );

            if ( !l_varDecl ) {
                logError( "arrayDeclRef does not refer to a VarDecl" );

                goto EXIT;
            }

            logVariable( l_varDecl );

            l_baseExprText = l_getSourceTextOrFallback(
                l_arrayDeclRef, l_varDecl->getNameAsString() );

            logVariable( l_baseExprText );

            // Extract element type from array type
            if ( const clang::Type* l_tp =
                     l_varDecl->getType().getTypePtrOrNull() ) {
                if ( const auto* l_at =
                         llvm::dyn_cast< clang::ArrayType >( l_tp ) ) {
                    l_recordQt = l_at->getElementType().getUnqualifiedType();

                } else {
                    // fallback: try unwrapping typedefs (defensive)
                    l_recordQt = l_getRecordTypeFromVar(
                        l_varDecl, /*pointerPassed=*/false );
                }

            } else {
                l_recordQt = l_getRecordTypeFromVar( l_varDecl,
                                                     /*pointerPassed=*/false );
            }
            logVariable( l_recordQt );

        } else if ( l_callExprPtr ) {
            // call expression that returns struct* (e.g. getStructPtr())
            l_pointerPassed = true;
            logVariable( l_pointerPassed );

            l_baseExprText = l_getSourceTextOrFallback( l_callExprPtr, "" );
            logVariable( l_baseExprText );

            clang::QualType l_t = l_callExprPtr->getType();
            if ( !l_t.isNull() ) {
                l_recordQt = l_t->getPointeeType().getUnqualifiedType();
            }
            logVariable( l_recordQt );

        } else if ( l_castToPtr ) {
            // explicit cast to struct* (we matched
            // hasDestinationType(pointer-to-record))
            l_pointerPassed = true;
            logVariable( l_pointerPassed );

            // destination type's pointee is the record type
            clang::QualType l_castDest = l_castToPtr->getType();

            if ( !l_castDest.isNull() ) {
                l_recordQt = l_castDest->getPointeeType().getUnqualifiedType();
            }

            logVariable( l_recordQt );

            // Prefer a bound inner decl (if matcher gave us castDeclRef)
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

            // Else if casted-from call expression
            if ( l_baseExprText.empty() && l_castCallExpr ) {
                l_baseExprText =
                    l_getSourceTextOrFallback( l_castCallExpr, "" );

                logVariable( l_baseExprText );
            }

            // Else if casted-from member expression
            if ( l_baseExprText.empty() && l_castMemberExpr ) {
                l_baseExprText =
                    l_getSourceTextOrFallback( l_castMemberExpr, "" );

                logVariable( l_baseExprText );
            }

            // As a final fallback, use the whole cast expression text
            if ( l_baseExprText.empty() ) {
                l_baseExprText = l_getSourceTextOrFallback( l_castToPtr, "" );

                logVariable( l_baseExprText );
            }
        } else {
            // nothing matched (shouldn't happen if matcher and bindings are
            // correct)
            logError( "No matching first-argument pattern found." );

            goto EXIT;
        }

        if ( l_recordQt.isNull() ) {
            logError( "Record type is null." );

            goto EXIT;
        }

        if ( !l_recordQt->isStructureType() && !l_recordQt->isUnionType() ) {
            logError( "First arg is not a struct/union type." );

            goto EXIT;
        }

        const clang::RecordType* l_rt =
            l_recordQt->getAs< clang::RecordType >();

        if ( !l_rt ) {
            logError( "RecordType cast failed." );

            goto EXIT;
        }

        clang::RecordDecl* l_rd = l_rt->getOriginalDecl();

        if ( !l_rd ) {
            logError( "Original RecordDecl missing." );

            goto EXIT;
        }

        l_rd = l_rd->getDefinition();

        if ( !l_rd ) {
            logError(
                std::string( "Record has no definition (forward-decl): " ) +
                l_varDecl->getNameAsString() );

            goto EXIT;
        }

        logVariable( l_rd );

        // Determine indentation from call location (use spelling loc for
        // column)
        clang::SourceLocation l_startLoc =
            l_sm.getSpellingLoc( l_call->getBeginLoc() );
        unsigned l_col = l_sm.getSpellingColumnNumber( l_startLoc );
        std::string l_indent( ( l_col > 0 ) ? ( l_col - 1 ) : 0, ' ' );

        // Prepare replacement
        std::string l_replacementText;

        // offsetof/ sizeof
        std::string l_recordTypeStr;

        if ( const auto* l_tdt = l_recordQt->getAs< clang::TypedefType >() ) {
            l_recordTypeStr = l_tdt->getDecl()->getNameAsString();

        } else {
            l_recordTypeStr = l_recordQt.getAsString();
        }

        logVariable( l_recordTypeStr );

        llvm::raw_string_ostream l_ss( l_replacementText );

        auto l_appendFieldLine = [ & ]( const clang::FieldDecl* _fd ) {
            if ( ( !_fd ) || ( _fd->isUnnamedBitField() ) ||
                 ( !_fd->getIdentifier() ) ) {
                return;
            }

            std::string l_fname = _fd->getNameAsString();

            if ( l_fname.empty() ) {
                return;
            }

            std::string l_ftype = _fd->getType().getAsString();
            std::string l_memberAccess =
                ( ( l_pointerPassed )
                      ? ( "(" + l_baseExprText + ")->" + l_fname )
                      : ( l_baseExprText + "." + l_fname ) );
            std::string l_fieldRef = ( "&(" + l_memberAccess + ")" );

            // callbackName( "fieldName", "fieldType", &(variable->field),
            // __builtin_offsetof( structType, field ), sizeof( ( (structType*)
            // 0)->field ) );
            l_ss << l_indent << l_callbackName << "(\"" << l_fname << "\", "
                 << "\"" << l_ftype << "\", " << l_fieldRef << ", "
                 << "__builtin_offsetof(" << l_recordTypeStr << ", " << l_fname
                 << "), "
                 << "sizeof(((" << l_recordTypeStr << "*)0)->" << l_fname
                 << "));\n";
        };

        for ( const clang::FieldDecl* l_fd : l_rd->fields() ) {
            l_appendFieldLine( l_fd );
        }

        l_ss.flush();

        l_replacementText.erase( 0, l_indent.length() );

        if ( !l_replacementText.empty() && l_replacementText.back() == '\n' ) {
            l_replacementText.pop_back();
        }

        logVariable( l_replacementText );

        // Replace the entire call (including semicolon) with replacementText
        clang::SourceLocation l_endLoc = l_call->getEndLoc();
        clang::SourceLocation l_afterCall =
            clang::Lexer::getLocForEndOfToken( l_endLoc, 0, l_sm, l_lo );

        bool l_includeSemi = false;

        if ( l_afterCall.isValid() ) {
            clang::SourceLocation l_afterSpelling =
                l_sm.getSpellingLoc( l_afterCall );

            if ( l_afterSpelling.isValid() ) {
                bool l_invalid = false;

                const char* l_c =
                    l_sm.getCharacterData( l_afterSpelling, &l_invalid );

                if ( ( !l_invalid ) && ( l_c ) && ( *l_c == ';' ) ) {
                    l_includeSemi = true;
                }
            }
        }

        clang::SourceLocation l_replaceEnd =
            ( ( l_includeSemi ) ? ( l_afterCall.getLocWithOffset( 1 ) )
                                : ( l_endLoc ) );
        clang::CharSourceRange l_toReplace =
            clang::CharSourceRange::getCharRange( l_call->getBeginLoc(),
                                                  l_replaceEnd );

        // Only attempt to query rewritten text / replace if the range is valid
        // and in main file
        if ( ( !l_toReplace.isValid() ) ||
             ( !_rewriter.getSourceMgr().isWrittenInMainFile(
                 l_toReplace.getBegin() ) ) ) {
            logError( "Invalid or non-main file range for rewrite." );

            goto EXIT;
        }

        // optional: debug existing rewritten text safely
        std::string l_existing = _rewriter.getRewrittenText( l_toReplace );

        if ( l_existing.empty() ) {
            logError(
                "Existing rewritten text is empty or not yet rewritten." );

        } else {
            logError( "Existing rewritten text: " + l_existing );
        }

        _rewriter.ReplaceText( l_toReplace, l_replacementText );

        log( "performed_replace" );
    }

EXIT:
    traceExit();
}

void IterateStructUnionHandler::addMatcher( MatchFinder& _matcher,
                                            clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateStructUnionHandler >( _rewriter );

    // Canonical record type matcher (handles typedefs/quals)
    auto l_isRecordType = qualType( hasCanonicalType( recordType() ) );

    // Helper matchers
    auto l_ptrToRecord = pointsTo( l_isRecordType );
    auto l_arrayOfRecord = arrayType( hasElementType( l_isRecordType ) );

    // First-argument possibilities:
    //  - &struct                 -> bind "addrDeclRef"
    //  - struct*                 -> bind "ptrDeclRef"
    //  - Array of struct         -> bind "arrayDeclRef" (decays to pointer)
    //  - getStructPointer()      -> bind "callExprPtr" (call expression that
    //  yields struct*)
    //  - (struct S*)expression   -> bind "castToPtr" and inner nodes if
    //  available
    auto l_firstArgument = anyOf(
        // &struct
        unaryOperator(
            hasOperatorName( "&" ),
            hasUnaryOperand( ignoringParenImpCasts(
                declRefExpr( to( varDecl( hasType( l_isRecordType ) ) ) )
                    .bind( "addrDeclRef" ) ) ) ),

        // struct*
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_ptrToRecord ) ) ) )
                .bind( "ptrDeclRef" ) ),

        // Array of struct
        ignoringParenImpCasts(
            declRefExpr( to( varDecl( hasType( l_arrayOfRecord ) ) ) )
                .bind( "arrayDeclRef" ) ),

        // getStructPointer()
        ignoringParenImpCasts(
            callExpr( hasType( l_ptrToRecord ) ).bind( "callExprPtr" ) ),

        // (struct S*)expression
        ignoringParenImpCasts(
            cStyleCastExpr(
                hasDestinationType( l_ptrToRecord ),
                hasSourceExpression( ignoringParenImpCasts( anyOf(
                    declRefExpr( to( varDecl( hasType( l_isRecordType ) ) ) )
                        .bind( "castDeclRef" ),
                    callExpr().bind( "castCallExpr" ),
                    memberExpr().bind( "castMemberExpr" ) ) ) ) )
                .bind( "castToPtr" ) ) );

    // Match calls to:
    // iterate_struct(&struct, "callback")
    // iterate_union(&struct, "callback")
    // iterate_struct_union(&struct, "callback")
    _matcher.addMatcher(
        callExpr(
            callee( functionDecl( hasAnyName( "iterate_struct", "iterate_union",
                                              "iterate_struct_union" ) ) ),
            hasArgument( 0, l_firstArgument ),
            hasArgument( 1, stringLiteral().bind( "macroStr" ) ) )
            .bind( "iterateStructUnionCall" ),
        l_handler.release() );

    traceExit();
}
