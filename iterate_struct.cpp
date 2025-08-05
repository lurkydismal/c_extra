#include <argp.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

constexpr const char* g_applicationIdentifier = "gamuingu";
constexpr const char* g_applicationVersion = "0.1";
constexpr const char* g_applicationDescription = "brrr gamuingu";
constexpr const char* g_applicationContactAddress = "<lurkydismal@duck.com>";

const char* argp_program_version;
const char* argp_program_bug_address;

static std::vector< std::string > g_compileArgs;
static std::vector< std::string > g_sources;

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
            // std::string typeName = field->getType().getAsString();
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

            // callbkName( "fieldName", /* fieldType,*/ &(variable->field),
            // offsetof( structType, field ), sizeof( ( (structType*) 0)->field
            // ) );
            l_replacementText.append( l_indent )
                .append( l_callbackName )
                .append( "(" )
                .append( "\"" )
                .append( l_fieldName )
                .append( "\", " ) // "fieldName",
                //.append(type_name).append(", ")              // fieldType
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
            l_includeSemi ? l_afterCall.getLocWithOffset( 1 ) : l_endLoc;
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

class SFuncFrontendAction : public ASTFrontendAction {
public:
    auto CreateASTConsumer( CompilerInstance& _ci, StringRef _file )
        -> std::unique_ptr< ASTConsumer > override {
        _theRewriter.setSourceMgr( _ci.getSourceManager(), _ci.getLangOpts() );
        return std::make_unique< SFuncASTConsumer >( _theRewriter );
    }
    void EndSourceFileAction() override {
        // Write the rewritten buffer to output file
        SourceManager& l_sm = _theRewriter.getSourceMgr();
        std::error_code l_ec;
        // TODO: Write to input file in-place  or .fileName
        llvm::raw_fd_ostream l_outFile( "t/2.c", l_ec, llvm::sys::fs::OF_None );
        _theRewriter.getEditBuffer( l_sm.getMainFileID() ).write( l_outFile );
    }

private:
    Rewriter _theRewriter;
};

static auto parserForOption( int _key, char* _value, struct argp_state* _state )
    -> error_t {
    error_t l_returnValue = 0;

    switch ( _key ) {
        case ARGP_KEY_END: {
            break;
        }

        default: {
            l_returnValue = ARGP_ERR_UNKNOWN;
        }
    }

    return ( l_returnValue );
}

// Setup recources to load
// Set compiler flags
static auto parseArguments( int _argumentCount, char** _argumentVector )
    -> bool {
    bool l_returnValue = false;

    if ( !_argumentVector ) {
        llvm::errs() << "Invalid argument\n";

        goto EXIT;
    }

    {
        static const std::string l_programVersion =
            ( std::string( g_applicationIdentifier ) + " " +
              g_applicationVersion );
        argp_program_version = l_programVersion.c_str();
        argp_program_bug_address = g_applicationContactAddress;

        {
            static const std::string l_description =
                ( std::string( g_applicationIdentifier ) + " - " +
                  g_applicationDescription );

            const std::vector< struct argp_option > l_options = {
                // { "verbose", 'v', 0, 0, "Produce verbose output", 0 },
                // { "quiet", 'q', 0, 0, "Do not produce any output", 0 },
                // { "print", 'p', 0, 0, "Print available configuration", 0 },
                // { "save", 's', 0, 0, "Save without running", 0 },
                { { nullptr, 0, nullptr, 0, nullptr, 0 } } };

            // [NAME] - optional
            // NAME - required
            // NAME... - at least one and more
            const char* l_arguments = "FILE...";

            struct argp l_argumentParser = {
                l_options.data(), parserForOption,
                l_arguments,      l_description.c_str(),
                nullptr,          nullptr,
                nullptr };

            l_returnValue = argp_parse( &l_argumentParser, _argumentCount,
                                        _argumentVector, 0, nullptr, nullptr );
        }

        if ( !l_returnValue ) {
            goto EXIT;
        }

        l_returnValue = true;
    }

EXIT:
    return ( l_returnValue );
}

auto main( int _argumentCount, char** _argumentVector ) -> int {
    int l_returnValue = EXIT_FAILURE;

    {
        if ( parseArguments( _argumentCount, _argumentVector ) ) {
            l_returnValue = -1;

            llvm::errs() << "Parsing arguments\n";

            goto EXIT;
        }

        // TODO: Imrpove
        g_compileArgs.emplace_back( "-std=c11" );
        g_sources.emplace_back( "t/t.c" );

        FixedCompilationDatabase l_compilationDatabase( ".", g_compileArgs );
        ClangTool l_tool( l_compilationDatabase, g_sources );

        auto l_factory = newFrontendActionFactory< SFuncFrontendAction >();

        l_returnValue = l_tool.run( l_factory.get() );
    }

EXIT:
    if ( l_returnValue != EXIT_SUCCESS ) {
        llvm::errs() << g_applicationIdentifier << " returned " << l_returnValue
                     << "\n";
    }

    return ( l_returnValue );
}
