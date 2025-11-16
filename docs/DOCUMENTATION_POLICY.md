# Documentation Organization Policy

## Commit Author Policy

**CRITICAL REQUIREMENT**: All git commits must use ONLY the following author information:

```
Name: Olumuyiwa Oluwasanmi
Email: [your email]
```

**PROHIBITED**: Any co-authored-by tags, mentions of AI assistants (Claude, Anthropic, GitHub Copilot), or any attribution to artificial intelligence tools.

## Documentation Structure

### `/docs/` - Main Documentation Directory

#### `/docs/development/` - Development Documentation
- **MVP_COMPLETE.md**: Complete MVP implementation details and results
- **C++17_COMPATIBILITY_REPORT.md**: C++17 compatibility analysis and implementation variants
- **ImplementationPlan.md**: Overall project implementation strategy
- **TechnicalDesign.md**: Detailed technical architecture and design decisions

#### `/docs/results/` - Results and Performance Data
- **MVP_RESULTS.md**: Performance benchmarks and test results
- **Dependencies.md**: Project dependency analysis and requirements

#### `/docs/toolchain/` - Toolchain and Build Documentation  
- **CLANG21_BUILD_RESEARCH.md**: Comprehensive research on LLVM/Clang 21.1.5 build configuration
- **Architecture.md**: System architecture overview

#### `/docs/ai-agents/` - AI Development Tools Documentation
- **MultiAgentSystem.md**: Multi-agent development system documentation
- **coding_standards.md**: Code quality standards and guidelines
- **CLAUDE.md**: AI assistant interaction patterns and workflows

#### `/docs/` - Root Documentation Files
- **PRD.md**: Product Requirements Document

### Main Project Files
- **README.md**: Primary project overview and getting started guide
- **LICENSE**: Project license terms
- **CMakeLists.txt**: Primary build configuration

## Agent Awareness Notice

All AI agents working on this project must:
1. **NEVER** add co-authored-by tags in commits
2. **NEVER** mention AI assistance in commit messages
3. **ALWAYS** ensure commits are attributed to "Olumuyiwa Oluwasanmi" only
4. Follow the documentation organization structure above
5. Maintain professional, technical commit messages without AI references

## File Movement History

**November 16, 2025**: Reorganized project documentation into structured directories:
- Moved development docs: `MVP_COMPLETE.md`, `C++17_COMPATIBILITY_REPORT.md` → `/docs/development/`
- Moved results: `MVP_RESULTS.md` → `/docs/results/` 
- Moved toolchain docs: `CLANG21_BUILD_RESEARCH.md` → `/docs/toolchain/`
- Moved AI docs: `ai/MultiAgentSystem.md`, `ai/coding_standards.md`, `ai/CLAUDE.md` → `/docs/ai-agents/`