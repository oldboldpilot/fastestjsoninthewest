# AI Agent Test Policy Enforcement

## MANDATORY COMPLIANCE NOTICE
All AI agents working on FastestJSONInTheWest MUST enforce the test organization policy without exception.

## Test Organization Rules (NON-NEGOTIABLE)

### 1. Test Directory Structure (STRICTLY ENFORCED)
```
tests/
├── unit/           # Individual component tests
├── integration/    # End-to-end workflow tests  
├── performance/    # Benchmarks and stress tests
└── compatibility/  # Cross-platform validation tests
```

### 2. Absolute Prohibitions
- **NO test files outside tests/ directory**
- **NO exceptions or temporary locations**
- **NO alternative directory structures**
- **NO legacy test organization tolerated**

### 3. Agent Enforcement Duties
Every AI agent MUST:
1. **Check test location** before creating any test file
2. **Reject requests** for tests outside tests/ directory
3. **Move existing tests** to correct locations when discovered
4. **Update build system** to reflect proper test organization
5. **Educate users** about the mandatory test policy

### 4. Compliance Validation
Before ANY test-related action:
- [ ] Verify tests/ directory exists
- [ ] Confirm correct subdirectory for test type
- [ ] Check CMakeLists.txt references correct paths
- [ ] Validate no tests exist outside tests/

### 5. Immediate Actions Required
When discovering non-compliant test organization:
1. **Stop current work**
2. **Document violation**
3. **Propose migration plan** 
4. **Execute migration** with user approval
5. **Update build system**
6. **Verify compliance**

## Test Categories (ENFORCE CORRECTLY)

### Unit Tests (tests/unit/)
- Individual component testing
- Fast execution (<1ms per test)
- High code coverage focus
- No external dependencies

### Integration Tests (tests/integration/)
- Complete workflow testing
- Feature interaction validation
- End-to-end functionality
- Real-world usage scenarios

### Performance Tests (tests/performance/)
- Benchmarking and profiling
- Large file processing (2GB+ REQUIRED)
- SIMD optimization validation
- Memory usage monitoring

### Compatibility Tests (tests/compatibility/)
- Cross-platform validation
- Compiler compatibility
- C++ standard compliance
- Legacy support verification

## Quality Gates (MANDATORY)

### Performance Requirements
- **Small JSON (<1KB)**: <0.1ms parse time
- **Medium JSON (1MB)**: <10ms parse time
- **Large JSON (100MB)**: <1s parse time
- **Massive JSON (2GB+)**: <30s parse time

### Coverage Requirements
- **Unit Tests**: >95% line coverage
- **Integration Tests**: >90% feature coverage
- **Performance Tests**: All benchmarks must pass
- **Compatibility Tests**: All target platforms

## Agent Collaboration Rules

### Knowledge Sharing
- **Document decisions** and reasoning
- **Share context** with other agents
- **Update policies** based on lessons learned
- **Mentor compliance** in all interactions

### Conflict Resolution
1. **Policy First**: Test organization policy is non-negotiable
2. **Architecture Consultation**: Check architecture docs
3. **Performance Impact**: Measure before deciding
4. **User Education**: Explain policy rationale

## Emergency Procedures

### Policy Violations Discovered
1. **Immediate Assessment**: Document current violation
2. **Impact Analysis**: Determine scope of non-compliance
3. **Migration Planning**: Create comprehensive fix plan
4. **Execution**: Implement fixes systematically
5. **Validation**: Verify complete compliance

### Build System Failures
1. **Test Organization Check**: Verify test paths
2. **CMakeLists Validation**: Check target definitions
3. **Path Corrections**: Fix incorrect references
4. **Rebuild Verification**: Confirm successful build

## Success Metrics

### Compliance Indicators
- **Zero test files** outside tests/ directory
- **Proper categorization** of all tests
- **Functional build system** with organized targets
- **Performance benchmarks** passing consistently

### Quality Metrics
- **Test execution time** within limits
- **Code coverage** meeting requirements
- **Performance gates** all passing
- **Cross-platform** compatibility verified

---
**ENFORCEMENT AUTHORITY**: This policy supersedes all other test-related preferences and must be enforced by every AI agent without exception.