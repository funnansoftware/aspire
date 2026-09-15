# aspire
Personal integration and rendering engine.

## Static analysis

The `clang-tidy` and `clang-tidy-diff` build targets are available when the C++
compiler is Clang. Configure with a Clang preset, then run either target:

```powershell
cmake --preset x64-windows-clang-debug
cmake --build --preset x64-windows-clang-debug --target clang-tidy
cmake --build --preset x64-windows-clang-debug --target clang-tidy-diff
```

```sh
cmake --preset x64-linux-clang-debug
cmake --build --preset x64-linux-clang-debug --target clang-tidy
cmake --build --preset x64-linux-clang-debug --target clang-tidy-diff
```

Release builds use `x64-windows-clang-release` or `x64-linux-clang-release`.
You can also use `cmake --build <clang-build-directory> --target clang-tidy`.
MSVC and GCC builds do not expose these targets.

Install Clang and clang-tidy from the same LLVM release, their `run-clang-tidy`
and `clang-tidy-diff.py` scripts, Python 3, Git, and Ninja. The Windows
Clang presets use clang-cl with the MSVC STL and Windows SDK configured by the
existing MSVC preset. Linux requires a standard library with `import std`
support.

Both targets use LLVM's Python runners in parallel and build project dependencies
first so the current build's Clang modules are available. `clang-tidy` checks
compiled C/C++ sources and module interfaces under `src/` and `app/`, including
tests. Build output and vendored sources are excluded.

`clang-tidy-diff` reports findings on changed lines in those files, comparing the
working tree (including staged changes) against `origin/main`, or local `main`
when `origin/main` is unavailable at configuration time. Untracked files and
standalone headers are outside this diff check. LLVM still analyzes each changed
file in full. Its diff parser cannot handle repository-relative filenames with
spaces; use the full target for such files.

Both targets use `.clang-tidy`, propagate failures, and do not apply fixes.
Parallelism defaults to the detected CPU count. Configure a job limit or another
comparison base as needed:

```powershell
cmake --preset x64-windows-clang-debug -DASPIRE_CLANG_TIDY_JOBS=4 -DASPIRE_CLANG_TIDY_DIFF_BASE=main
```

Tools are discovered automatically, including LLVM's Windows `bin` and
`share/clang` locations. Overrides are `ASPIRE_CLANG_TIDY_EXECUTABLE`,
`ASPIRE_RUN_CLANG_TIDY_SCRIPT`, `ASPIRE_CLANG_TIDY_DIFF_SCRIPT`, and
`Python3_EXECUTABLE`.
