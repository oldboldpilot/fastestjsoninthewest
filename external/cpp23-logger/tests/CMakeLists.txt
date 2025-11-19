add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE logger)

add_test(NAME test_logger COMMAND test_logger)
