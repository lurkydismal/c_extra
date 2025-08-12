#pragma once

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>

class CExtraFrontendAction : public clang::ASTFrontendAction {
public:
    auto CreateASTConsumer( clang::CompilerInstance& _ci,
                            const clang::StringRef _file )
        -> std::unique_ptr< clang::ASTConsumer > override;

#if 0
    void ExecuteAction() override;
#endif

    void EndSourceFileAction() override;

private:
    clang::Rewriter _theRewriter;
};
