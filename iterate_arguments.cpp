#include "iterate_arguments.hpp"

#include <clang/Lex/Lexer.h>

#include <memory>

#include "log.hpp"
#include "trace.hpp"

using namespace clang::ast_matchers;

IterateArgumentsHandler::IterateArgumentsHandler( clang::Rewriter& _rewriter )
    : _rewriter( _rewriter ) {
    traceEnter();

    traceExit();
}

void IterateArgumentsHandler::run( const MatchFinder::MatchResult& _result ) {
    traceEnter();

    clang::SourceManager& l_sm = *_result.SourceManager;
    clang::LangOptions l_lo = _result.Context->getLangOpts();

    const auto* l_call =
        _result.Nodes.getNodeAs< clang::CallExpr >( "argCall" );
    if ( !l_call )
        goto EXIT;

    {
        const auto* l_macroStr =
            _result.Nodes.getNodeAs< clang::StringLiteral >( "macroStr" );
        if ( !l_macroStr )
            goto EXIT;
        std::string l_callbackName = l_macroStr->getString().str();
        logVariable( l_callbackName );
        log( "Found callback name: " + l_callbackName );

        const auto* l_funcDecl =
            _result.Nodes.getNodeAs< clang::FunctionDecl >( "funcDecl" );
        if ( !l_funcDecl )
            goto EXIT;
        logVariable( l_funcDecl );

        // Determine indent
        clang::SourceLocation l_startLoc =
            l_sm.getSpellingLoc( l_call->getBeginLoc() );
        unsigned l_col = l_sm.getSpellingColumnNumber( l_startLoc );
        std::string l_indent( ( l_col > 0 ) ? ( l_col - 1 ) : 0, ' ' );

        // Build replacement text: one callback per parameter
        std::string l_replacementText;
        llvm::raw_string_ostream l_ss( l_replacementText );
        for ( const clang::ParmVarDecl* l_param : l_funcDecl->parameters() ) {
            std::string l_name = l_param->getNameAsString();
            if ( l_name.empty() ) {
                logError( "Parameter has no name; skipping" );
                continue;
            }
            clang::QualType l_pt = l_param->getType();
            std::string l_typeStr;
            if ( auto* l_tdt = l_pt->getAs< clang::TypedefType >() ) {
                l_typeStr = l_tdt->getDecl()->getNameAsString();
            } else {
                l_typeStr = l_pt.getAsString();
            }
            l_ss << l_indent << l_callbackName << "(\"" << l_name << "\", "
                 << "\"" << l_typeStr << "\", "
                 << "&(" << l_name << "));\n";
        }
        l_ss.flush();
        // Remove indent on first line and trailing newline
        l_replacementText.erase( 0, l_indent.length() );
        if ( !l_replacementText.empty() && l_replacementText.back() == '\n' )
            l_replacementText.pop_back();
        logVariable( l_replacementText );

        // Replace the call including semicolon if present (same logic as above)
        clang::SourceLocation l_endLoc = l_call->getEndLoc();
        clang::SourceLocation l_afterCall =
            clang::Lexer::getLocForEndOfToken( l_endLoc, 0, l_sm, l_lo );
        bool l_includeSemi = false;
        if ( l_afterCall.isValid() ) {
            clang::SourceLocation l_afterSpelling =
                l_sm.getSpellingLoc( l_afterCall );
            bool l_invalid = false;
            const char* l_c =
                l_sm.getCharacterData( l_afterSpelling, &l_invalid );
            if ( !l_invalid && l_c && *l_c == ';' ) {
                l_includeSemi = true;
            }
        }
        clang::SourceLocation l_replaceEnd =
            ( l_includeSemi ? l_afterCall.getLocWithOffset( 1 ) : l_endLoc );
        clang::CharSourceRange l_toReplace =
            clang::CharSourceRange::getCharRange( l_call->getBeginLoc(),
                                                  l_replaceEnd );
        if ( !l_toReplace.isValid() ||
             !_rewriter.getSourceMgr().isWrittenInMainFile(
                 l_toReplace.getBegin() ) ) {
            logError( "Invalid range for rewrite" );
            goto EXIT;
        }
        std::string l_existing = _rewriter.getRewrittenText( l_toReplace );
        if ( !l_existing.empty() ) {
            logError( "Existing rewritten text: " + l_existing );
        }
        _rewriter.ReplaceText( l_toReplace, l_replacementText );
        log( "performed_replace" );
    }

EXIT:
    traceExit();
}

void IterateArgumentsHandler::addMatcher( MatchFinder& _matcher,
                                          clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateArgumentsHandler >( _rewriter );

    // Match calls to iterate_arguments(&struct, "callback")
    _matcher.addMatcher(
        callExpr( callee( functionDecl( hasName( "iterate_arguments" ) ) ),
                  hasAncestor( functionDecl().bind( "funcDecl" ) ),
                  hasArgument( 0, stringLiteral().bind( "macroStr" ) ) )
            .bind( "argCall" ),
        l_handler.release() );
    traceExit();
}
