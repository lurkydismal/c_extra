#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Rewrite/Core/Rewriter.h>

using namespace clang::ast_matchers;

class IterateStructHandler : public MatchFinder::MatchCallback {
public:
    IterateStructHandler( clang::Rewriter& _rewriter );

    void run( const MatchFinder::MatchResult& _result ) override;

    static void addMatcher( MatchFinder& _matcher, clang::Rewriter& _rewriter );

private:
    clang::Rewriter& _rewriter;
};
