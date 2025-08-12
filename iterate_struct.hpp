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
