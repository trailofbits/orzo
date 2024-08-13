#include "macroni/Translation/Safety/SafetyVisitor.hpp"
#include "macroni/Dialect/Safety/SafetyOps.hpp"
#include "vast/CodeGen/CodeGenBuilder.hpp"
#include "vast/CodeGen/CodeGenVisitorBase.hpp"
#include "vast/CodeGen/CodeGenVisitorList.hpp"
#include "vast/CodeGen/Common.hpp"
#include "vast/CodeGen/DefaultStmtVisitor.hpp"
#include "vast/Util/Common.hpp"
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/LLVM.h>

namespace macroni::safety {
safety_visitor::safety_visitor(vast::cg::visitor_base &head,
                               vast::mcontext_t &mctx,
                               vast::cg::codegen_builder &bld,
                               vast::cg::meta_generator &mg,
                               safety_conditions &safe_block_conditions)
    : vast::cg::fallthrough_list_node(), m_mctx(mctx), m_bld(bld),
      m_safe_block_conditions(safe_block_conditions), m_view(head) {}

vast::operation safety_visitor::visit(const vast::cg::clang_stmt *stmt,
                                      vast::cg::scope_context &scope) {
  auto if_stmt = clang::dyn_cast<clang::IfStmt>(stmt);
  if (!if_stmt) {
    return next->visit(stmt, scope);
  }

  auto else_branch = if_stmt->getElse();
  if (!else_branch) {
    return next->visit(stmt, scope);
  }

  auto integer_literal =
      clang::dyn_cast<const clang::IntegerLiteral>(if_stmt->getCond());
  if (!integer_literal) {
    return next->visit(stmt, scope);
  }

  if (!m_safe_block_conditions.contains(integer_literal)) {
    return next->visit(stmt, scope);
  }

  auto mk_region_builder = [&](const vast::cg::clang_stmt *stmt) {
    return
        [this, stmt, &scope](auto &_bld, auto) { m_view.visit(stmt, scope); };
  };

  m_bld.compose<UnsafeRegion>()
      .bind(m_view.location(stmt))
      .bind(mk_region_builder(else_branch))
      .freeze();

  auto op = next->visit(else_branch, scope);

  return op;
}
} // namespace macroni::safety
