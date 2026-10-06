# Contributing to Football Management

First off, thank you for considering contributing to Football Management! It's people like you that make open source such a great community.

## Getting Started

1. **Fork the repository** on GitHub.
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/FootballManagement.git
   cd FootballManagement
   ```
3. **Build the project** with CMake (3.29 or newer) and Ninja. The first
   configure fetches the pinned dependencies; see the
   [README](README.md#building) for the system packages SDL needs.
   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
   cmake --build build --parallel 2
   ```
   The presets in `CMakePresets.json` (`release`, `release-tests`,
   `debug-sanitized`, `profile`, `clang-tidy`) are described in
   [docs/development/builds.md](docs/development/builds.md).
4. **Run the tests**:
   ```bash
   # Fast suites (what CI runs)
   ctest --test-dir build --output-on-failure -LE "playtest|monkey|slow"

   # One area, e.g. the headless core tests or a single suite
   ctest --test-dir build --output-on-failure -L core
   ctest --test-dir build --output-on-failure -R "TransferMarket"

   # Everything, including the long playtest, monkey and adversarial suites
   ctest --test-dir build --output-on-failure
   ```
   Labels: `unit`, `core`, `gui`, `lab`, `playtest`, `monkey`, `adversarial`
   and `slow`. Tests run headless with fixed seeds and a scratch data
   directory, so they never touch your saves.

## Performance Benchmarks

If your Pull Request introduces changes that might affect the game's performance (such as database IO, tight game loops, etc.), you **MUST** run the benchmarks locally. Note that these benchmarks are *not* run in CI to save resources and avoid flakiness.

To run the benchmarks:
```bash
# Configure the build with benchmarks enabled
cmake -S . -B build -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
# Build the benchmarks
cmake --build build --target db_benchmarks
# Execute the benchmarks
./build/benchmarks/db_benchmarks
```
Please include the benchmark output in your Pull Request description.

## Branching Strategy

- **Main branch:** `main` is our bleeding edge. It should always compile and pass all tests.
- **Epic branches:** Features that take weeks go into Epic branches (e.g. `epic/modern-ui-design`).
- **Issue branches:** Work on specific issues in their own branch, named after the issue number. For example, if you are working on Issue #42:
  ```bash
  git checkout -b 42-add-player-transfers
  ```

## Code Style

This project uses `clang-format` and `clang-tidy` to enforce coding standards.
- The style is in `.clang-format`: Google-based, **Allman braces**, 2-space
  indentation and an 80-column limit.
- Before committing, format the files you changed:
  ```bash
  clang-format -i src/path/to/file.cpp src/path/to/file.h
  ```
  or format all of `src/` and `test/` through the build (the `format` target
  exists when CMake finds `clang-format`):
  ```bash
  cmake --build build --target format
  ```
- Always check that your changes don't introduce new `clang-tidy` warnings
  (`cmake --preset clang-tidy && cmake --build --preset clang-tidy`).

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
