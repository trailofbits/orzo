#pragma once

#include "vast/CodeGen/AttrVisitorProxy.hpp"
#include "vast/CodeGen/CodeGenDriver.hpp"
#include "vast/CodeGen/CodeGenFunction.hpp"
#include "vast/CodeGen/CodeGenMetaGenerator.hpp"
#include "vast/CodeGen/CodeGenVisitorBase.hpp"
#include "vast/CodeGen/CodeGenVisitorList.hpp"
#include "vast/CodeGen/DefaultVisitor.hpp"
#include "vast/CodeGen/FallthroughVisitor.hpp"
#include "vast/CodeGen/TypeCachingProxy.hpp"
#include "vast/CodeGen/UnsupportedVisitor.hpp"
#include "vast/Frontend/Action.hpp"
#include "vast/Util/Common.hpp"
#include <memory>
#include <mlir/IR/Dialect.h>
#include <optional>
#include <utility>

namespace macroni {

using maybe_mod_and_context = std::optional<
    std::pair<vast::vast_module, std::unique_ptr<vast::mcontext_t>>>;

// Creates a vast::cg::driver and vast::mcontext_t with all MLIR and VAST
// dialects preloaded along with the given dialect and metagenerator from the
// given AST, sets up the default visitor list, places the given visitor at the
// front of the list, emits the driver's module, and returns the result along
// with the created context. The visitor's constructor is passed the given
// mlir::MLIRContext, an MLIR::OpBuilder, and a vast::cg::meta_generator first,
// then the extra arguments `args`.
template <typename dialect_t, typename metagen_t, typename visitor_t,
          typename... args_t>
  requires std::derived_from<dialect_t, mlir::Dialect> &&
           std::derived_from<metagen_t, vast::cg::meta_generator> &&
           std::derived_from<visitor_t, vast::cg::visitor_base>
maybe_mod_and_context mk_mod_and_mctx(vast::acontext_t &actx,
                                      args_t &&...args) {
  using namespace vast::cg;

  // Create an MLIR context with all VAST and MLIR dialects pre-loaded.
  auto mctx = vast::cc::mk_mcontext();

  // Insert our custom dialect.

  mlir::DialectRegistry registry;
  registry.insert<dialect_t>();
  mctx->appendDialectRegistry(registry);
  mctx->loadAllAvailableDialects();

  // Set up variables for constructing the codegen driver.

  auto bld = mk_codegen_builder(*mctx);
  auto mg = std::make_shared<metagen_t>(&actx, &*mctx);
  auto man_ctx = actx.createMangleContext();
  auto sg = std::make_shared<default_symbol_generator>(man_ctx);

  auto visitors =
      std::make_shared<visitor_list>() |
      as_node_with_list_ref<visitor_t>(*mctx, *bld, *mg,
                                       std::forward<args_t>(args)...) |
      as_node_with_list_ref<attr_visitor_proxy>() |
      as_node<type_caching_proxy>() |
      as_node_with_list_ref<default_visitor>(*mctx, actx, *bld, mg, sg,
                                             /* strict return = */ true,
                                             missing_return_policy::emit_trap) |
      as_node_with_list_ref<unsup_visitor>(*mctx, *bld) |
      as_node<fallthrough_visitor>();

  // Create the codegen driver.

  auto drv = driver(actx, *mctx, std::move(bld), visitors);
  drv.enable_verifier(true);

  // Emit the lowered MLIR.

  drv.emit(actx.getTranslationUnitDecl());
  drv.finalize();
  if (!drv.verify()) {
    return std::nullopt;
  }
  return std::make_pair(drv.freeze().release(), std::move(mctx));
}
} // namespace macroni
