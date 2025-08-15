#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Rewrite/Core/Rewriter.h>

class CExtraASTConsumer : public clang::ASTConsumer {
public:
    CExtraASTConsumer( clang::Rewriter& _rewriter );

    void HandleTranslationUnit( clang::ASTContext& _context ) override;

private:
    clang::ast_matchers::MatchFinder _matcher;
};
