# Codex Update: Vanta Interpreter v0.1

## What Was Built

This repo now contains a working tree-walking interpreter for the Vanta programming language, implemented in C++17.

The interpreter reads a `.vt` source file, tokenizes it, parses it into an AST, and executes the program with a runtime environment that supports:

- Variables
- Functions and closures
- Arrow functions
- Arrays and object literals
- Classes and single inheritance
- `this`-bound instance methods
- `if`, `while`, and `for`
- `break`, `continue`, and `return`
- `try`, `catch`, `finally`, and `throw`
- String interpolation
- Destructuring in `var` declarations
- Optional type hints in parsing
- A small built-in standard library

## Execution Pipeline

The implementation follows a conventional interpreter pipeline:

1. Lexer
   Converts source text into a stream of tokens.

2. Parser
   Uses recursive descent to build an AST for statements and expressions.

3. Evaluator
   Walks the AST and executes code against nested environments.

4. Runtime Value System
   Represents primitive values and heap-backed values such as arrays, objects, functions, classes, and instances.

## Main Files

### Front End

- `src/token.h`
  Defines token types and source locations.

- `src/lexer.h`
- `src/lexer.cpp`
  Tokenization for literals, keywords, operators, comments, and interpolated strings.

- `src/ast.h`
  AST node definitions for expressions and statements.

- `src/parser.h`
- `src/parser.cpp`
  Recursive-descent parser for declarations, statements, expressions, arrow functions, destructuring, optional type hints, and interpolation expressions.

### Runtime

- `src/value.h`
- `src/value.cpp`
  Runtime value model, including primitive values and heap objects.

- `src/environment.h`
- `src/environment.cpp`
  Lexical scope chain for variable lookup and assignment.

- `src/evaluator.h`
- `src/evaluator.cpp`
  Tree-walking evaluator, control-flow handling, function calls, classes, inheritance, method binding, exceptions, and built-in functions.

### Entry Point

- `src/main.cpp`
  CLI entrypoint for reading a file and executing it through the full pipeline.

### Tests and Examples

- `tests/test_lexer.cpp`
- `tests/test_parser.cpp`
- `tests/test_runtime.cpp`
- `tests/test_integration.cpp`

- `examples/hello_world.vt`
- `examples/fibonacci.vt`
- `examples/classes.vt`
- `examples/exceptions.vt`

### Docs

- `README.md`
  Basic usage and build instructions.

- `docs/LANGUAGE_GUIDE.md`
  Short user-facing language guide.

## Supported Language Features

### Variables

```vanta
var answer = 42;
var name: String = "Alice";
```

### Functions and Closures

```vanta
func add(a, b) {
    return a + b;
}
```

Anonymous function expressions are supported via `func(...) { ... }`.

### Arrow Functions

Single-expression arrow functions return implicitly:

```vanta
var double = x => x * 2;
```

Block-bodied arrow functions are also supported:

```vanta
var f = (x, y) => {
    print(x);
    return y;
};
```

### Arrays and Objects

```vanta
var numbers = [1, 2, 3];
var person = {name: "Alice", age: 30};
```

### Destructuring

```vanta
var [a, b] = [10, 20];
var {name, age} = {name: "Alice", age: 30};
```

Destructuring currently applies to `var` declarations.

### Classes and Inheritance

```vanta
class Animal {
    func speak() {
        return "noise";
    }
}

class Dog extends Animal {
    func speak() {
        return "woof";
    }
}
```

Constructors use `__init__`:

```vanta
class Person {
    func __init__(name) {
        this.name = name;
    }
}
```

### Control Flow

Supported:

- `if` / `else`
- `while`
- `for`
- `break`
- `continue`
- `return`

### Exceptions

```vanta
try {
    throw "boom";
} catch (error) {
    print(error);
} finally {
    print("done");
}
```

### String Interpolation

```vanta
var name = "Alice";
print($"Hello {name}");
```

Interpolation expressions inside `{...}` are parsed and evaluated as Vanta expressions.

### Optional Type Hints

Type hints are parsed and stored in the AST, but not enforced at runtime:

```vanta
func add(a: Number, b: Number) -> Number {
    return a + b;
}
```

## Built-In Functions and Methods

### Global Built-Ins

- `print(value...)`
  Writes joined values to stdout and appends a newline.

- `len(value)`
  Works for arrays, strings, and plain objects.

- `type(value)`
  Returns a runtime type label such as `number`, `string`, `array`, `class`, or `instance`.

- `input(prompt?)`
  Writes an optional prompt and reads one line from stdin.

### Array Methods

- `.push(value)`
- `.pop()`
- `.map(fn)`
- `.filter(fn)`
- `.forEach(fn)`
- `.length`

### String Methods

- `.length`
- `.substring(start, end?)`
- `.toUpperCase()`
- `.split(delimiter)`

## Build and Run

### Build

```bash
cmake -S . -B build
cmake --build build
```

### Run a Program

```bash
./build/vanta examples/hello_world.vt
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Verification Performed

The implementation was verified by:

- Building the project with CMake
- Running the Google Test suite successfully
- Executing `example_code.txt`
- Executing `examples/classes.vt`

The `example_code.txt` banking example exercised:

- Class instantiation
- Instance fields
- Method calls
- Arithmetic
- String concatenation
- Exceptions
- `try/catch/finally`

## Implementation Notes

### Closures

Functions capture the environment active at declaration time. Calls execute in a new child environment rooted at the captured closure.

### Method Binding

When a method is accessed on an instance, the evaluator returns a function value bound with `this`.

### Constructors

If a class defines `__init__`, it is invoked automatically during `new ClassName(...)`.

The initializer returns the instance even if it reaches a `return`.

### Exceptions and Control Flow

The evaluator uses internal signal objects for:

- `return`
- `break`
- `continue`
- `throw`

This keeps execution logic straightforward in the tree-walker.

## Current Limitations

This is a solid v0.1 interpreter, but not a fully polished language runtime yet.

Current limitations include:

- Type hints are parsed only; there is no static or runtime type enforcement.
- Destructuring is limited to `var` declarations, not arbitrary assignment targets or function parameter patterns.
- Object literals support explicit `key: value` pairs only.
- There is no `super` keyword.
- There is no module system or imports.
- Error reporting is functional but not yet especially rich or contextual.
- Arrays do not support index syntax like `arr[0]`.
- Objects and instances do not expose a full reflection API.
- The parser and evaluator are intentionally direct and readable rather than optimized.

## Suggested Next Steps

If work continues, the highest-value next steps are:

1. Improve diagnostics with source excerpts and cleaner parser/runtime errors.
2. Add indexing support for arrays and objects.
3. Add `super` and more complete inheritance semantics.
4. Enforce or validate type hints.
5. Add more built-in functions and data structure helpers.
6. Add more negative tests and parser error tests.
7. Consider a REPL mode in `main.cpp`.

## Summary

The repo has moved from a stub to a functional interpreter implementation with:

- A real lexer
- A real parser
- A real runtime
- Integration tests
- Example programs
- Basic documentation

`codex_update.md` is intended to be the practical handoff document for future work on this codebase.
