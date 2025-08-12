#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

class RewritionFrontendAction : public clang::ASTFrontendAction {
public:
    auto CreateASTConsumer( clang::CompilerInstance& _ci,
                            clang::StringRef _file )
        -> std::unique_ptr< clang::ASTConsumer > override;

#if 0
    void ExecuteAction() override;
#endif

    // TODO: Improve
    // TODO: Write to input file in-place  or .fileName
    void EndSourceFileAction() override;

private:
    clang::Rewriter _theRewriter;
};
