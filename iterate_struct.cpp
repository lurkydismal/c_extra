#include "iterate_struct.hpp"

#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Lex/Lexer.h>

#include "log.hpp"
#include "trace.hpp"

SFuncHandler::SFuncHandler( clang::Rewriter& _r ) : _theRewriter( _r ) {
    traceEnter();

    traceExit();
}

void SFuncHandler::run(
    const clang::ast_matchers::MatchFinder::MatchResult& _result ) {
    traceEnter();

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

        {
            std::string l_message = "Found macro name: ";
            l_message.append( l_callbackName );

            log( l_message );
        }

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
        const clang::DeclRefExpr* l_baseDeclRef =
            l_ptrDeclRef ? l_ptrDeclRef : l_addrDeclRef;

        if ( !l_baseDeclRef )
            goto EXIT;

        const auto* l_varDecl =
            llvm::dyn_cast< clang::VarDecl >( l_baseDeclRef->getDecl() );

        if ( !l_varDecl )
            goto EXIT;

        clang::QualType l_varType = l_varDecl->getType();
        clang::QualType l_recordQt;

        if ( l_pointerPassed ) {
            // pointer was passed; try to get pointee
            if ( l_varType->isPointerType() ) {
                l_recordQt = l_varType->getPointeeType();
            } else if ( const auto* l_rt =
                            l_varType->getAs< clang::ReferenceType >() ) {
                // defensive for C++ reference types
                l_recordQt = l_rt->getPointeeType();
            } else {
                // fallback: maybe typedef to pointer or unexpected; use as-is
                l_recordQt = l_varType;
            }
        } else {
            // &var was passed; var should be a record type
            l_recordQt = l_varType;
        }

        // Normalize type: strip qualifiers/typedefs
        l_recordQt = l_recordQt.getUnqualifiedType();

        if ( l_recordQt.isNull() ) {
            goto EXIT;
        }

        if ( !l_recordQt->isStructureType() && !l_recordQt->isUnionType() ) {
            // Not a struct/union; nothing to do
            goto EXIT;
        }

        const clang::RecordType* l_rt =
            l_recordQt->getAs< clang::RecordType >();
        if ( !l_rt )
            goto EXIT;

        clang::RecordDecl* l_rd = l_rt->getOriginalDecl();
        if ( !l_rd )
            goto EXIT;

        // Need the definition to iterate fields
        l_rd = l_rd->getDefinition();
        if ( !l_rd ) {
            std::string l_message = "Record has no definition (forward-decl): ";
            l_message.append( l_varDecl->getNameAsString() );

            logError( l_message );

            goto EXIT;
        }

        clang::SourceManager& l_sm = *( _result.SourceManager );
        clang::LangOptions l_lo = _result.Context->getLangOpts();

        // Get base expression text (use declref token-range so we get exact var
        // text)
        clang::CharSourceRange l_baseRange =
            clang::CharSourceRange::getTokenRange(
                l_baseDeclRef->getSourceRange() );

        std::string l_baseExprText;
        bool l_gotBaseText = false;
        if ( l_baseRange.isValid() ) {
            llvm::Expected< llvm::StringRef > l_maybeText =
                clang::Lexer::getSourceText( l_baseRange, l_sm, l_lo );
            if ( l_maybeText ) {
                l_baseExprText = l_maybeText->str();
                l_gotBaseText = true;
            }
        }
        if ( !l_gotBaseText ) {
            // fallback to var name if source text extraction failed
            l_baseExprText = l_varDecl->getNameAsString();
        }

        // Determine indentation from call location (use spelling loc for
        // column)
        clang::SourceLocation l_startLoc =
            l_sm.getSpellingLoc( l_call->getBeginLoc() );
        unsigned l_col = l_sm.getSpellingColumnNumber( l_startLoc );
        std::string l_indent( ( l_col > 0 ) ? ( l_col - 1 ) : 0, ' ' );

        // Prepare replacement
        std::string l_replacementText = "\n";

        // Get a reasonable textual representation of the record type for
        // offsetof/sizeof. Prefer the printed QualType (may include "struct
        // ..." or typedef name).
        std::string l_recordTypeStr = l_recordQt.getAsString();

        for ( const clang::FieldDecl* l_field : l_rd->fields() ) {
            if ( l_field->isUnnamedBitField() ) {
                continue;
            }

            if ( !l_field->getIdentifier() ) {
                continue;
            }

            std::string l_fieldName = l_field->getNameAsString();

            if ( l_fieldName.empty() ) {
                continue;
            }

            // Build the &(base->field) or &(base.field) expression safely
            std::string l_memberAccess;

            if ( l_pointerPassed ) {
                // wrap base in parentheses to be safe if base is a complex
                // expression
                l_memberAccess = "(";
                l_memberAccess += l_baseExprText;
                l_memberAccess += ")->";
                l_memberAccess += l_fieldName;

            } else {
                l_memberAccess = l_baseExprText;
                l_memberAccess += ".";
                l_memberAccess += l_fieldName;
            }

            std::string l_fieldRef = "&(" + l_memberAccess + ")";

            // Field type string (use printing - may include qualifiers)
            std::string l_fieldTypeStr = l_field->getType().getAsString();

            // callbackName( "fieldName", "fieldType", &(variable->field),
            // __builtin_offsetof( structType, field ), sizeof( ( (structType*)
            // 0)->field ) );
            l_replacementText.append( l_indent )
                .append( l_callbackName )
                .append( "(" )
                .append( "\"" )
                .append( l_fieldName )
                .append( "\"" ) // "fieldName"
                .append( ", " )
                .append( "\"" )
                .append( l_fieldTypeStr )
                .append( "\"" ) // "fieldType"
                .append( ", " )
                .append( l_fieldRef ) // &(variable->field)
                .append( ", " )
                .append( "__builtin_offsetof(" )
                .append( l_recordTypeStr )
                .append( ", " )
                .append( l_fieldName )
                .append( "), " ) // __builtin_offsetof(structType, field)
                .append( "sizeof(((" )
                .append( l_recordTypeStr )
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
            // Check the immediate character after the call end; use spelling
            // location
            const char* l_c = nullptr;

            clang::SourceLocation l_afterSpelling =
                l_sm.getSpellingLoc( l_afterCall );

            if ( l_afterSpelling.isValid() ) {
                l_c = l_sm.getCharacterData( l_afterSpelling, nullptr );
            }

            if ( l_c && *l_c == ';' ) {
                l_includeSemi = true;
            }
        }

        clang::SourceLocation l_replaceEnd =
            ( ( l_includeSemi ) ? ( l_afterCall.getLocWithOffset( 1 ) )
                                : ( l_endLoc ) );

        clang::CharSourceRange l_toReplace =
            clang::CharSourceRange::getCharRange( l_call->getBeginLoc(),
                                                  l_replaceEnd );

        _theRewriter.ReplaceText( l_toReplace, l_replacementText );
    }

EXIT:
    traceExit();
}
