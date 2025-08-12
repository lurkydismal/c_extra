#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Rewrite/Core/Rewriter.h>

class SFuncHandler : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    SFuncHandler( clang::Rewriter& _r );

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& _result ) override;

private:
    clang::Rewriter& _theRewriter;
};

class SFuncASTConsumer : public clang::ASTConsumer {
public:
    SFuncASTConsumer( clang::Rewriter& _r );

    void HandleTranslationUnit( clang::ASTContext& _context ) override;

private:
    SFuncHandler _handlerForSFunc;
    clang::ast_matchers::MatchFinder _matcher;
};
