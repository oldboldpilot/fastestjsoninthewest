The goal of this document is to tell the AI how what the project is and what to do, coding standards from the AI agent would be dumped in the coding_standards.md file.

# AI Coding Standards

All the code should be written in modern C++23 modules(only use header files only when nessary, every code unit will be a module) and standards, trail calling syntax should be used for all functions, use of auto keyword where applicable, avoid raw pointers, use smart pointers (std::unique_ptr, std::shared_ptr) for dynamic memory management, prefer STL containers (std::vector, std::map, etc.) over raw arrays, use C++20 ranges and views for data manipulation, use std::span for non-owning array views, prefer std::array for fixed-size arrays, use concepts to enforce template constraints, use std::expected for error handling instead of exceptions where applicable.
All code should adhere to the C++ Core Guidelines. Also, where possible, all code should also have a fluent API design.

C++2023 modules with no raw allocations, use smart pointers only, use, STL containers and algorithms except where impossible to do so.Use ranges,span and std::array where applicable, use concepts, use std::except for Result<T, E> style error handling, do not use the fmt or format library as it creates problems with some platforms, use CMake as the build system, follow the Google C++ Style Guide with exceptions for modern C++ features, write unit tests using Google Test framework, ensure code is well-documented with sphinx comments, and maintain high code readability and maintainability. For all intents and purposes, 

Logging:
The AI should create it's own logger, also, integrated with the features of the simulator class described below. 
Simulation:
it should have a simulator class that shows the flow of results from the logger, as in the logger should log to the simulator class which will show the logs in a GUI window, the GUI framework to be used is ImGui.So we can see the logs not only in the console but in a GUI window as well and so we can see the computation flow of a component or logger visually, meaning the simulator class should have a visual representation of the flow of data through the logger and other components, showing how data is processed and logged step by step.

Clang-tidy should be used to enforce coding standards and catch potential issues early in the development process.
clang-tidy should be used to enforce the C++ Core Guidelines and other best practices.
clang-format should be used to format code consistently across the project.

All major components should have comprehensive unit tests and we should have a regression test suite to ensure that new changes do not break existing functionality.
multithreading should be using the C++ standard library facilities such as std::thread, std::async, std::mutex, and std::condition_variable, in some cases OpenMP where applicable for easy parallelism, avoid using platform-specific threading libraries unless absolutely necessary.

Documentation:
Standard Documentation for the code should be well-documented markdown files explaining the architecture, design decisions, and usage instructions. A PRD document should be created which describes the product requirements in detail, including features, user stories, and acceptance criteria. Where possible, all code should also have a fluent API design.

Generated Documentation:
Code Documentation should be generated using Sphinx, while in-code comments should follow the Doxygen style for consistency and clarity.
All public APIs should have clear and concise documentation explaining their purpose, parameters, return values, and any exceptions they may throw.
Our current compiler of choice is clang++ on Linux and MSVC on Windows with C++23 standard enabled.

