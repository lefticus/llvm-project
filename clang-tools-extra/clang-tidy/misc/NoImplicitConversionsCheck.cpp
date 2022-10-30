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
              allOf(hasCastKind(CK_ArrayToPointerDecay), has(stringLiteral())),
              allOf(hasCastKind(CK_IntegralCast), has(integerLiteral())),
              allOf(has(cxxNullPtrLiteralExpr()),
                    hasCastKind(CK_NullToPointer)),
              hasParent(cxxStaticCastExpr()),
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

    if (Expr != nullptr) {
      if (dyn_cast<clang::MemberExpr>(Expr) == nullptr &&
          Expr->getType().withoutLocalFastQualifiers() !=
              ResultType.withoutLocalFastQualifiers()) {
        return Expr;
      }

      return getFromExpression(Expr, ResultType);
    }
  }

  return nullptr;
}

void NoImplicitConversionsCheck::check(const MatchFinder::MatchResult &Result) {
  const auto PrintCast = [&, this](const Expr *Matched) {
    if (Matched != nullptr) {
      const auto *FromExpr = getFromExpression(Matched, Matched->getType());

      const auto Description = [&]() -> llvm::StringRef {
        if (Matched->getType()->isStructureOrClassType()) {
          // result type is not a built-in type

          if (Result.Context != nullptr &&
              !Matched->getType().isTriviallyCopyableType(*Result.Context)) {
            return " creates new non-trivial object";
          }
          // result type is trivial if we get here
          if (Result.Context != nullptr &&
              !FromExpr->getType().isTriviallyCopyableType(*Result.Context)) {
            // and from type is non-trivial
            return ", potential for dangling reference detected";
          }
          
          return " creates new object";
        }

        if (Matched->getType()->isPointerType()) {
          return ", use nullptr instead";
        }

        for (const auto &Parent :
             Result.Context->getParentMapContext().getParents(*Matched)) {
          if (Parent.get<CallExpr>() != nullptr) {
            return " affects overload resolution, and might change meaning if "
                   "more overloads are added";
          }
        }

        return " affects readability";
      }();

      diag(Matched->getExprLoc(), "implicit conversion from %0 to %1%2")
          << FromExpr->getType().withoutLocalFastQualifiers()
          << Matched->getType().withoutLocalFastQualifiers() << Description;
    }
  };

  PrintCast(Result.Nodes.getNodeAs<ImplicitCastExpr>("cast"));
  PrintCast(Result.Nodes.getNodeAs<MaterializeTemporaryExpr>("temporary"));

  const auto PrintSlicing = [this](const Expr *Matched) {
    if (Matched != nullptr) {
      const auto *FromExpr = getFromExpression(Matched, Matched->getType());

      diag(Matched->getExprLoc(), "implicit conversion from %0 to %1 causes "
                                  "slicing and creates new object")
          << FromExpr->getType().withoutLocalFastQualifiers()
          << Matched->getType().withoutLocalFastQualifiers();
    }
  };
  PrintSlicing(Result.Nodes.getNodeAs<CXXConstructExpr>("slice"));
}

} // namespace misc
} // namespace tidy
} // namespace clang
