# Repository Guidelines

## Project Structure & Module Organization
- `*.c` and `*.h` in the repo root contain the core DICOM library and CLI tools (e.g., `zzio.c`, `zzdump.c`).
- `tests/` holds unit and integration test programs plus CTest registrations.
- `samples/` includes sample DICOM files used by tests and examples.
- `nifti/` contains experimental NIfTI conversion tools and its own CMake target.
- `SQL/` and `zzqt/` include supporting SQL assets and a Qt-based viewer utility.

## Build, Test, and Development Commands
- Configure and build:
  - `mkdir -p build && cd build`
  - `cmake ..`
  - `make -j` (builds the library and CLI tools)
- Run tests:
  - `make test` (invokes CTest; expects sample files in `samples/`)
  - `ctest -V` for verbose output when diagnosing failures.

## Coding Style & Naming Conventions
- C code uses tabs for indentation and K&R-style braces on the next line.
- Keep file and function names short and descriptive (e.g., `zzread`, `zzwrite`).
- Prefer explicit error handling and avoid introducing new dependencies in core code.
- Compiler warnings are treated as errors (`-Werror`); fix warnings in new code.

## Testing Guidelines
- Tests are C executables defined in `tests/CMakeLists.txt` and registered via CTest.
- Naming follows concise prefixes (`zz1`, `zziotest`, `zznettest`).
- Use existing sample files in `samples/` or add new fixtures there with clear names.
- Run full suite with `make test`; add new tests to CMake so they run in CI.

## Commit & Pull Request Guidelines
- Recent commits use short, imperative summaries (e.g., “Fix CharLS library version incompatibilities”).
- Keep messages single-line and focused; add detail in the PR description.
- PRs should include: summary of changes, tests run, and any new dependencies.
- Include sample outputs or screenshots only if you modify user-facing tools or docs.

## Dependencies & Configuration Notes
- Optional features rely on SQLite, CharLS, GLUT, and Qt (see `README.md`).
