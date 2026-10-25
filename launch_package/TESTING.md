# Testing Guide

## Overview

BendCore includes a comprehensive test suite with:
- 50+ integration tests
- Stress tests (10K+ entities)
- Performance profiling
- Memory leak detection
- Cross-platform validation

## Running Tests

```bash
cd build
ctest                              # Run all tests
ctest -V                          # Verbose output
ctest -R "E2E"                     # Run only E2E tests
ctest -R "Stress"                  # Run stress tests
ctest --output-on-failure         # Show failures
```

## Test Categories

1. **Unit Tests** - Individual component testing
2. **Integration Tests** - Multi-phase workflows
3. **E2E Tests** - Complete user scenarios
4. **Stress Tests** - Large data handling
5. **Performance Tests** - Timing and memory
6. **Recovery Tests** - Crash resilience

## Performance Profiling

```bash
cd build
./bendcore_performance_test
```

## Memory Profiling

```bash
valgrind --leak-check=full ./bendcore_test
```

