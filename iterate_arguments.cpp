#include "iterate_arguments.hpp"

#include <memory>

#include "common_ast_handlers.hpp"
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

        const std::string l_replacementText = common::buildReplacementText(
            _rewriter, l_callingExpression,
            l_ancestorFunctionDeclaration->parameters(),
            [ & ]( const clang::ParmVarDecl* _argumentDeclaration,
                   llvm::raw_string_ostream& _replacementTextStringStream,
                   const clang::StringRef _indent ) {
                traceEnter();

                if ( ( !_argumentDeclaration ) ||
                     ( !_argumentDeclaration->getIdentifier() ) ) {
                    goto EXIT;
                }

                {
                    const std::string l_argumentName =
                        _argumentDeclaration->getNameAsString();

                    logVariable( l_argumentName );

                    if ( l_argumentName.empty() ) {
                        logError( "Argument has no name; skipping" );

                        goto EXIT;
                    }

                    std::string l_argumentTypeString;

                    // Build argument type string
                    {
                        clang::QualType l_argumentType =
                            _argumentDeclaration->getType();

                        const auto* l_typedefType =
                            l_argumentType->getAs< clang::TypedefType >();

                        if ( l_typedefType ) {
                            l_argumentTypeString =
                                l_typedefType->getDecl()->getNameAsString();

                        } else {
                            l_argumentTypeString = l_argumentType.getAsString();
                        }
                    }

                    logVariable( l_argumentName );

                    const std::string l_argumentReference =
                        ( "&(" + l_argumentName + ")" );

                    // callbackName(
                    //   "argumentName",
                    //   "argumentType",
                    //   sizeof( argumentName );
                    _replacementTextStringStream
                        << _indent << l_callbackName << "("
                        << "\"" << l_argumentName << "\", "
                        << "\"" << l_argumentTypeString << "\", "
                        << l_argumentReference << ", " << "sizeof("
                        << l_argumentName << ")" << ");\n";
                }

            EXIT:
                traceExit();
            } );

        logVariable( l_replacementText );

        common::replaceText( _rewriter, l_callingExpression,
                             l_replacementText );
    }

EXIT:
    traceExit();
}

void IterateArgumentsHandler::addMatcher( MatchFinder& _matcher,
                                          clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateArgumentsHandler >( _rewriter );

    // Match calls to iterate_arguments("callback")
    _matcher.addMatcher(
        callExpr(
            callee( functionDecl( hasName( "iterate_arguments" ) ) ),
            hasAncestor( functionDecl().bind( "ancestorFunctionDeclaration" ) ),
            hasArgument( 0, stringLiteral().bind( "callbackName" ) ) )
            .bind( "iterateArgumentsCall" ),
        l_handler.release() );
    traceExit();
}
