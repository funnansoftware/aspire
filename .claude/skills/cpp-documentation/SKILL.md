---
name: cpp-documentation
description: Doxygen-style documentation comments for C++ classes, member functions, and free functions. Use when adding or changing a declaration that other code calls or derives from, or when reviewing whether public APIs are documented.
---

# C++ Documentation Comments

Document the API with Doxygen comments: `///` lines with `@` commands, placed on their own lines directly above the declaration. Doc comments describe the contract a caller relies on. Ordinary `//` comments explain implementation details inside a body.

## When to Use

- Adding or changing an exported class, struct, enum, concept, or namespace-scope function
- Adding or changing a public or protected member function, including virtual hooks meant to be overridden
- Reviewing whether a change leaves an API undocumented or its documentation out of date

### When NOT to Use

- Statements inside a function body: use `//` (see the Comments rule in `cpp-coding-standards`)
- Private helpers whose name and signature say everything (document them only when they carry a non-obvious contract)
- Test cases: the test name is the documentation

## Rules

| Rule | Summary |
|------|---------|
| **DOC.1** | Use `///` line comments. Don't use `/** */` blocks or `//!` |
| **DOC.2** | Use `@` commands (`@brief`, `@param`), never `\` commands |
| **DOC.3** | Put the doc comment on its own lines directly above the declaration. Never use trailing `///<` |
| **DOC.4** | Start every doc comment with `@brief` and one sentence |
| **DOC.5** | Separate the brief from any details with an empty `///` line |
| **DOC.6** | Give every parameter an `@param`, in declaration order, with the exact parameter name |
| **DOC.7** | Give every template parameter an `@tparam` |
| **DOC.8** | Give every non-`void` function an `@return`. Omit it for `void` |
| **DOC.9** | Document the contract (preconditions, effects, ownership, ordering, failure), not the implementation |
| **DOC.10** | Don't repeat the base class documentation on an `override`. Document an override only where its behaviour differs |
| **DOC.11** | Update the doc comment in the same change that changes the behaviour |

## Commands

| Command | Use for |
|---------|---------|
| `@brief` | One sentence saying what it is or does. Required |
| `@param name` | What the argument means and any constraint on it |
| `@tparam T` | What the template parameter must be, beyond its concept |
| `@return` | What the result means, including the failure value |
| `@pre` | What must be true before the call (state, thread, ownership) |
| `@post` | What is guaranteed after the call, when not obvious from `@return` |
| `@throws Type` | An exception the caller may see, and when |
| `@note` | A consequence that's easy to miss, such as re-entrancy or ordering |
| `@warning` | Misuse that compiles but breaks: lifetime, threading, recursion |
| `@see` | A related function or type, by name |

Refer to code in prose with backticks (`` `addChild()` ``, `` `State::Started` ``). Doxygen renders them as code.

## Classes

Say what the class is for and the invariant a user must keep. Put rules about placement, ownership, and lifecycle here, not on each member.

```cpp
/// @brief Base for objects that take part in the frame loop.
///
/// A Service must be a direct child of Engine. Engine ticks only its direct children, so a nested
/// Service starts and shuts down but never ticks.
class Service : public Object
{
public:
    /// @brief Advances the service by one variable-length frame.
    /// @param x Seconds since the previous frame.
    virtual auto update(float x) -> void = 0;
};
```

## Member and Free Functions

Name the effect, every failure path, and anything that runs as a side effect (hooks, child starts).

```cpp
/// @brief Attaches an object as the last child of this one.
///
/// If this object is `State::Started`, the child and its subtree start immediately. Otherwise they
/// start when this object does.
///
/// @param x The object to attach. Must not already have a parent; call `remove()` on it first to move it.
/// @return `true` if `x` was attached; `false` if `x` is null, already has a parent, or is this object or one
/// of its ancestors.
auto addChild(std::shared_ptr<Object> x) -> bool;
```

Parameter names in this project are often short (`x`). The `@param` text says what the value means, so a short name stays clear.

## Templates

```cpp
/// @brief Finds the child at an index among children of type `T`.
/// @tparam T The child type to filter on.
/// @param x Zero-based index among the children that are a `T`.
/// @return The child, or `nullptr` if fewer than `x + 1` children are a `T`.
template <ObjectType T>
auto getChild(std::size_t x = 0) -> std::shared_ptr<T>;
```

## Virtual Hooks

Document what calls the hook and when, what state the object is in at that moment, and what an override may do.

```cpp
/// @brief Called by `shutdown()` after this object's children have shut down.
///
/// The state is already `State::Shutdown`, so a re-entrant `shutdown()` does nothing.
///
/// @warning Must not throw: a throwing override terminates the program.
virtual auto onShutdown() noexcept -> void;
```

## Enums and Data Members

Document each enumerator and each public data member on the line above it.

```cpp
/// @brief Where an object is in its lifecycle.
enum class State : std::uint8_t
{
    /// Never started.
    Created,

    /// Live. `addChild()` starts new children immediately.
    Started,

    /// Was started, then shut down. `startup()` can start it again.
    Shutdown,
};
```

A one-sentence enumerator or data member may skip `@brief`.

## Anti-Patterns

- Trailing `///<` comments (DOC.3; breaks the own-line comment rule)
- Restating the signature: `/// @brief Gets the name.` above `getName()` adds nothing. Say what the name is used for, or keep only the brief when there's nothing more to say
- An `@param` name that doesn't match the declaration (clang's `-Wdocumentation` flags it)
- Describing the algorithm in a doc comment instead of the contract (DOC.9)
- Copying a base class's docs onto every override (DOC.10)
- Doc comments that describe the old behaviour after a change (DOC.11)

## Checklist

- [ ] Every exported class, struct, enum, concept, and namespace-scope function has a `///` doc comment
- [ ] Every public and protected member function has one, except overrides that behave as their base documents
- [ ] Each starts with `@brief` and one sentence, with an empty `///` line before any details
- [ ] Each parameter has a matching `@param`, each template parameter a `@tparam`, and each non-`void` function an `@return`
- [ ] Failure values, preconditions, and hooks that run as side effects are documented
- [ ] No trailing `///<` comments
