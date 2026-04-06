# vanta

A programming language interpreter written in C++.

## Source Files

Vanta programs use the `.vt` file extension.

## Build

```bash
cmake -S . -B build
cmake --build build
```

If GoogleTest is not already installed, CMake will fetch it automatically while configuring tests.

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/vanta examples/hello_world.vt
./build/vanta -e "print(1 + 2);"
./build/vanta --tokens examples/hello_world.vt
./build/vanta --ast examples/hello_world.vt
./build/vanta
```

## Language Notes

### Indexing

Arrays and strings support bracket indexing with the same postfix precedence as calls and member access:

```vt
var items = [10, 20, 30];
print(items[0]);
print(getItems()[1]);
print("vanta"[2]);
```

Array indexed assignment is also supported:

```vt
items[1] = 99;
```

Indexes must be numeric integers and stay within bounds. String indexed assignment is rejected because strings are immutable.

### Standard Helpers

Vanta keeps the built-in surface small. Current helpers include:

- `array.reduce(callback, initial?)`
- `array.slice(start, end?)`
- `array.join(separator?)`
- `string.slice(start, end?)`
- `keys(object)`

Examples:

```vt
print([1, 2, 3].reduce((sum, value) => sum + value, 0));
print([1, 2, 3, 4].slice(1, 3).join("-"));
print("vanta".slice(1, 4));
print(keys({alpha: 1, beta: 2}).join(","));
```

### Type Hints

Vanta now treats supported type hints as small runtime contracts.

The parser and AST preserve type hints on:

- variable declarations: `var answer: Number = 42;`
- function parameters, including arrow functions: `func greet(name: String) { ... }`, `(x: Number) => x * 2`
- named function return types: `func add(a: Number, b: Number) -> Number { ... }`

Runtime enforcement is gradual:

- untyped code continues to work unchanged
- typed variable bindings are checked when they are initialized and when they are assigned later
- typed function parameters are checked at call time
- typed function return values are checked before the function result is returned
- `var name: String;` is allowed without an initializer; later assignments must match `String`

Currently enforced built-in hint names are:

- `Number`
- `String`
- `Boolean`
- `Array`
- `Object`
- `Function`
- `Null`

Any other hint name is still parsed and stored, but it is not enforced yet. This keeps the language gradual while making the built-in hints useful today.

Examples:

```vt
var total: Number = 41;
total = total + 1;

func double(value: Number) -> Number {
    return value * 2;
}

print(double(total));
```

This is rejected at runtime:

```vt
var enabled: Boolean = true;
enabled = "yes";
```

This is still allowed because `Person` is not part of the currently enforced built-in hint set:

```vt
var user: Person = "Alice";
```
