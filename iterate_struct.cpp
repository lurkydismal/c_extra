#include "iterate_struct.hpp"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Lex/Lexer.h>

#include "clang/AST/Expr.h"

using namespace clang::ast_matchers;

SFuncHandler::SFuncHandler( clang::Rewriter& _r ) : _theRewriter( _r ) {}

void SFuncHandler::run( const MatchFinder::MatchResult& _result ) {
    const auto* l_call =
        _result.Nodes.getNodeAs< clang::CallExpr >( "sFuncCall" );

    if ( !l_call )
        return;

    const auto* l_macroStr =
        _result.Nodes.getNodeAs< clang::StringLiteral >( "macroStr" );

    if ( !l_macroStr ) {
        return;
    }

    std::string l_macroName = l_macroStr->getString().str();
    llvm::outs() << "Found macro name: " << l_macroName << "\n";

    clang::SourceManager& l_sm = *_result.SourceManager;
    clang::LangOptions l_lo = _result.Context->getLangOpts();

    // Get the callback macro name (2nd argument) as text
    const clang::Expr* l_arg1 = l_call->getArg( 1 )->IgnoreParenImpCasts();
    clang::CharSourceRange l_cbRange =
        clang::CharSourceRange::getTokenRange( l_arg1->getSourceRange() );
#if 0
    std::string l_callbackName =
        clang::Lexer::getSourceText( l_cbRange, l_sm, l_lo ).str();
#endif
    std::string& l_callbackName = l_macroName;

    // Process the first argument: expect pointer to struct
    const clang::Expr* l_arg0 = l_call->getArg( 0 )->IgnoreParenImpCasts();
    l_arg0 = l_arg0->IgnoreParenImpCasts();

    std::string l_baseExprText;
    bool l_pointerPassed = true;
    clang::QualType l_recType;

    // If arg0 is an address-of operator, handle specially
    if ( const auto* l_uop = dyn_cast< clang::UnaryOperator >( l_arg0 ) ) {
        if ( l_uop->getOpcode() == clang::UO_AddrOf ) {
            l_pointerPassed = false;

            const clang::Expr* l_sub =
                l_uop->getSubExpr()->IgnoreParenImpCasts();

            // Get the struct type from the sub-expression
            l_recType = l_sub->getType();

            // Text of the base variable (e.g. "var")
            clang::CharSourceRange l_baseRange =
                clang::CharSourceRange::getTokenRange(
                    l_sub->getSourceRange() );

            l_baseExprText =
                clang::Lexer::getSourceText( l_baseRange, l_sm, l_lo ).str();
        }
    }

    if ( l_pointerPassed ) {
        // arg0 is already a pointer expression; get pointee struct type
        clang::QualType l_ptrType = l_arg0->getType();

        if ( l_ptrType.getTypePtrOrNull() )
            l_recType = l_ptrType->getPointeeType();

        clang::CharSourceRange l_baseRange =
            clang::CharSourceRange::getTokenRange( l_arg0->getSourceRange() );

        l_baseExprText =
            clang::Lexer::getSourceText( l_baseRange, l_sm, l_lo ).str();
    }

    if ( !l_recType->isStructureType() )
        return;

    const clang::RecordType* l_rt = l_recType->getAs< clang::RecordType >();

    if ( !l_rt )
        return;

    clang::RecordDecl* l_rd = l_rt->getDecl();

    // if (!RD->hasDefinition()) RD = RD->getDefinition();

    l_rd = l_rd->getDefinition();

    if ( !l_rd )
        return;

    // Determine indentation from call location
    clang::SourceLocation l_startLoc = l_call->getBeginLoc();
    unsigned l_col = l_sm.getSpellingColumnNumber( l_startLoc );
    std::string l_indent( l_col - 1, ' ' );

    // Build replacement text: one cb(...) call per field
    std::string l_replacementText = "\n";

    for ( const clang::FieldDecl* l_field : l_rd->fields() ) {
        if ( l_field->isUnnamedBitField() )
            continue;

        std::string l_fieldName = l_field->getNameAsString();

        if ( l_fieldName.empty() )
            continue;

        std::string l_typeName = l_field->getType().getAsString();
        std::string l_fieldRef;

        l_fieldRef.append( "&(" )
            .append( ( l_pointerPassed ) ? ( "(" ) : ( "" ) )
            .append( l_baseExprText )
            .append( ( l_pointerPassed ) ? ( ")->" ) : ( "." ) )
            .append( l_fieldName )
            .append( ")" );

        // callbackName( "fieldName", "fieldType", &(variable->field),
        // __builtin_offsetof( structType, field ), sizeof( ( (structType*)
        // 0)->field ) );
        l_replacementText.append( l_indent )
            .append( l_callbackName )
            .append( "(" )
            .append( "\"" )
            .append( l_fieldName )
            .append( "\", " ) // "fieldName",
            .append( "\"" )
            .append( l_typeName )
            .append( "\"" )
            .append( ", " ) // "fieldType",
            .append( l_fieldRef )
            .append( ", " ) // &(variable->field),
            .append( "__builtin_offsetof(" )
            .append( l_recType.getAsString() )
            .append( ", " )
            .append( l_fieldName )
            .append( "), " ) // __builtin_offsetof(structType, field),
            .append( "sizeof(((" )
            .append( l_recType.getAsString() )
            .append( "*)0)->" )
            .append( l_fieldName )
            .append( ")" ) // sizeof(((structType*)0)->field)
            .append( ");\n" );
    }

    // Replace the entire call (including semicolon) with replacementText
    clang::SourceLocation l_endLoc = l_call->getEndLoc();
    clang::SourceLocation l_afterCall =
        clang::Lexer::getLocForEndOfToken( l_endLoc, 0, l_sm, l_lo );

    bool l_includeSemi = false;

    if ( l_afterCall.isValid() ) {
        const char* l_c = l_sm.getCharacterData( l_afterCall );

        if ( l_c && *l_c == ';' )
            l_includeSemi = true;
    }
    clang::SourceLocation l_replaceEnd =
        ( ( l_includeSemi ) ? ( l_afterCall.getLocWithOffset( 1 ) )
                            : ( l_endLoc ) );
    clang::CharSourceRange l_toReplace =
        clang::CharSourceRange::getCharRange( l_startLoc, l_replaceEnd );

    _theRewriter.ReplaceText( l_toReplace, l_replacementText );
}

SFuncASTConsumer::SFuncASTConsumer( clang::Rewriter& _r )
    : _handlerForSFunc( _r ) {
    // Match calls to sFunc(...)
    _matcher.addMatcher(
        callExpr(
            callee( functionDecl( hasName( "sFunc" ) ) ),
            hasArgument(
                0, expr( hasType( pointerType( pointee( recordType() ) ) ) )
                       .bind( "structPtrArg" ) ),
            hasArgument( 1, stringLiteral().bind( "macroStr" ) ) )
            .bind( "sFuncCall" ),
        &_handlerForSFunc );
}

void SFuncASTConsumer::HandleTranslationUnit( clang::ASTContext& _context ) {
#if 0
    Step3 step3;
    step3.run( _context, step2.getIntermediate() );
#endif

    _matcher.matchAST( _context );
}
