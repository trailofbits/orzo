// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ParseAST.hpp"
#include <iostream>
#include <llvm/Support/CommandLine.h>
#include <macroni/Conversion/MacroniRewriters.hpp>
#include <macroni/Translation/MacroniCodeGenVisitorMixin.hpp>
#include <macroni/Translation/MacroniMetaGenerator.hpp>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>
#include <mlir/Transforms/Passes.h>
#include <optional>
#include <pasta/AST/AST.h>
#include <vast/Translation/CodeGen.hpp>

int main(int argc, char **argv) {
    bool convert = false;
    for (int i = 0; i < argc; i++) {
        if (strncmp("--convert", argv[i], 10) == 0) {
            convert = true;
        }
    }

    auto maybe_ast = pasta::parse_ast(argc, argv);
    if (!maybe_ast.Succeeded()) {
        std::cerr << maybe_ast.TakeError() << '\n';
        return EXIT_FAILURE;
    }
    auto ast = maybe_ast.TakeValue();

    // Register the MLIR dialects we will be lowering to
    mlir::DialectRegistry registry;
    registry.insert<
        vast::hl::HighLevelDialect,
        macroni::macroni::MacroniDialect,
        macroni::kernel::KernelDialect
    >();
    auto mctx = mlir::MLIRContext(registry);
    macroni::MacroniMetaGenerator meta(ast, &mctx, convert);
    vast::cg::CodeGenContext cgctx(mctx, ast.UnderlyingAST());
    vast::cg::CodeGenBase<macroni::MacroniVisitor> codegen(cgctx, meta);

    // Generate the MLIR
    auto tud_decl = ast.UnderlyingAST().getTranslationUnitDecl();
    auto mod = codegen.emit_module(tud_decl);

    if (convert) {
        // Register conversions
        mlir::RewritePatternSet patterns(&mctx);
        patterns.add(macroni::rewrite_get_user)
            .add(macroni::rewrite_offsetof)
            .add(macroni::rewrite_container_of)
            .add(macroni::rewrite_rcu_dereference)
            .add(macroni::rewrite_smp_mb)
            .add(macroni::rewrite_list_for_each)
            .add(macroni::rewrite_rcu_read_unlock)
            .add(macroni::rewrite_safe_unsafe);

        // Apply the conversions.
        mlir::FrozenRewritePatternSet frozen_pats(std::move(patterns));
        mod->walk([&frozen_pats](mlir::Operation *op) {
            using ME = macroni::macroni::MacroExpansion;
            using FO = vast::hl::ForOp;
            using CO = vast::hl::CallOp;
            using IO = vast::hl::IfOp;
            if (mlir::isa<ME, FO, CO, IO>(op)) {
                std::ignore = mlir::applyOpPatternsAndFold(op, frozen_pats);
            }}
        );
    }
    // Print the result
    mod->print(llvm::outs());

    return EXIT_SUCCESS;
}
