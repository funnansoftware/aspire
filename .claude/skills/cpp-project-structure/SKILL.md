---
name: cpp-project-structure
description: Project layout rules for C++ modules. Use when adding a new src/<folder>, creating or renaming module files, choosing module or namespace names, or wiring a library into CMake.
---

# C++ Project Structure

Every folder under `src/` is one C++ module with a single public entry point, `Module.ixx`. The project name is `aspire`; `<folder>` below is the folder's name under `src/`.

## When to Use

- Adding a new `src/<folder>` library
- Adding, moving, or renaming files inside an existing `src/<folder>`
- Choosing a module name, partition name, or namespace
- Registering module sources in CMake

## Rules

| Rule | Summary |
|------|---------|
| **PS.1** | Each `src/<folder>` has exactly one `Module.ixx` that exports the module |
| **PS.2** | The module name is `<project name>.<folder name>` (e.g. `aspire.core`) |
| **PS.3** | The namespace is `<project name>::<folder name>` (e.g. `aspire::core`) |
| **PS.4** | Every other `.ixx` in the folder is a partition of that module: `<module name>:<partition>` |
| **PS.5** | `Module.ixx` re-exports every partition with `export import :<partition>;` and contains nothing else |
| **PS.6** | Consumers import the module (`import aspire.core;`), never a partition directly |
| **PS.7** | The top-level `src/Module.ixx` (`aspire`) re-exports each folder's module |
| **PS.8** | Folder names are lowercase single words, matching their module and namespace segment |

## Layout

```
src/
  Module.ixx                 // export module aspire;  re-exports every folder
  core/
    CMakeLists.txt
    Module.ixx               // export module aspire.core;
    Vec2.ixx                 // export module aspire.core:vec2;
    Object.ixx               // export module aspire.core:object;
    test/
      CMakeLists.txt
      Object.test.cpp
```

## Module Interface (PS.1, PS.2, PS.5)

```cpp
// src/core/Module.ixx
export module aspire.core;

export import :object;
export import :vec2;
```

`Module.ixx` lists partitions in alphabetical order. Adding a file to the folder means adding its `export import` line here.

## Partitions (PS.3, PS.4)

```cpp
// src/core/Vec2.ixx
export module aspire.core:vec2;

export namespace aspire::core
{
    struct Vec2
    {
        float x{0.0F};
        float y{0.0F};
    };
}
```

- One partition per file; the file name is the `CamelCase` partition name and the partition name is its lowercase form (`Vec2.ixx` -> `:vec2`).
- Partitions import each other with `import :other;`, and import other project modules by their full name (`import aspire.string;`).
- Everything in the folder lives in `aspire::<folder>`, even when the file is a partition.

## Consuming Modules (PS.6, PS.7)

```cpp
// Anywhere outside the module: import the whole module
import aspire.core;

// DON'T: partitions are not importable outside their module
import aspire.core:vec2;  // ill-formed outside aspire.core
```

```cpp
// src/Module.ixx
export module aspire;

export import aspire.core;
export import aspire.parser;
```

## CMake

Each folder is one library named `<project name>-<folder name>`, with all `.ixx` files in a `CXX_MODULES` file set (including `Module.ixx`).

```cmake
# src/core/CMakeLists.txt
project(aspire-core)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Module.ixx
        Object.ixx
        Vec2.ixx
)

add_subdirectory(test)
```

Tests live in `src/<folder>/test/`, named `<Partition>.test.cpp`, and link against `aspire-<folder>`.

## Anti-Patterns

- A folder without `Module.ixx`, or with more than one primary module interface
- Module names that don't match the folder (`aspire.coreutils` in `src/core`)
- Namespaces that differ from the module path (`namespace aspire { ... }` in `src/core`)
- Code, declarations, or namespaces inside `Module.ixx` beyond `export import` lines
- Partitions that are not re-exported from `Module.ixx`
- Importing partitions from outside their module
- Nested folders under `src/<folder>` other than `test/`

## Checklist

- [ ] `src/<folder>/Module.ixx` exists and declares `export module aspire.<folder>;`
- [ ] Every other `.ixx` declares `export module aspire.<folder>:<partition>;`
- [ ] `Module.ixx` has an `export import :<partition>;` for each partition
- [ ] All declarations are in `aspire::<folder>`
- [ ] All `.ixx` files are listed in the folder's `CXX_MODULES` file set
- [ ] `src/Module.ixx` re-exports `aspire.<folder>`
- [ ] Tests are under `src/<folder>/test/`
