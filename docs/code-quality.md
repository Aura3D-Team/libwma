# C++ checks

Inside `vulkan-dev`, from `/home/developer/workspace/libwma`:

```sh
cmake --preset linux-debug
cmake --build build/linux/debug -j8
python3 ci/check-cpp.py format
python3 ci/check-cpp.py format --fix
python3 ci/check-cpp.py lint --build build/linux/debug
```

Requires `clang-format-21` and `clang-tidy-21`. The Linux Debug preset exports
`compile_commands.json`; build before linting so generated headers exist.

| Option | Scope |
|---|---|
| Default | Changes against `HEAD`, including staged, unstaged and untracked C++ files |
| `--base REV` | Changed lines against a PR base or previous push |
| `--all` | All owned C++ files, including existing style/lint debt |
| Explicit file paths | Entire named files |

CI checks formatting before the Linux Debug build and lint after it; diagnostics
fail the job. Owned directories: `include`, `src`, `tests`, `examples`. Header
changes analyze all owned translation units in the compilation database, with
diagnostics limited to changed lines. Sources outside that build are reported
separately; build and generated directories are excluded.

Rules match Aura3D: LLVM/Allman, four spaces, 120 columns; Clang analysis,
bug-prone and performance checks. Enum storage, heuristic parameter-order and
unsafe-C-function checks are excluded. Lint fixes are never automatic.
