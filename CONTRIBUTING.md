# Contributing to Football Management

First off, thank you for considering contributing to Football Management! It's people like you that make open source such a great community.

## Getting Started

1. **Fork the repository** on GitHub.
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/FootballManagement.git
   cd FootballManagement
   ```
3. **Build the project** using CMake:
   ```bash
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Debug ..
   make -j $(nproc)
   ```
4. **Run the tests**:
   ```bash
   ctest --test-dir build -V
   ```

## Branching Strategy

- **Main branch:** `main` is our bleeding edge. It should always compile.
- **Epic branches:** Features that take weeks go into Epic branches (e.g. `epic/modern-ui-design`).
- **Issue branches:** Work on specific issues in their own branch, named after the issue number. For example, if you are working on Issue #42:
  ```bash
  git checkout -b 42-add-player-transfers
  ```

## Code Style

This project uses `clang-format` and `clang-tidy` to enforce coding standards.
- We use **Allman bracing style** and a 100-character line limit.
- Before committing, ensure your code matches the format:
  ```bash
  # Inside build directory
  make format
  ```
- Always check that your changes don't introduce new `clang-tidy` warnings.

## AI Guidelines

We are a **pro-AI** project! We encourage the use of AI tools (GitHub Copilot, ChatGPT, Antigravity, etc.) to accelerate your workflow. However, we have strict rules regarding accountability and review:

Please read our [AI Guidelines](AI_GUIDELINES.md) before submitting an AI-assisted Pull Request. All AI-generated code MUST be declared and heavily reviewed by the human author.

## Pull Requests

1. **Keep it focused:** A PR should only do one thing. If you find a typo while working on a feature, consider a separate PR.
2. **Pass all tests:** Ensure CI passes (or local tests pass).
3. **Fill out the template:** We have a PR template that includes a checklist. Please fill it out completely, especially the AI declaration.
4. **Link the Issue:** Use keywords like `Fixes #42` or `Closes #42` in your PR description.

## Code of Conduct

Please note that this project is released with a [Contributor Code of Conduct](CODE_OF_CONDUCT.md). By participating in this project you agree to abide by its terms.
