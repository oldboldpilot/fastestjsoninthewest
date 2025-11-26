add_executable(basic_usage basic_usage.cpp)
target_link_libraries(basic_usage PRIVATE logger)

add_executable(named_templates named_templates.cpp)
target_link_libraries(named_templates PRIVATE logger)

add_executable(code_generation code_generation.cpp)
target_link_libraries(code_generation PRIVATE logger)

add_executable(fluent_codegen fluent_codegen.cpp)
target_link_libraries(fluent_codegen PRIVATE logger)
