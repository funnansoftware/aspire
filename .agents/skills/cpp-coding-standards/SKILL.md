---
name: cpp-coding-standards
description: C++ coding standards based on the C++ Core Guidelines (isocpp.github.io). Use when writing, reviewing, or refactoring C++ code to enforce modern, safe, and idiomatic practices.
metadata:
  origin: ECC
---

# C++ Coding Standards (C++ Core Guidelines)

Comprehensive coding standards for modern C++ (C++17/20/23) derived from the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). Enforces type safety, resource safety, immutability, and clarity.

## When to Use

- Writing new C++ code (classes, functions, templates)
- Reviewing or refactoring existing C++ code
- Making architectural decisions in C++ projects
- Enforcing consistent style across a C++ codebase
- Choosing between language features (e.g., `enum` vs `enum class`, raw pointer vs smart pointer)

### When NOT to Use

- Non-C++ projects
- Legacy C codebases that cannot adopt modern C++ features
- Embedded/bare-metal contexts where specific guidelines conflict with hardware constraints (adapt selectively)

## Cross-Cutting Principles

These themes recur across the entire guidelines and form the foundation:

1. **RAII everywhere** (P.8, R.1, E.6, CP.20): Bind resource lifetime to object lifetime
2. **Immutability by default** (P.10, Con.1-5, ES.25): Start with `const`/`constexpr`; mutability is the exception
3. **Type safety** (P.4, I.4, ES.46-49, Enum.3): Use the type system to prevent errors at compile time
4. **Express intent** (P.3, F.1, NL.1-2, T.10): Names, types, and concepts should communicate purpose
5. **Minimize complexity** (F.2-3, ES.5, Per.4-5): Simple code is correct code
6. **Value semantics over pointer semantics** (C.10, R.3-5, F.20, CP.31): Prefer returning by value and scoped objects

## Philosophy & Interfaces (P.*, I.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **P.1** | Express ideas directly in code |
| **P.3** | Express intent |
| **P.4** | Ideally, a program should be statically type safe |
| **P.5** | Prefer compile-time checking to run-time checking |
| **P.8** | Don't leak any resources |
| **P.10** | Prefer immutable data to mutable data |
| **I.1** | Make interfaces explicit |
| **I.2** | Avoid non-const global variables |
| **I.4** | Make interfaces precisely and strongly typed |
| **I.11** | Never transfer ownership by a raw pointer or reference |
| **I.23** | Keep the number of function arguments low |

### DO

```cpp
// P.10 + I.4: Immutable, strongly typed interface
struct Temperature {
    double kelvin;
};

auto Boil(const Temperature& water) -> Temperature;
```

### DON'T

```cpp
// Weak interface: unclear ownership, unclear units
auto Boil(double* temp) -> double;

// Non-const global variable
int Counter = 0;  // I.2 violation
```

## Functions (F.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **F.1** | Package meaningful operations as carefully named functions |
| **F.2** | A function should perform a single logical operation |
| **F.3** | Keep functions short and simple |
| **F.4** | If a function might be evaluated at compile time, declare it `constexpr` |
| **F.6** | If your function must not throw, declare it `noexcept` |
| **F.8** | Prefer pure functions |
| **F.16** | For "in" parameters, pass cheaply-copied types by value and others by `const&` |
| **F.20** | For "out" values, prefer return values to output parameters |
| **F.21** | To return multiple "out" values, prefer returning a struct |
| **F.43** | Never return a pointer or reference to a local object |
| **Project** | Always declare functions with trailing return types (`auto f() -> T`) |
| **Project** | Use free functions (`std::begin(c)`, `std::size(c)`) instead of container member functions |

### Parameter Passing

```cpp
// F.16: Cheap types by value, others by const&
auto Print(int x) -> void;                   // cheap: by value
auto Analyze(const std::string& data) -> void; // expensive: by const&
auto Transform(std::string s) -> void;       // sink: by value (will move)

// F.20 + F.21: Return values, not output parameters
struct ParseResult {
    std::string token;
    int position;
};

auto Parse(std::string_view input) -> ParseResult; // GOOD: return struct

// BAD: output parameters
auto Parse(std::string_view input,
           std::string& token, int& pos) -> void; // avoid this
```

### Trailing Return Types (Project Rule)

Declare and define every function, member function, operator, and template with a trailing return type: `auto name(params) -> Type`. This keeps names aligned at a consistent column, lets the return type see the parameters and class scope, and makes all signatures uniform.

```cpp
// DO: trailing return type everywhere, including void and members
auto Area(const Shape& s) -> double;
auto Log(std::string_view msg) -> void;

class Widget {
public:
    auto size() const -> std::size_t;
    auto operator=(const Widget&) -> Widget& = default;
};

template<typename T>
auto First(const std::vector<T>& v) -> const T&;

// DON'T: leading return types
double area(const Shape& s);
void log(std::string_view msg);
```

Exceptions: constructors, destructors, and conversion operators have no return type; lambdas may omit the trailing return type unless it must be stated explicitly. `main` may stay `int main()`.

### Free Functions Over Container Members (Project Rule)

Access containers through the standalone `std::` functions (`std::begin`, `std::end`, `std::cbegin`, `std::cend`, `std::rbegin`, `std::rend`, `std::size`, `std::ssize`, `std::empty`, `std::data`) rather than the equivalent member functions. The free functions also work on C arrays, `std::initializer_list`, and user types that only provide ADL-visible overloads, so generic code stays uniform.

```cpp
// DO: free functions
auto first = std::begin(values);
auto last = std::end(values);
const auto count = std::size(values);
if (std::empty(values)) { return; }
std::sort(std::begin(values), std::end(values));

// DON'T: member functions
auto first = values.begin();
const auto count = values.size();
if (values.empty()) { return; }
```

In generic code that must find user-defined overloads, bring the std version into scope first (`using std::begin; begin(c);`). Member functions with no standalone equivalent (`push_back`, `front`, `insert`, `find`, ...) are fine.

### Pure Functions and constexpr

```cpp
// F.4 + F.8: Pure, constexpr where possible
constexpr auto Factorial(int n) noexcept -> int {
    return (n <= 1) ? 1 : n * Factorial(n - 1);
}

static_assert(Factorial(5) == 120);
```

### Anti-Patterns

- Returning `T&&` from functions (F.45)
- Using `va_arg` / C-style variadics (F.55)
- Capturing by reference in lambdas passed to other threads (F.53)
- Returning `const T` which inhibits move semantics (F.49)

## Classes & Class Hierarchies (C.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **C.2** | Use `class` if invariant exists; `struct` if data members vary independently |
| **C.9** | Minimize exposure of members |
| **C.20** | If you can avoid defining default operations, do (Rule of Zero) |
| **C.21** | If you define or `=delete` any copy/move/destructor, handle them all (Rule of Five) |
| **C.35** | Base class destructor: public virtual or protected non-virtual |
| **C.41** | A constructor should create a fully initialized object |
| **C.46** | Declare single-argument constructors `explicit` |
| **C.67** | A polymorphic class should suppress public copy/move |
| **C.128** | Virtual functions: specify exactly one of `virtual`, `override`, or `final` |

### Rule of Zero

```cpp
// C.20: Let the compiler generate special members
struct Employee {
    std::string name;
    std::string department;
    int id;
    // No destructor, copy/move constructors, or assignment operators needed
};
```

### Rule of Five

```cpp
// C.21: If you must manage a resource, define all five
class Buffer {
public:
    explicit Buffer(std::size_t size)
        : data_(std::make_unique<char[]>(size)), size_(size) {}

    ~Buffer() = default;

    Buffer(const Buffer& other)
        : data_(std::make_unique<char[]>(other.size_)), size_(other.size_) {
        std::copy_n(other.data_.get(), size_, data_.get());
    }

    auto operator=(const Buffer& other) -> Buffer& {
        if (this != &other) {
            auto newData = std::make_unique<char[]>(other.size_);
            std::copy_n(other.data_.get(), other.size_, newData.get());
            data_ = std::move(newData);
            size_ = other.size_;
        }
        return *this;
    }

    Buffer(Buffer&&) noexcept = default;
    auto operator=(Buffer&&) noexcept -> Buffer& = default;

private:
    std::unique_ptr<char[]> data_;
    std::size_t size_;
};
```

### Class Hierarchy

```cpp
// C.35 + C.128: Virtual destructor, use override
class Shape {
public:
    virtual ~Shape() = default;
    virtual auto area() const -> double = 0;  // C.121: pure interface
};

class Circle : public Shape {
public:
    explicit Circle(double r) : radius_(r) {}
    auto area() const -> double override { return 3.14159 * radius_ * radius_; }

private:
    double radius_;
};
```

### Anti-Patterns

- Calling virtual functions in constructors/destructors (C.82)
- Using `memset`/`memcpy` on non-trivial types (C.90)
- Providing different default arguments for virtual function and overrider (C.140)
- Making data members `const` or references, which suppresses move/copy (C.12)

## Resource Management (R.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **R.1** | Manage resources automatically using RAII |
| **R.3** | A raw pointer (`T*`) is non-owning |
| **R.5** | Prefer scoped objects; don't heap-allocate unnecessarily |
| **R.10** | Avoid `malloc()`/`free()` |
| **R.11** | Avoid calling `new` and `delete` explicitly |
| **R.20** | Use `unique_ptr` or `shared_ptr` to represent ownership |
| **R.21** | Prefer `unique_ptr` over `shared_ptr` unless sharing ownership |
| **R.22** | Use `make_shared()` to make `shared_ptr`s |

### Smart Pointer Usage

```cpp
// R.11 + R.20 + R.21: RAII with smart pointers
auto widget = std::make_unique<Widget>("config");  // unique ownership
auto cache  = std::make_shared<Cache>(1024);        // shared ownership

// R.3: Raw pointer = non-owning observer
auto Render(const Widget* w) -> void {  // does NOT own w
    if (w) w->draw();
}

Render(widget.get());
```

### RAII Pattern

```cpp
// R.1: Resource acquisition is initialization
class FileHandle {
public:
    explicit FileHandle(const std::string& path)
        : handle_(std::fopen(path.c_str(), "r")) {
        if (!handle_) throw std::runtime_error("Failed to open: " + path);
    }

    ~FileHandle() {
        if (handle_) std::fclose(handle_);
    }

    FileHandle(const FileHandle&) = delete;
    auto operator=(const FileHandle&) -> FileHandle& = delete;
    FileHandle(FileHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}
    auto operator=(FileHandle&& other) noexcept -> FileHandle& {
        if (this != &other) {
            if (handle_) std::fclose(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

private:
    std::FILE* handle_;
};
```

### Anti-Patterns

- Naked `new`/`delete` (R.11)
- `malloc()`/`free()` in C++ code (R.10)
- Multiple resource allocations in a single expression (R.13 -- exception safety hazard)
- `shared_ptr` where `unique_ptr` suffices (R.21)

## Expressions & Statements (ES.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **ES.5** | Keep scopes small |
| **ES.20** | Always initialize an object |
| **ES.23** | Prefer `{}` initializer syntax |
| **ES.25** | Declare objects `const` or `constexpr` unless modification is intended |
| **ES.28** | Use lambdas for complex initialization of `const` variables |
| **ES.45** | Avoid magic constants; use symbolic constants |
| **ES.46** | Avoid narrowing/lossy arithmetic conversions |
| **ES.47** | Use `nullptr` rather than `0` or `NULL` |
| **ES.48** | Avoid casts |
| **ES.50** | Don't cast away `const` |

### Initialization

```cpp
// ES.20 + ES.23 + ES.25: Always initialize, prefer {}, default to const
const int maxRetries{3};
const std::string name{"widget"};
const std::vector<int> primes{2, 3, 5, 7, 11};

// ES.28: Lambda for complex const initialization
const auto config = [&] {
    Config c;
    c.timeout = std::chrono::seconds{30};
    c.retries = maxRetries;
    c.verbose = debugMode;
    return c;
}();
```

### Anti-Patterns

- Uninitialized variables (ES.20)
- Using `0` or `NULL` as pointer (ES.47 -- use `nullptr`)
- C-style casts (ES.48 -- use `static_cast`, `const_cast`, etc.)
- Casting away `const` (ES.50)
- Magic numbers without named constants (ES.45)
- Mixing signed and unsigned arithmetic (ES.100)
- Reusing names in nested scopes (ES.12)

## Error Handling (E.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **E.1** | Develop an error-handling strategy early in a design |
| **E.2** | Throw an exception to signal that a function can't perform its assigned task |
| **E.6** | Use RAII to prevent leaks |
| **E.12** | Use `noexcept` when throwing is impossible or unacceptable |
| **E.14** | Use purpose-designed user-defined types as exceptions |
| **E.15** | Throw by value, catch by reference |
| **E.16** | Destructors, deallocation, and swap must never fail |
| **E.17** | Don't try to catch every exception in every function |

### Exception Hierarchy

```cpp
// E.14 + E.15: Custom exception types, throw by value, catch by reference
class AppError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class NetworkError : public AppError {
public:
    NetworkError(const std::string& msg, int code)
        : AppError(msg), statusCode(code) {}
    int statusCode;
};

auto FetchData(const std::string& url) -> void {
    // E.2: Throw to signal failure
    throw NetworkError("connection refused", 503);
}

auto Run() -> void {
    try {
        FetchData("https://api.example.com");
    } catch (const NetworkError& e) {
        LogError(e.what(), e.statusCode);
    } catch (const AppError& e) {
        LogError(e.what());
    }
    // E.17: Don't catch everything here -- let unexpected errors propagate
}
```

### Anti-Patterns

- Throwing built-in types like `int` or string literals (E.14)
- Catching by value (slicing risk) (E.15)
- Empty catch blocks that silently swallow errors
- Using exceptions for flow control (E.3)
- Error handling based on global state like `errno` (E.28)

## Constants & Immutability (Con.*)

### All Rules

| Rule | Summary |
|------|---------|
| **Con.1** | By default, make objects immutable |
| **Con.2** | By default, make member functions `const` |
| **Con.3** | By default, pass pointers and references to `const` |
| **Con.4** | Use `const` for values that don't change after construction |
| **Con.5** | Use `constexpr` for values computable at compile time |

```cpp
// Con.1 through Con.5: Immutability by default
class Sensor {
public:
    explicit Sensor(std::string id) : id_(std::move(id)) {}

    // Con.2: const member functions by default
    auto id() const -> const std::string& { return id_; }
    auto lastReading() const -> double { return reading_; }

    // Only non-const when mutation is required
    auto record(double value) -> void { reading_ = value; }

private:
    const std::string id_;  // Con.4: never changes after construction
    double reading_{0.0};
};

// Con.3: Pass by const reference
auto Display(const Sensor& s) -> void {
    std::cout << s.id() << ": " << s.lastReading() << '\n';
}

// Con.5: Compile-time constants
constexpr double Pi = 3.14159265358979;
constexpr int MaxSensors = 256;
```

## Concurrency & Parallelism (CP.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **CP.2** | Avoid data races |
| **CP.3** | Minimize explicit sharing of writable data |
| **CP.4** | Think in terms of tasks, rather than threads |
| **CP.8** | Don't use `volatile` for synchronization |
| **CP.20** | Use RAII, never plain `lock()`/`unlock()` |
| **CP.21** | Use `std::scoped_lock` to acquire multiple mutexes |
| **CP.22** | Never call unknown code while holding a lock |
| **CP.42** | Don't wait without a condition |
| **CP.44** | Remember to name your `lock_guard`s and `unique_lock`s |
| **CP.100** | Don't use lock-free programming unless you absolutely have to |

### Safe Locking

```cpp
// CP.20 + CP.44: RAII locks, always named
class ThreadSafeQueue {
public:
    auto push(int value) -> void {
        std::lock_guard<std::mutex> lock(mutex_);  // CP.44: named!
        queue_.push(value);
        cv_.notify_one();
    }

    auto pop() -> int {
        std::unique_lock<std::mutex> lock(mutex_);
        // CP.42: Always wait with a condition
        cv_.wait(lock, [this] { return !std::empty(queue_); });
        const int value = queue_.front();
        queue_.pop();
        return value;
    }

private:
    std::mutex mutex_;             // CP.50: mutex with its data
    std::condition_variable cv_;
    std::queue<int> queue_;
};
```

### Multiple Mutexes

```cpp
// CP.21: std::scoped_lock for multiple mutexes (deadlock-free)
auto Transfer(Account& from, Account& to, double amount) -> void {
    std::scoped_lock lock(from.mutex_, to.mutex_);
    from.balance_ -= amount;
    to.balance_ += amount;
}
```

### Anti-Patterns

- `volatile` for synchronization (CP.8 -- it's for hardware I/O only)
- Detaching threads (CP.26 -- lifetime management becomes nearly impossible)
- Unnamed lock guards: `std::lock_guard<std::mutex>(m);` destroys immediately (CP.44)
- Holding locks while calling callbacks (CP.22 -- deadlock risk)
- Lock-free programming without deep expertise (CP.100)

## Templates & Generic Programming (T.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **T.1** | Use templates to raise the level of abstraction |
| **T.2** | Use templates to express algorithms for many argument types |
| **T.10** | Specify concepts for all template arguments |
| **T.11** | Use standard concepts whenever possible |
| **T.13** | Prefer shorthand notation for simple concepts |
| **T.43** | Prefer `using` over `typedef` |
| **T.120** | Use template metaprogramming only when you really need to |
| **T.144** | Don't specialize function templates (overload instead) |

### Concepts (C++20)

```cpp
#include <concepts>

// T.10 + T.11: Constrain templates with standard concepts
template<std::integral T>
auto Gcd(T a, T b) -> T {
    while (b != 0) {
        a = std::exchange(b, a % b);
    }
    return a;
}

// T.13: Shorthand concept syntax
auto Sort(std::ranges::random_access_range auto& range) -> void {
    std::ranges::sort(range);
}

// Custom concept for domain-specific constraints
template<typename T>
concept Serializable = requires(const T& t) {
    { t.serialize() } -> std::convertible_to<std::string>;
};

template<Serializable T>
auto Save(const T& obj, const std::string& path) -> void;
```

### Anti-Patterns

- Unconstrained templates in visible namespaces (T.47)
- Specializing function templates instead of overloading (T.144)
- Template metaprogramming where `constexpr` suffices (T.120)
- `typedef` instead of `using` (T.43)

## Standard Library (SL.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **SL.1** | Use libraries wherever possible |
| **SL.2** | Prefer the standard library to other libraries |
| **SL.con.1** | Prefer `std::array` or `std::vector` over C arrays |
| **SL.con.2** | Prefer `std::vector` by default |
| **SL.str.1** | Use `std::string` to own character sequences |
| **SL.str.2** | Use `std::string_view` to refer to character sequences |
| **SL.io.50** | Avoid `endl` (use `'\n'` -- `endl` forces a flush) |

```cpp
// SL.con.1 + SL.con.2: Prefer vector/array over C arrays
const std::array<int, 4> fixedData{1, 2, 3, 4};
std::vector<std::string> dynamicData;

// SL.str.1 + SL.str.2: string owns, string_view observes
auto BuildGreeting(std::string_view name) -> std::string {
    return "Hello, " + std::string(name) + "!";
}

// SL.io.50: Use '\n' not endl
std::cout << "result: " << value << '\n';
```

## Enumerations (Enum.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **Enum.1** | Prefer enumerations over macros |
| **Enum.3** | Prefer `enum class` over plain `enum` |
| **Enum.5** | Don't use ALL_CAPS for enumerators |
| **Enum.6** | Avoid unnamed enumerations |

```cpp
// Enum.3 + Enum.5: Scoped enum, no ALL_CAPS
enum class Color { red, green, blue };
enum class LogLevel { debug, info, warning, error };

// BAD: plain enum leaks names, ALL_CAPS clashes with macros
enum { RED, GREEN, BLUE };           // Enum.3 + Enum.5 + Enum.6 violation
#define MAX_SIZE 100                  // Enum.1 violation -- use constexpr
```

## Source Files & Naming (SF.*, NL.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **SF.1** | Use `.cpp` for code files and `.h` for interface files |
| **SF.7** | Don't write `using namespace` at global scope in a header |
| **SF.8** | Use `#include` guards for all `.h` files |
| **SF.11** | Header files should be self-contained |
| **NL.5** | Avoid encoding type information in names (no Hungarian notation) |
| **NL.8** | Use a consistent naming style |
| **NL.9** | Use ALL_CAPS for macro names only |
| **NL.10** | Project overrides: follow the `.clang-tidy` naming conventions (see below), not `underscore_style` |

### Header Guard

```cpp
// SF.8: Include guard (or #pragma once)
#ifndef PROJECT_MODULE_WIDGET_H
#define PROJECT_MODULE_WIDGET_H

// SF.11: Self-contained -- include everything this header needs
#include <string>
#include <vector>

namespace project::module {

class Widget {
public:
    explicit Widget(std::string name);
    auto name() const -> const std::string&;

private:
    std::string name_;
};

}  // namespace project::module

#endif  // PROJECT_MODULE_WIDGET_H
```

### Naming Conventions (Project Rule, enforced by `.clang-tidy`)

The project's `readability-identifier-naming` settings override the Core Guidelines' `underscore_style` (NL.10). Warnings are errors, so examples and new code must follow this table:

| Entity | Style | Example |
|--------|-------|---------|
| Classes, structs, enums, concepts | `CamelCase` | `TcpConnection`, `LogLevel`, `Serializable` |
| Namespace-scope functions | `CamelCase` | `ParseConfig` |
| Member functions, static methods | `camelBack` | `sendMessage` |
| Namespace-scope constants and variables | `CamelCase` | `MaxBufferSize` |
| Local variables and constants | `camelBack` | `retryCount` |
| Parameters and parameter packs | `camelBack` | `timeout`, `args` |
| Data members (public) | `camelBack` | `statusCode` |
| Private / protected data members | `camelBack` + trailing `_` | `host_` |
| Private static members | `camelBack` + trailing `_` | `instanceCount_` |
| Namespaces | `camelBack` | `myProject` |
| Macros | `ALL_CAPS` (only macros) | `PROJECT_MODULE_WIDGET_H` |

Names dictated by an external framework (e.g. gtest's `SetUp`, libFuzzer's `LLVMFuzzerTestOneInput`) keep their required spelling and are suppressed with `// NOLINTNEXTLINE(readability-identifier-naming)` on the preceding line. Prefer `NOLINTNEXTLINE` over trailing `NOLINT` for all clang-tidy suppressions, and always name the specific check.

```cpp
// NL.8 + project rule: CamelCase types, camelBack functions/variables
namespace myProject {

constexpr int MaxBufferSize = 4096;  // NL.9: not ALL_CAPS (it's not a macro)

class TcpConnection {                 // CamelCase class
public:
    auto sendMessage(std::string_view msg) -> void;
    auto isConnected() const -> bool;

private:
    std::string host_;                 // trailing underscore for members
    int port_;
};

}  // namespace myProject
```

### Anti-Patterns

- `using namespace std;` in a header at global scope (SF.7)
- Headers that depend on inclusion order (SF.10, SF.11)
- Hungarian notation like `strName`, `iCount` (NL.5)
- ALL_CAPS for anything other than macros (NL.9)

## Performance (Per.*)

### Key Rules

| Rule | Summary |
|------|---------|
| **Per.1** | Don't optimize without reason |
| **Per.2** | Don't optimize prematurely |
| **Per.6** | Don't make claims about performance without measurements |
| **Per.7** | Design to enable optimization |
| **Per.10** | Rely on the static type system |
| **Per.11** | Move computation from run time to compile time |
| **Per.19** | Access memory predictably |

### Guidelines

```cpp
// Per.11: Compile-time computation where possible
constexpr auto LookupTable = [] {
    std::array<int, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = i * i;
    }
    return table;
}();

// Per.19: Prefer contiguous data for cache-friendliness
std::vector<Point> points;           // GOOD: contiguous
std::vector<std::unique_ptr<Point>> indirectPoints; // BAD: pointer chasing
```

### Anti-Patterns

- Optimizing without profiling data (Per.1, Per.6)
- Choosing "clever" low-level code over clear abstractions (Per.4, Per.5)
- Ignoring data layout and cache behavior (Per.19)

## Quick Reference Checklist

Before marking C++ work complete:

- [ ] Functions use trailing return types (`auto f() -> T`)
- [ ] Containers accessed via `std::begin`/`std::end`/`std::size`/`std::empty`, not members
- [ ] Names follow the `.clang-tidy` conventions (CamelCase types/namespace-scope functions/constants, camelBack members/locals/params, `_` suffix on private/protected members)
- [ ] No raw `new`/`delete` -- use smart pointers or RAII (R.11)
- [ ] Objects initialized at declaration (ES.20)
- [ ] Variables are `const`/`constexpr` by default (Con.1, ES.25)
- [ ] Member functions are `const` where possible (Con.2)
- [ ] `enum class` instead of plain `enum` (Enum.3)
- [ ] `nullptr` instead of `0`/`NULL` (ES.47)
- [ ] No narrowing conversions (ES.46)
- [ ] No C-style casts (ES.48)
- [ ] Single-argument constructors are `explicit` (C.46)
- [ ] Rule of Zero or Rule of Five applied (C.20, C.21)
- [ ] Base class destructors are public virtual or protected non-virtual (C.35)
- [ ] Templates are constrained with concepts (T.10)
- [ ] No `using namespace` in headers at global scope (SF.7)
- [ ] Headers have include guards and are self-contained (SF.8, SF.11)
- [ ] Locks use RAII (`scoped_lock`/`lock_guard`) (CP.20)
- [ ] Exceptions are custom types, thrown by value, caught by reference (E.14, E.15)
- [ ] `'\n'` instead of `std::endl` (SL.io.50)
- [ ] No magic numbers (ES.45)
