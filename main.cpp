#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "arguments_parse.hpp"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

class MacroNameReplacer : public PPCallbacks {
public:
    MacroNameReplacer( Rewriter& _r, SourceManager& _sm )
        : _theRewriter( _r ), _sm( _sm ) {}

    void MacroExpands( const Token& _macroNameTok,
                       const MacroDefinition& _md,
                       SourceRange _range,
                       const MacroArgs* _args ) override {
        SourceLocation l_loc = _macroNameTok.getLocation();
        StringRef l_macroName = _macroNameTok.getIdentifierInfo()->getName();

        // Replace macro usage with "MACRO_NAME"
        _theRewriter.ReplaceText( l_loc, l_macroName.size(),
                                  "\"" + l_macroName.str() + "\"" );
    }

private:
    Rewriter& _theRewriter;
    SourceManager& _sm;
};

class SFuncHandler : public MatchFinder::MatchCallback {
public:
    SFuncHandler( Rewriter& _r ) : _theRewriter( _r ) {}

    void run( const MatchFinder::MatchResult& _result ) override {
        const auto* l_call = _result.Nodes.getNodeAs< CallExpr >( "sFuncCall" );

        if ( !l_call )
            return;

        SourceManager& l_sm = *_result.SourceManager;
        LangOptions l_lo = _result.Context->getLangOpts();

        // Get the callback macro name (2nd argument) as text
        const Expr* l_arg1 = l_call->getArg( 1 )->IgnoreParenImpCasts();
        CharSourceRange l_cbRange =
            CharSourceRange::getTokenRange( l_arg1->getSourceRange() );
        std::string l_callbackName =
            Lexer::getSourceText( l_cbRange, l_sm, l_lo ).str();

        // Process the first argument: expect pointer to struct
        const Expr* l_arg0 = l_call->getArg( 0 )->IgnoreParenImpCasts();
        l_arg0 = l_arg0->IgnoreParenImpCasts();

        std::string l_baseExprText;
        bool l_pointerPassed = true;
        QualType l_recType;

        // If arg0 is an address-of operator, handle specially
        if ( const auto* l_uop = dyn_cast< UnaryOperator >( l_arg0 ) ) {
            if ( l_uop->getOpcode() == UO_AddrOf ) {
                l_pointerPassed = false;

                const Expr* l_sub = l_uop->getSubExpr()->IgnoreParenImpCasts();

                // Get the struct type from the sub-expression
                l_recType = l_sub->getType();

                // Text of the base variable (e.g. "var")
                CharSourceRange l_baseRange =
                    CharSourceRange::getTokenRange( l_sub->getSourceRange() );

                l_baseExprText =
                    Lexer::getSourceText( l_baseRange, l_sm, l_lo ).str();
            }
        }

        if ( l_pointerPassed ) {
            // arg0 is already a pointer expression; get pointee struct type
            QualType l_ptrType = l_arg0->getType();

            if ( l_ptrType.getTypePtrOrNull() )
                l_recType = l_ptrType->getPointeeType();

            CharSourceRange l_baseRange =
                CharSourceRange::getTokenRange( l_arg0->getSourceRange() );

            l_baseExprText =
                Lexer::getSourceText( l_baseRange, l_sm, l_lo ).str();
        }

        if ( !l_recType->isStructureType() )
            return;

        const RecordType* l_rt = l_recType->getAs< RecordType >();

        if ( !l_rt )
            return;

        RecordDecl* l_rd = l_rt->getDecl();

        // if (!RD->hasDefinition()) RD = RD->getDefinition();

        l_rd = l_rd->getDefinition();

        if ( !l_rd )
            return;

        // Determine indentation from call location
        SourceLocation l_startLoc = l_call->getBeginLoc();
        unsigned l_col = l_sm.getSpellingColumnNumber( l_startLoc );
        std::string l_indent( l_col - 1, ' ' );

        // Build replacement text: one cb(...) call per field
        std::string l_replacementText = "\n";

        for ( const FieldDecl* l_field : l_rd->fields() ) {
            if ( l_field->isUnnamedBitField() )
                continue;

            std::string l_fieldName = l_field->getNameAsString();

            if ( l_fieldName.empty() )
                continue;

            std::string l_typeName = l_field->getType().getAsString();
            std::string l_fieldRef;

            if ( l_pointerPassed ) {
                l_fieldRef.append( "&((" )
                    .append( l_baseExprText )
                    .append( ")->" )
                    .append( l_fieldName )
                    .append( ")" );

            } else {
                l_fieldRef.append( "&(" )
                    .append( l_baseExprText )
                    .append( "." )
                    .append( l_fieldName )
                    .append( ")" );
            }

            // callbackName( "fieldName", fieldTypeAsString, &(variable->field),
            // offsetof( structType, field ), sizeof( ( (structType*) 0)->field
            // ) );
            l_replacementText.append( l_indent )
                .append( l_callbackName )
                .append( "(" )
                .append( "\"" )
                .append( l_fieldName )
                .append( "\", " ) // "fieldName",
                .append( l_typeName )
                .append( ", " ) // fieldType
                .append( l_fieldRef )
                .append( ", " ) // &(variable->field),
                .append( "offsetof(" )
                .append( l_recType.getAsString() )
                .append( ", " )
                .append( l_fieldName )
                .append( "), " ) // offsetof(structType, field),
                .append( "sizeof(((" )
                .append( l_recType.getAsString() )
                .append( "*)0)->" )
                .append( l_fieldName )
                .append( ")" ) // sizeof(((structType*)0)->field)
                .append( ");\n" );
        }

        // Replace the entire call (including semicolon) with replacementText
        SourceLocation l_endLoc = l_call->getEndLoc();
        SourceLocation l_afterCall =
            Lexer::getLocForEndOfToken( l_endLoc, 0, l_sm, l_lo );

        bool l_includeSemi = false;

        if ( l_afterCall.isValid() ) {
            const char* l_c = l_sm.getCharacterData( l_afterCall );

            if ( l_c && *l_c == ';' )
                l_includeSemi = true;
        }
        SourceLocation l_replaceEnd =
            ( ( l_includeSemi ) ? ( l_afterCall.getLocWithOffset( 1 ) )
                                : ( l_endLoc ) );
        CharSourceRange l_toReplace =
            CharSourceRange::getCharRange( l_startLoc, l_replaceEnd );

        _theRewriter.ReplaceText( l_toReplace, l_replacementText );
    }

private:
    Rewriter& _theRewriter;
};

class SFuncASTConsumer : public ASTConsumer {
public:
    SFuncASTConsumer( Rewriter& _r ) : _handlerForSFunc( _r ) {
        // Match calls to sFunc(...)
        _matcher.addMatcher(
            callExpr( callee( functionDecl( hasName( "sFunc" ) ) ) )
                .bind( "sFuncCall" ),
            &_handlerForSFunc );
    }

    void HandleTranslationUnit( ASTContext& _context ) override {
        _matcher.matchAST( _context );
    }

private:
    SFuncHandler _handlerForSFunc;
    MatchFinder _matcher;
};

class RewritionFrontendAction : public ASTFrontendAction {
public:
    auto CreateASTConsumer( CompilerInstance& _ci, StringRef _file )
        -> std::unique_ptr< ASTConsumer > override {
        _theRewriter.setSourceMgr( _ci.getSourceManager(), _ci.getLangOpts() );

        return std::make_unique< SFuncASTConsumer >( _theRewriter );
    }

    void ExecuteAction() override {
        CompilerInstance& l_ci = getCompilerInstance();
        Preprocessor& l_pp = l_ci.getPreprocessor();
        SourceManager& l_sm = l_ci.getSourceManager();

        _theRewriter.setSourceMgr( l_sm, l_ci.getLangOpts() );
        l_pp.addPPCallbacks(
            std::make_unique< MacroNameReplacer >( _theRewriter, l_sm ) );

        // Run normal compilation to trigger macro expansion
        ASTFrontendAction::ExecuteAction();

        // Output modified code
        _theRewriter.getEditBuffer( l_sm.getMainFileID() )
            .write( llvm::outs() );
    }

    // TODO: Improve
    // TODO: Write to input file in-place  or .fileName
    void EndSourceFileAction() override {
        // Write the rewritten buffer to output file
        SourceManager& l_sm = _theRewriter.getSourceMgr();

        FileID l_mainFileId = l_sm.getMainFileID();
        StringRef l_inputFile =
            l_sm.getFileEntryForID( l_mainFileId )->tryGetRealPathName();

        llvm::SmallString< FILENAME_MAX > l_filePath = l_inputFile;

        // Do not edit in-place
        // Write to fileName -> .fileName
        if ( true ) {
            llvm::StringRef l_baseName =
                llvm::sys::path::filename( l_filePath );
            std::string l_newName = std::string( "." ) + l_baseName.str();

            llvm::sys::path::remove_filename( l_filePath );
            llvm::sys::path::append( l_filePath, l_newName );
        }

        std::error_code l_ec;

        llvm::raw_fd_ostream l_outFile( l_filePath, l_ec,
                                        llvm::sys::fs::OF_None );

        if ( l_ec ) {
            // Function file:line | message
            llvm::errs() << __FUNCTION__ << " " << __FILE_NAME__ << ":"
                         << __LINE__ << " | " << l_ec.message() << "\n";

            return;
        }

        _theRewriter.getEditBuffer( l_mainFileId ).write( l_outFile );
    }

private:
    Rewriter _theRewriter;
};

auto main( int _argumentCount, char* _argumentVector[] ) -> int {
    int l_returnValue = EXIT_FAILURE;

    {
        if ( !parseArguments( _argumentCount, _argumentVector ) ) {
            l_returnValue = EXIT_FAILURE;

            goto EXIT;
        }

        FixedCompilationDatabase l_compilationDatabase(
            g_compilationSourceDirectory, g_compileArgs );
        ClangTool l_tool( l_compilationDatabase, g_sources );

        // TODO: #for, #repeat
#if 0
        auto l_preprocessionFactory = newFrontendActionFactory< EvaluationFrontendAction >();
        l_returnValue = l_tool.run( l_preprocessionFactory.get() );
#endif

        // TODO: iterate_struct, iterate_enum, iterate_union, iterate_arguments,
        // iterate_annotation, iterate_scope
        auto l_rewritionFactory =
            newFrontendActionFactory< RewritionFrontendAction >();
        l_returnValue = l_tool.run( l_rewritionFactory.get() );

        // TODO: constinit, consteval, constexpr
#if 0
        auto l_evaluationFactory = newFrontendActionFactory< EvaluationFrontendAction >();
        l_returnValue = l_tool.run( l_evaluationFactory.get() );
#endif
    }

EXIT:
    return ( l_returnValue );
}
