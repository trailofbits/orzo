set(LLVM_LINK_COMPONENTS Core Support AsmParser)

add_executable(macronify Macronify.cpp ParseAST.cpp)

llvm_update_compile_flags(macronify)

# Suppress warnings in PASTA headers.
set_target_properties(
  pasta pasta_cxx_settings
  PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
             $<TARGET_PROPERTY:pasta,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(
  macronify PRIVATE macroni_settings pasta pasta_cxx_settings macroni_common
                    macroni_translation_api MLIRMacroni)

mlir_check_link_libraries(macronify)
