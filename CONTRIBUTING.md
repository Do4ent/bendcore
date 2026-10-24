# Contributing to BendCore CAD

Thank you for your interest in contributing to BendCore CAD! This document provides guidelines and instructions for contributing to the project.

## Code of Conduct

We are committed to providing a welcoming and inspiring community for all. Please read and adhere to our Code of Conduct in all your interactions within this project.

## Getting Started

### Prerequisites
- Node.js 18+ LTS
- npm 9+ or yarn 3+
- Git
- Basic understanding of TypeScript, React, and Node.js

### Setting Up Development Environment

```bash
# Fork and clone the repository
git clone https://github.com/[your-username]/bendcore.git
cd bendcore

# Add upstream remote
git remote add upstream https://github.com/[organization]/bendcore.git

# Install dependencies
npm install
cd backend && npm install && cd ..

# Create a feature branch
git checkout -b feature/your-feature-name
```

## Development Workflow

### 1. Code Standards

**TypeScript:**
- Use TypeScript strict mode
- Provide type annotations for function parameters and return types
- Avoid `any` type - use `unknown` when necessary with proper type guards
- Follow naming conventions: camelCase for variables/functions, PascalCase for classes/interfaces

**Code Style:**
```bash
# Format code with Prettier
npm run format

# Check code with ESLint
npm run lint

# Fix linting issues automatically
npm run lint:fix
```

**Comments:**
- Use JSDoc for public APIs
- Keep comments concise and meaningful
- Update comments when updating code

Example:
```typescript
/**
 * Calculates the bending angle for a tube segment
 * @param radius - Tube radius in mm
 * @param length - Segment length in mm
 * @param material - Material properties
 * @returns Bending angle in degrees
 */
export function calculateBendingAngle(
  radius: number,
  length: number,
  material: Material
): number {
  // Implementation
}
```

### 2. Testing Requirements

**Unit Tests:**
- Write tests for all new functions
- Aim for >90% code coverage
- Use Jest for testing

```bash
# Run all tests
npm test

# Run tests with coverage
npm run coverage

# Run tests in watch mode
npm test -- --watch
```

**Integration Tests:**
- Add integration tests for major features
- Test API endpoints and database operations

**Example Test:**
```typescript
describe('BendingRulesEngine', () => {
  describe('calculateBendingAngle', () => {
    it('should calculate correct angle for standard tube', () => {
      const result = calculateBendingAngle(10, 100, standardMaterial);
      expect(result).toBeCloseTo(45, 1);
    });

    it('should handle edge cases', () => {
      expect(() => calculateBendingAngle(-1, 100, standardMaterial))
        .toThrow('Radius must be positive');
    });
  });
});
```

### 3. Commit Message Conventions

Follow conventional commits format:

```
[TYPE]: Brief description (max 50 chars)

Detailed explanation of changes (if needed).
Wrap at 72 characters per line.

- Bullet point for major changes
- Reference issue numbers: Fixes #123

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com>
Claude-Session: [session-url]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `refactor`: Code refactoring
- `test`: Test additions/updates
- `docs`: Documentation changes
- `chore`: Build, dependencies, etc.
- `perf`: Performance improvements
- `ci`: CI/CD configuration

**Examples:**
```
feat: Add STEP file export with metadata preservation

- Implement STEP exporter with full assembly support
- Add compression for large files
- Include validation before export
- Update API documentation

Fixes #456
```

```
fix: Prevent memory leak in collision detection

Memory leak was caused by circular references in geometry cache.
Use WeakMap to allow garbage collection.

Fixes #123
```

### 4. Pull Request Process

1. **Create Feature Branch:**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make Changes:**
   - Keep commits logical and atomic
   - One feature per branch
   - Test frequently during development

3. **Keep Branch Updated:**
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

4. **Push and Create PR:**
   ```bash
   git push origin feature/your-feature-name
   ```

5. **PR Description Template:**
   ```markdown
   ## Description
   Brief description of changes

   ## Type of Change
   - [ ] Bug fix
   - [ ] New feature
   - [ ] Breaking change
   - [ ] Documentation update

   ## Changes Made
   - List specific changes
   - Include any relevant details

   ## Testing
   - [ ] Unit tests added
   - [ ] Integration tests added
   - [ ] All tests passing
   - [ ] >90% coverage maintained

   ## Documentation
   - [ ] README updated (if needed)
   - [ ] API documentation updated (if needed)
   - [ ] Comments added for complex code

   ## Checklist
   - [ ] Code follows style guidelines
   - [ ] No new warnings generated
   - [ ] Tests pass locally
   - [ ] Branch is up to date with main
   ```

6. **Code Review:**
   - Address feedback promptly
   - Keep discussion professional
   - Request re-review after changes

7. **Merge:**
   - Rebase and squash commits if requested
   - Use "Squash and merge" for single feature
   - Delete branch after merging

## Directory Structure

```
bendcore/
├── backend/              # Node.js backend
│   ├── src/
│   │   ├── converters/   # CAD format converters
│   │   ├── assembly/     # Assembly operations
│   │   ├── geometry/     # Geometric algorithms
│   │   ├── bending/      # Bending calculations
│   │   ├── features/     # Feature generation
│   │   ├── api/          # API routes
│   │   └── services/     # Business logic
│   ├── tests/
│   │   ├── unit/
│   │   ├── integration/
│   │   └── smoke/
│   └── package.json
│
├── src/                  # React frontend
│   ├── ui/              # React components
│   ├── modules/         # Feature modules
│   ├── screens/         # Pages/screens
│   ├── services/        # API clients
│   └── utils/           # Helper functions
│
├── docs/                # Documentation
│   ├── PHASE1/
│   ├── PHASE2/
│   ├── PHASE3/
│   ├── DEPLOYMENT/
│   ├── ARCHITECTURE/
│   └── GUIDES/
│
└── README.md
```

## Areas for Contribution

### High Priority
- [ ] CAD format support (DXF, SAT enhancements)
- [ ] FEA solver improvements
- [ ] UI/UX enhancements
- [ ] Performance optimizations
- [ ] Documentation

### Medium Priority
- [ ] Additional test coverage
- [ ] Error handling improvements
- [ ] API enhancements
- [ ] Security audits

### Low Priority
- [ ] Code style improvements
- [ ] Example projects
- [ ] Community features

## Reporting Issues

When reporting bugs or requesting features:

1. **Check existing issues** - Don't create duplicates
2. **Use clear title** - Describe the problem concisely
3. **Provide details:**
   - Operating system and version
   - Node.js version
   - Steps to reproduce
   - Expected vs. actual behavior
   - Screenshots/logs if applicable

**Example Issue:**
```markdown
## Bug: Memory leak in large assembly files

### Environment
- OS: macOS 13.2
- Node: 18.14.0
- BendCore: v1.2.0

### Steps to Reproduce
1. Open large assembly (500+ parts)
2. Scroll through parts list
3. Monitor memory usage

### Expected
Memory should remain constant

### Actual
Memory increases by 10MB per minute

### Logs
[paste relevant logs]
```

## Documentation Contributions

Good documentation is crucial. We welcome:
- User guides
- API documentation
- Code examples
- Tutorials
- Troubleshooting guides

Guidelines:
- Write in clear, concise English
- Use code examples where appropriate
- Keep documentation up-to-date
- Follow markdown best practices

## Questions or Need Help?

- 💬 Open a discussion on GitHub
- 📧 Email: yura.glushko@gmail.com
- 📚 Check existing documentation in `/docs`
- 💡 Review existing code and tests for patterns

## Recognition

Contributors will be:
- Added to CONTRIBUTORS.md
- Acknowledged in release notes
- Credited in project documentation

Thank you for making BendCore better! 🚀

---

*Last updated: October 2026*
*For detailed implementation guidelines, see documentation in `/docs`*
