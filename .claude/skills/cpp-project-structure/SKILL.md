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
- Adding a new class, struct, or enum, or deciding which file a type belongs in
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
| **PS.9** | Each type (class, struct, or enum) gets its own partition file, named after the type |

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

## One Type per File (PS.9)

Each class, struct, or enum is defined in its own partition, and the file is named after it: `Color` lives in `Color.ixx` (`:color`), `Transform` in `Transform.ixx` (`:transform`). A file never holds two unrelated types.

What may share a type's file is what exists only to serve that type:

- Its nested types, and its free functions: helpers (`Intersect` next to `Rect`) and JSON conversions (`to_json` and `from_json` next to the type they convert)
- Constants of that type (`White` next to `Color`)
- An alias that only names that type's contents (`DrawPrimitive`, the variant `DrawItem` holds, next to `DrawItem`)
- A helper class that exists only to implement or support the file's namesake type: `TemplateProperty`, the implementation of the `Property` interface, in `Property.ixx`, and `Creator` and `TemplateCreator`, which `ObjectFactory` uses to build objects, in `ObjectFactory.ixx`. If other code starts using a helper on its own, give it its own file.

```
src/graphics/
  Color.ixx        // Color, White, to_json/from_json for Color
  Rect.ixx         // Rect, Intersect, to_json/from_json for Rect
  Transform.ixx    // Transform
  DrawItem.ixx     // DrawPrimitive, DrawItem
```

Partitions import only what they use, so a type's file imports the partitions of the types it mentions. When two types seem to need each other, find the one-way order rather than forward-declaring a type in one partition and defining it in another: move what causes the cycle (a member, a `friend`, a back-pointer) until every import points the same way. Declarations split across partitions are legal, but are where GCC's module support is least reliable.


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

Tests live in `src/<folder>/test/`, named `<Partition>.test.cpp`, and link against `aspire-<folder>`. With one type per partition, that means one test file per type, such as `Color.test.cpp` and `Transform.test.cpp`. A test that needs several partitions together, such as filling a `DrawList` through `Collect`, goes in the test file of the partition that drives it.

## Anti-Patterns

- A folder without `Module.ixx`, or with more than one primary module interface
- Module names that don't match the folder (`aspire.coreutils` in `src/core`)
- Namespaces that differ from the module path (`namespace aspire { ... }` in `src/core`)
- Code, declarations, or namespaces inside `Module.ixx` beyond `export import` lines
- Partitions that are not re-exported from `Module.ixx`
- Importing partitions from outside their module
- Nested folders under `src/<folder>` other than `test/`
- Several types in one partition, or a catch-all file such as `Types.ixx` or `Common.ixx` (PS.9)
- A type forward-declared in one partition and defined in another (PS.9)

## Checklist

- [ ] `src/<folder>/Module.ixx` exists and declares `export module aspire.<folder>;`
- [ ] Every other `.ixx` declares `export module aspire.<folder>:<partition>;`
- [ ] `Module.ixx` has an `export import :<partition>;` for each partition
- [ ] All declarations are in `aspire::<folder>`
- [ ] Each class, struct, or enum has its own `<Type>.ixx` partition, holding only that type and what serves it, including helper classes nothing else uses
- [ ] All `.ixx` files are listed in the folder's `CXX_MODULES` file set
- [ ] `src/Module.ixx` re-exports `aspire.<folder>`
- [ ] Tests are under `src/<folder>/test/`
