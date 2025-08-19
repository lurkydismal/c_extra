#include "iterate_struct_union.hpp"

#include <memory>

#include "common_ast_handlers.hpp"
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

    auto [ l_callingExpression, l_recordQualifierType,
           l_recordOriginalDeclaration, l_baseExpressionText, l_pointerPassed,
           l_callbackName ] =
        common::myFunction< common::RecordTag >(
            _result, "iterateStructUnionCall",
            []( const clang::QualType& _qualifierType ) {
                traceEnter();

                bool l_returnValue = false;

                {
                    l_returnValue = ( ( _qualifierType->isStructureType() ) ||
                                      ( _qualifierType->isUnionType() ) );

                    if ( !l_returnValue ) {
                        logError( "First arg is not a struct/ union type." );

                        goto EXIT;
                    }

                    l_returnValue = true;
                }

            EXIT:
                traceExit();

                return ( l_returnValue );
            } );

    // offsetof/ sizeof
    const std::string l_recordTypeString =
        common::buildUnderlyingTypeString( l_recordQualifierType );

    logVariable( l_recordTypeString );

    const std::string l_replacementText = common::buildReplacementText(
        _rewriter, l_callingExpression, l_recordOriginalDeclaration->fields(),
        [ & ]( const clang::FieldDecl* _fieldDeclaration,
               llvm::raw_string_ostream& _replacementTextStringStream,
               const clang::StringRef _indentation ) {
            traceEnter();

            if ( ( !_fieldDeclaration ) ||
                 ( _fieldDeclaration->isUnnamedBitField() ) ||
                 ( !_fieldDeclaration->getIdentifier() ) ) {
                goto EXIT;
            }

            {
                std::string l_fieldName = _fieldDeclaration->getNameAsString();

                logVariable( l_fieldName );

                if ( l_fieldName.empty() ) {
                    logWarning( "Record has no name; skipping" );

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
                              ? ( "(" + l_baseExpressionTextString + ")->" +
                                  l_fieldName )
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
                _replacementTextStringStream
                    << _indentation << l_callbackName << "(" << "\""
                    << l_fieldName << "\", \"" << l_fieldType << "\", "
                    << l_fieldReference << ", "
                    << "__builtin_offsetof(" << l_recordTypeString << ", "
                    << l_fieldName << "), "
                    << "sizeof(((" << l_recordTypeString << "*)0)->"
                    << l_fieldName << ")" << ");\n";
            }

        EXIT:
            traceExit();
        } );

    logVariable( l_replacementText );

    common::replaceText( _rewriter, l_callingExpression, l_replacementText );

EXIT:
    traceExit();
}

void IterateStructUnionHandler::addMatcher( MatchFinder& _matcher,
                                            clang::Rewriter& _rewriter ) {
    traceEnter();

    auto l_handler = std::make_unique< IterateStructUnionHandler >( _rewriter );

    // Match calls to:
    // iterate_struct(&struct, "callback")
    // iterate_union(&struct, "callback")
    // iterate_struct_union(&struct, "callback")
    _matcher.addMatcher(
        callExpr(
            callee( functionDecl( hasAnyName( "iterate_struct", "iterate_union",
                                              "iterate_struct_union" ) ) ),
            hasArgument( 0, common::anyReference( recordType() ) ),
            hasArgument( 1, stringLiteral().bind( "callbackName" ) ) )
            .bind( "iterateStructUnionCall" ),
        l_handler.release() );

    traceExit();
}
