#include "iterate_struct.hpp"

#include <clang/Lex/Lexer.h>

#include <memory>

#include "log.hpp"
#include "trace.hpp"

using namespace clang::ast_matchers;

IterateStructHandler::IterateStructHandler( clang::Rewriter& _rewriter )
    : _rewriter( _rewriter ) {
    traceEnter();

    traceExit();
}

void IterateStructHandler::run( const MatchFinder::MatchResult& _result ) {
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

        clang::StringRef l_txt = clang::Lexer::getSourceText( l_r, l_sm, l_lo );

        if ( !l_txt.empty() ) {
            // TODO: Test
            traceExit();
            return ( l_txt.str() );
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
        _result.Nodes.getNodeAs< clang::CallExpr >( "sFuncCall" );

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

        if ( !l_addrDeclRef && !l_ptrDeclRef ) {
            logError( "No addrDeclRef or ptrDeclRef bound." );

            goto EXIT;
        }

        const bool l_pointerPassed = ( l_ptrDeclRef != nullptr );

        logVariable( l_pointerPassed );

        const clang::DeclRefExpr* l_baseDeclRef =
            ( ( l_ptrDeclRef ) ? ( l_ptrDeclRef ) : ( l_addrDeclRef ) );

        logVariable( l_baseDeclRef );

        if ( !l_baseDeclRef ) {
            goto EXIT;
        }

        const auto* l_varDecl =
            llvm::dyn_cast< clang::VarDecl >( l_baseDeclRef->getDecl() );

        if ( !l_varDecl ) {
            logError( "Base decl is not a VarDecl." );

            goto EXIT;
        }

        logVariable( l_varDecl );

        clang::QualType l_recordQt =
            l_getRecordTypeFromVar( l_varDecl, l_pointerPassed );

        logVariable( l_recordQt );

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

        std::string l_baseExprText = l_getSourceTextOrFallback(
            l_baseDeclRef, l_varDecl->getNameAsString() );

        logVariable( l_baseExprText );

        //  Determine indentation from call location (use spelling loc for
        //  column)
        clang::SourceLocation l_startLoc =
            l_sm.getSpellingLoc( l_call->getBeginLoc() );
        unsigned l_col = l_sm.getSpellingColumnNumber( l_startLoc );
        std::string l_indent( ( l_col > 0 ) ? ( l_col - 1 ) : 0, ' ' );

        // Prepare replacement
        std::string l_replacementText = "\n";

        // Get a reasonable textual representation of the record type for
        // offsetof/sizeof. Prefer the printed QualType (may include "struct
        // ..." or typedef name).
        std::string l_recordTypeStr;

        // llvm::dyn_cast< clang::TypedefType >( l_recordQt ) ) {

        if ( const auto* l_tdt = l_recordQt->getAs< clang::TypedefType >() ) {
            l_recordTypeStr = l_tdt->getDecl()->getNameAsString();

#if 0
        } else if ( const auto* TAT = llvm::dyn_cast< clang::TypeAliasType >(
                        l_recordQt.getTypePtrOrNull() ) ) {
            // using alias
            l_recordTypeStr = TAT->getDecl()->getNameAsString();
#endif

        } else {
            l_recordTypeStr = l_recordQt.getAsString();
        }

        logVariable( l_recordTypeStr );

        llvm::raw_string_ostream l_ss( l_replacementText );

        auto l_appendFieldLine = [ & ]( const clang::FieldDecl* _fd ) {
            if ( !_fd ) {
                return;
            }

            if ( _fd->isUnnamedBitField() ) {
                return;
            }

            if ( !_fd->getIdentifier() ) {
                return;
            }

            std::string l_fname = _fd->getNameAsString();
            if ( l_fname.empty() ) {
                return;
            }

            std::string l_ftype = _fd->getType().getAsString();
            std::string l_memberAccess =
                l_pointerPassed ? ( "(" + l_baseExprText + ")->" + l_fname )
                                : ( l_baseExprText + "." + l_fname );
            std::string l_fieldRef = "&(" + l_memberAccess + ")";

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

                if ( !l_invalid && l_c && *l_c == ';' )
                    l_includeSemi = true;
            }
        }

        clang::SourceLocation l_replaceEnd =
            ( l_includeSemi ? l_afterCall.getLocWithOffset( 1 ) : l_endLoc );
        clang::CharSourceRange l_toReplace =
            clang::CharSourceRange::getCharRange( l_call->getBeginLoc(),
                                                  l_replaceEnd );

        // Only attempt to query rewritten text / replace if the range is valid
        // and in main file
        if ( !l_toReplace.isValid() ||
             !_rewriter.getSourceMgr().isWrittenInMainFile(
                 l_toReplace.getBegin() ) ) {
            logError( "Invalid or non-main file range for rewrite." );

            goto EXIT;
        }

        // optional: debug existing rewritten text safely
        std::string l_existing = _rewriter.getRewrittenText( l_toReplace );
        if ( l_existing.empty() ) {
            llvm::errs()
                << "Existing rewritten text is empty or not yet rewritten.\n";

        } else {
            llvm::errs() << "Existing rewritten text: " << l_existing << "\n";
        }

        _rewriter.ReplaceText( l_toReplace, l_replacementText );
        log( "performed_replace" );
    }

EXIT:
    traceExit();
}

void IterateStructHandler::addMatcher( MatchFinder& _matcher,
                                       clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateStructHandler >( _rewriter );

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
        l_handler.release() );

    traceExit();
}
