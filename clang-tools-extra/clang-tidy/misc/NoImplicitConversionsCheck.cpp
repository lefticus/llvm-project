//===--- NoImplicitConversionsCheck.cpp - clang-tidy ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NoImplicitConversionsCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace misc {

void NoImplicitConversionsCheck::registerMatchers(MatchFinder *Finder) {

  Finder->addMatcher(
      implicitCastExpr(
          unless(anyOf(
              hasCastKind(CK_LValueToRValue),
              hasCastKind(CK_FunctionToPointerDecay), hasCastKind(CK_NoOp),
              hasCastKind(CK_UncheckedDerivedToBase),
              allOf(hasCastKind(CK_ArrayToPointerDecay),
                    hasDescendant(stringLiteral())),
              allOf(hasCastKind(CK_IntegralCast),
                    hasDescendant(integerLiteral())),
              hasCastKind(CK_NullToPointer), hasParent(cxxStaticCastExpr()),
              allOf(hasCastKind(CK_IntegralCast), hasParent(switchStmt())),
              allOf(hasCastKind(CK_IntegralCast),
                    hasParent(constantExpr(hasParent(caseStmt())))),
              hasCastKind(CK_DerivedToBase))))
          .bind("cast"),
      this);

  Finder->addMatcher(
      materializeTemporaryExpr(has(cxxBindTemporaryExpr(has(
                                   cxxConstructExpr(has(implicitCastExpr()))))))
          .bind("temporary"),
      this);

  Finder->addMatcher(cxxConstructExpr(hasDescendant(implicitCastExpr(
                                          hasCastKind(CK_DerivedToBase))))
                         .bind("slice"),
                     this);
}

const clang::Expr *getFromExpression(const clang::Expr *Expression,
                                     const QualType ResultType) {
  if (Expression != nullptr && !Expression->children().empty()) {
    const auto *Expr = dyn_cast<clang::Expr>(*Expression->children().begin());

    if (Expr) {
      if (Expr->getType() != ResultType) {
        return Expr;
      }

      return getFromExpression(Expr, ResultType);
    }
  }

  return nullptr;
}

void NoImplicitConversionsCheck::check(const MatchFinder::MatchResult &Result) {
  const auto PrintCast = [this](const auto *Matched) {
    if (Matched != nullptr) {
      const auto *FromExpr = getFromExpression(Matched, Matched->getType());

      diag(Matched->getBeginLoc(),
           "Implicit cast invoked to '%1' from '%2' (%0)")
          << Matched->getCastKindName() << Matched->getType().getAsString()
          << FromExpr->getType().getAsString();
    }
  };

  PrintCast(Result.Nodes.getNodeAs<ImplicitCastExpr>("cast"));

  const auto PrintTemporary = [this](const MaterializeTemporaryExpr *Matched) {
    if (Matched != nullptr) {
      const auto *FromExpr = getFromExpression(Matched, Matched->getType());
      diag(Matched->getBeginLoc(), "Implicit cast invoked to '%0' from '%1'")
          << Matched->getType().getAsString()
          << FromExpr->getType().getAsString();
    }
  };

  PrintTemporary(Result.Nodes.getNodeAs<MaterializeTemporaryExpr>("temporary"));

  const auto *SlicedDecl = Result.Nodes.getNodeAs<CXXConstructExpr>("slice");
  if (SlicedDecl != nullptr) {
    diag(SlicedDecl->getLocation(), "Implicit conversion resulting in slicing");
  }
}

} // namespace misc
} // namespace tidy
} // namespace clang
