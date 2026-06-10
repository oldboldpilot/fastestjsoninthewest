# Micro-benchmarks for the logger hot paths. Linked against the logger module
# exactly like the examples/tests. Run with: ./benchmarks/bench_logger
add_executable(bench_logger bench_logger.cpp)
target_link_libraries(bench_logger PRIVATE logger)
