# mason-cpp: MASON implementation for C++

This is a C++ implementation of [MASON](https://github.com/mortie/mason),
a JSON-like object notation.

## API

The parsing function has this interface:

```cpp
bool Mason::parse(
    std::istream &, Mason::Value &,
    std::string *err = nullptr, int maxDepth = 100);
```

It returns `true` on success, `false` on error.
If an error occurs, the string pointed to by `err`
will be filled with an error message, if it's not null.

## Running tests

To run tests, run `make check`.
This will download the MASON test suite and run it against this implementation.
