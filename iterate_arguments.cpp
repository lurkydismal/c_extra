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

    clang::SourceManager& l_sourceManager = *_result.SourceManager;
    clang::LangOptions l_langOptions = _result.Context->getLangOpts();

    const auto* l_callingExpression =
        _result.Nodes.getNodeAs< clang::CallExpr >( "iterateArgumentsCall" );

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
        const auto* l_ancestorFunctionDeclaration =
            _result.Nodes.getNodeAs< clang::FunctionDecl >(
                "ancestorFunctionDeclaration" );

        logVariable( l_ancestorFunctionDeclaration );

        if ( !l_ancestorFunctionDeclaration ) {
            goto EXIT;
        }

        std::string l_replacementText;

        // Build replacement text
        {
            // Determine indentation from call location
            // Use spelling loc for column
            const clang::SourceLocation l_sourceStartLocation =
                l_sourceManager.getSpellingLoc(
                    l_callingExpression->getBeginLoc() );
            unsigned l_spellingColumnNumber =
                l_sourceManager.getSpellingColumnNumber(
                    l_sourceStartLocation );
            l_spellingColumnNumber = ( ( l_spellingColumnNumber > 0 )
                                           ? ( l_spellingColumnNumber - 1 )
                                           : ( 0 ) );
            const std::string l_indent( l_spellingColumnNumber, ' ' );

            llvm::raw_string_ostream l_replacementTextStringStream(
                l_replacementText );

            for ( const clang::ParmVarDecl* l_parameter :
                  l_ancestorFunctionDeclaration->parameters() ) {
                const std::string l_parameterName =
                    l_parameter->getNameAsString();

                logVariable( l_parameterName );

                if ( l_parameterName.empty() ) {
                    logError( "Parameter has no name; skipping" );

                    continue;
                }

                std::string l_parameterTypeString;

                // Build
                {
                    clang::QualType l_parameterType = l_parameter->getType();

                    const auto* l_typedefType =
                        l_parameterType->getAs< clang::TypedefType >();

                    if ( l_typedefType ) {
                        l_parameterTypeString =
                            l_typedefType->getDecl()->getNameAsString();

                    } else {
                        l_parameterTypeString = l_parameterType.getAsString();
                    }
                }

                logVariable( l_parameterName );

                const std::string l_parameterReference =
                    ( "&(" + l_parameterName + ")" );

                // callbackName(
                //   "parameterName",
                //   "parameterType",
                //   sizeof( parameterName );
                l_replacementTextStringStream
                    << l_indent << l_callbackName << "(" << "\""
                    << l_parameterName << "\", "
                    << "\"" << l_parameterTypeString << "\", "
                    << l_parameterReference << ", " << "sizeof("
                    << l_parameterName << ")" << ");\n";
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

void IterateArgumentsHandler::addMatcher( MatchFinder& _matcher,
                                          clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateArgumentsHandler >( _rewriter );

    // Match calls to iterate_arguments(&struct, "callback")
    _matcher.addMatcher(
        callExpr(
            callee( functionDecl( hasName( "iterate_arguments" ) ) ),
            hasAncestor( functionDecl().bind( "ancestorFunctionDeclaration" ) ),
            hasArgument( 0, stringLiteral().bind( "callbackName" ) ) )
            .bind( "iterateArgumentsCall" ),
        l_handler.release() );
    traceExit();
}
