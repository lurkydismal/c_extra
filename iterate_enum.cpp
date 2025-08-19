#include "iterate_enum.hpp"

#include <memory>

#include "common_ast_handlers.hpp"
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

    auto [ l_callingExpression, l_qualifierType, l_originalDeclaration,
           l_baseExpressionText, l_pointerPassed, l_callbackName ] =
        common::myFunction< common::EnumTag >(
            _result, "iterateEnumCall",
            []( const clang::QualType& _qualifierType ) -> bool {
                traceEnter();

                traceExit();

                return ( true );
            } );

    {
        // Underlying integer type of enum
        const clang::QualType l_originalDeclarationUnderlyingQualifierType =
            l_originalDeclaration->getIntegerType();
        const std::string l_enumUnderlyingType =
            common::buildUnderlyingTypeString(
                l_originalDeclarationUnderlyingQualifierType );

        logVariable( l_enumUnderlyingType );

        const std::string l_replacementText = common::buildReplacementText(
            _rewriter, l_callingExpression,
            l_originalDeclaration->enumerators(),
            [ & ](
                const clang::EnumConstantDecl* _enumeratorConstantDeclaration,
                llvm::raw_string_ostream& _replacementTextStringStream,
                const clang::StringRef _indentation ) {
                traceEnter();

                if ( !_enumeratorConstantDeclaration ) {
                    goto EXIT;
                }

                {
                    std::string l_enumeratorConstantName =
                        _enumeratorConstantDeclaration->getNameAsString();

                    logVariable( l_enumeratorConstantName );

                    if ( l_enumeratorConstantName.empty() ) {
                        logWarning(
                            "Enumerator constant has no name; skipping" );

                        goto EXIT;
                    }

                    llvm::APSInt l_enumeratorConstantValue =
                        _enumeratorConstantDeclaration->getInitVal();

                    logVariable( l_enumeratorConstantValue );

                    clang::SmallVector< char >
                        l_enumeratorConstantValueAsString;

                    l_enumeratorConstantValue.toString(
                        l_enumeratorConstantValueAsString );

                    logVariable( l_enumeratorConstantValueAsString );

                    // callbackName(
                    //   "enumeratorConstantName",
                    //   "enumeratorConstantType",
                    //   enumeratorConstantValue,
                    //   sizeof( enumeratorConstantType ) );
                    _replacementTextStringStream
                        << _indentation << l_callbackName << "(" << "\""
                        << l_enumeratorConstantName << "\", "
                        << "\"" << l_enumUnderlyingType << "\", "
                        << "(" << l_enumUnderlyingType << ")"
                        << l_enumeratorConstantValueAsString << ", "
                        << "sizeof(" << l_enumUnderlyingType << ")" << ");\n";
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

void IterateEnumHandler::addMatcher( MatchFinder& _matcher,
                                     clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateEnumHandler >( _rewriter );

    // Match calls to iterate_enum(&enum, "callback")
    _matcher.addMatcher(
        callExpr( callee( functionDecl( hasName( "iterate_enum" ) ) ),
                  hasArgument( 0, common::anyReference( enumType() ) ),
                  hasArgument( 1, stringLiteral().bind( "callbackName" ) ) )
            .bind( "iterateEnumCall" ),
        l_handler.release() );

    traceExit();
}
