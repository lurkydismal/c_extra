#include "cextra_frontend.hpp"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <cstdio>

#include "arguments_parse.hpp"
#include "cextra_ast_consumer.hpp"
#include "log.hpp"

#if 0
class MacroNameReplacer : public clang::PPCallbacks {
public:
    MacroNameReplacer( clang::Rewriter& _r, clang::SourceManager& _sm )
        : _theRewriter( _r ), _sm( _sm ) {}

    void MacroExpands( const clang::Token& _macroNameTok,
                       const clang::MacroDefinition& _md,
                       clang::SourceRange _range,
                       const clang::MacroArguments* _arguments ) override {
        clang::SourceLocation l_loc = _macroNameTok.getLocation();
        clang::StringRef l_macroName = _macroNameTok.getIdentifierInfo()->getName();

        // Replace macro usage with "MACRO_NAME"
        _theRewriter.ReplaceText( l_loc, l_macroName.size(),
                                  "\"" + l_macroName.str() + "\"" );
    }

private:
    clang::Rewriter& _theRewriter;
    clang::SourceManager& _sm;
};
#endif

auto CExtraFrontendAction::CreateASTConsumer(
    clang::CompilerInstance& _compilerInstance,
    const clang::StringRef _filePath )
    -> std::unique_ptr< clang::ASTConsumer > {
    _theRewriter.setSourceMgr( _compilerInstance.getSourceManager(),
                               _compilerInstance.getLangOpts() );

    return ( std::make_unique< CExtraASTConsumer >( _theRewriter ) );
}

#if 0
void CExtraFrontendAction::ExecuteAction() override {
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

static inline auto writeToFile( const clang::StringRef _filePath,
                                const clang::FileID& _fileId,
                                clang::Rewriter& _theRewriter ) -> bool {
    bool l_returnValue = false;

    std::error_code l_errorCode;

    llvm::raw_fd_ostream l_outputFile( _filePath, l_errorCode,
                                       llvm::sys::fs::OF_None );

    l_returnValue = !( l_errorCode );

    if ( !l_returnValue ) {
        logError( l_errorCode.message() );

        goto EXIT;
    }

    _theRewriter.getEditBuffer( _fileId ).write( l_outputFile );

EXIT:
    return ( l_returnValue );
}

void CExtraFrontendAction::EndSourceFileAction() {
    clang::DiagnosticsEngine& l_diagnosticsEngine =
        getCompilerInstance().getDiagnostics();

    if ( l_diagnosticsEngine.hasErrorOccurred() ) {
        logError( "Processing failed due to errors." );

        return;
    }

    const clang::SourceManager& l_sourceManager = _theRewriter.getSourceMgr();

    const clang::FileID l_fileId = l_sourceManager.getMainFileID();
    const clang::StringRef l_inputFile =
        l_sourceManager.getFileEntryForID( l_fileId )->tryGetRealPathName();

    if ( l_inputFile.empty() ) {
        return;
    }

    clang::SmallString< FILENAME_MAX > l_filePath = l_inputFile;

    // Do not edit in-place and write to fileName -> prefix.fileName.extension
    if ( !g_needEditInPlace ) {
        const clang::StringRef l_fileName =
            llvm::sys::path::filename( l_filePath );
        // With prefix
        std::string l_newFileName = ( g_prefix + l_fileName.str() );

        // Add extension
        {
            clang::StringRef l_extension =
                llvm::sys::path::extension( l_newFileName );

            if ( l_extension.empty() ) {
                logError( "Extension not found in file name." );

                return;
            }

            // Remove extension temporarily
            l_newFileName.resize( l_newFileName.size() - l_extension.size() );

            // Append custom extension + original one
            l_newFileName += g_extension;
            l_newFileName += l_extension;
        }

        llvm::sys::path::remove_filename( l_filePath );
        llvm::sys::path::append( l_filePath, l_newFileName );
    }

    if ( !g_isDryRun ) {
        clang::SmallString< FILENAME_MAX > l_outputPath;

        if ( !g_outputDirectory.empty() ) {
            const clang::StringRef l_fileName =
                llvm::sys::path::filename( l_filePath );

            l_outputPath = ( g_outputDirectory + l_fileName.str() );

        } else {
            l_outputPath = l_filePath;
        }

        writeToFile( l_filePath, l_fileId, _theRewriter );
    }
}
