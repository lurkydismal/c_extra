#include "rewrition_frontend.hpp"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include "iterate_struct.hpp"

auto RewritionFrontendAction::CreateASTConsumer( clang::CompilerInstance& _ci,
                                                 clang::StringRef _file )
    -> std::unique_ptr< clang::ASTConsumer > {
    _theRewriter.setSourceMgr( _ci.getSourceManager(), _ci.getLangOpts() );

    return std::make_unique< SFuncASTConsumer >( _theRewriter );
}

#if 0
void RewritionFrontendAction::ExecuteAction() override {
    clang::CompilerInstance& l_ci = getCompilerInstance();
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
#endif

void RewritionFrontendAction::EndSourceFileAction() {
    // Write the rewritten buffer to output file
    clang::SourceManager& l_sm = _theRewriter.getSourceMgr();

    clang::FileID l_mainFileId = l_sm.getMainFileID();
    clang::StringRef l_inputFile =
        l_sm.getFileEntryForID( l_mainFileId )->tryGetRealPathName();

    clang::SmallString< FILENAME_MAX > l_filePath = l_inputFile;

    // Do not edit in-place
    // Write to fileName -> .fileName
    if ( true ) {
        clang::StringRef l_baseName = llvm::sys::path::filename( l_filePath );
        std::string l_newName = std::string( "." ) + l_baseName.str();

        llvm::sys::path::remove_filename( l_filePath );
        llvm::sys::path::append( l_filePath, l_newName );
    }

    std::error_code l_ec;

    llvm::raw_fd_ostream l_outFile( l_filePath, l_ec, llvm::sys::fs::OF_None );

    if ( l_ec ) {
        // Function file:line | message
        llvm::errs() << __FUNCTION__ << " " << __FILE_NAME__ << ":" << __LINE__
                     << " | " << l_ec.message() << "\n";

        return;
    }

    _theRewriter.getEditBuffer( l_mainFileId ).write( l_outFile );
}
