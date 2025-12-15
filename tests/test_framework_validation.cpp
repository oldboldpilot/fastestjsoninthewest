/**
 * Test Framework Validation Tests
 * 
 * Simple tests to verify the local test framework works correctly.
 * Uses std::expected for test results - no external dependencies.
 * 
 * Build: clang++ -std=c++23 -o test_framework_validation test_framework_validation.cpp
 * Run: ./test_framework_validation
 */

#include "test_framework.h"
#include <vector>
#include <string>

// ============================================================================
// Basic Assertion Tests
// ============================================================================

TEST(TestFramework, ExpectTrueWorks) {
    EXPECT_TRUE(true);
    EXPECT_TRUE(1 + 1 == 2);
    EXPECT_TRUE(!false);
    return {};
}

TEST(TestFramework, ExpectFalseWorks) {
    EXPECT_FALSE(false);
    EXPECT_FALSE(1 > 2);
    EXPECT_FALSE(0);
    return {};
}

TEST(TestFramework, ExpectEqWorks) {
    EXPECT_EQ(5, 5);
    EXPECT_EQ(std::string("hello"), std::string("hello"));
    
    int a = 42;
    int b = 42;
    EXPECT_EQ(a, b);
    
    return {};
}

TEST(TestFramework, ExpectNeWorks) {
    EXPECT_NE(5, 6);
    EXPECT_NE(std::string("hello"), std::string("world"));
    return {};
}

TEST(TestFramework, ExpectLtLeGtGeWork) {
    EXPECT_LT(1, 2);
    EXPECT_LE(1, 2);
    EXPECT_LE(2, 2);
    EXPECT_GT(3, 2);
    EXPECT_GE(3, 2);
    EXPECT_GE(2, 2);
    return {};
}

TEST(TestFramework, ExpectNearWorks) {
    EXPECT_NEAR(1.0, 1.0001, 0.001);
    EXPECT_NEAR(3.14, 3.14159, 0.01);
    EXPECT_FLOAT_EQ(1.0f, 1.0f);
    EXPECT_DOUBLE_EQ(3.14159265358979, 3.14159265358979);
    return {};
}

TEST(TestFramework, ExpectThrowWorks) {
    EXPECT_THROW(throw std::runtime_error("test"), std::runtime_error);
    EXPECT_THROW(throw std::invalid_argument("test"), std::invalid_argument);
    return {};
}

TEST(TestFramework, ExpectNoThrowWorks) {
    EXPECT_NO_THROW({
        int x = 5;
        x += 10;
        (void)x;
    });
    return {};
}

TEST(TestFramework, ExpectNullAndNotNullWork) {
    int* p = nullptr;
    EXPECT_NULL(p);
    
    int value = 42;
    int* q = &value;
    EXPECT_NOT_NULL(q);
    
    return {};
}

TEST(TestFramework, ExpectStreqStrneWork) {
    EXPECT_STREQ("hello", "hello");
    EXPECT_STRNE("hello", "world");
    return {};
}

// ============================================================================
// Container and Algorithm Tests (to verify std includes work)
// ============================================================================

TEST(Containers, VectorOperations) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[4], 5);
    
    v.push_back(6);
    EXPECT_EQ(v.size(), 6);
    
    return {};
}

TEST(Containers, StringOperations) {
    std::string s = "Hello, World!";
    
    EXPECT_EQ(s.size(), 13);
    EXPECT_TRUE(s.find("World") != std::string::npos);
    EXPECT_FALSE(s.empty());
    
    return {};
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(EdgeCases, EmptySuccess) {
    // Just return success without any assertions
    return {};
}

TEST(EdgeCases, MultipleAssertions) {
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
    EXPECT_EQ(1, 1);
    EXPECT_NE(1, 2);
    EXPECT_LT(1, 2);
    EXPECT_GT(2, 1);
    return {};
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    return RUN_ALL_TESTS();
}
