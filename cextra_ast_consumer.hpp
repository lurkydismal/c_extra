#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include "iterate_struct.hpp"

class CExtraASTConsumer : public clang::ASTConsumer {
public:
    CExtraASTConsumer( clang::Rewriter& _r );

    void HandleTranslationUnit( clang::ASTContext& _context ) override;

private:
    SFuncHandler _handlerForSFunc;
    clang::ast_matchers::MatchFinder _matcher;
};
