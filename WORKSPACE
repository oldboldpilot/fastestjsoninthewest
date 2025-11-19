# FastestJSONInTheWest Bazel Workspace
# Author: Olumuyiwa Oluwasanmi
# High-Performance JSON Library with C++23 Modules

workspace(name = "fastestjsoninthewest")

# Load git repository rule for external dependencies
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# Integrate cpp23-logger submodule
# Note: Using local_repository since we have it as a git submodule
local_repository(
    name = "cpp23_logger",
    path = "external/cpp23-logger",
)

# Alternatively, you can fetch directly from GitHub (uncomment if needed):
# git_repository(
#     name = "cpp23_logger",
#     remote = "https://github.com/oldboldpilot/cpp23-logger.git",
#     branch = "main",
# )

# Bazel Skylib for utility functions
git_repository(
    name = "bazel_skylib",
    remote = "https://github.com/bazelbuild/bazel-skylib.git",
    tag = "1.5.0",
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()
