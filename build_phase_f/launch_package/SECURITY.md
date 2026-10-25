# Security Guidelines

## Overview

BendCore follows security best practices throughout the codebase.

## Security Features

### Input Validation
- All user input is validated before processing
- File format parsing includes size limits
- XML/JSON parsing uses safe parsers

### Memory Safety
- No raw pointers (std::shared_ptr, std::optional)
- RAII for resource management
- Bounds checking on array access

### Data Protection
- Sensitive data not logged
- Temporary files cleaned up
- No hardcoded secrets

### Thread Safety
- Shared data protected with std::mutex
- Atomic operations for counters
- No race conditions

## Reporting Vulnerabilities

If you discover a security vulnerability, please email: security@bendcore.io

