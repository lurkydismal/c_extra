#pragma once

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
