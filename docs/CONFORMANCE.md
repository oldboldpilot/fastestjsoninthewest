# fastjson conformance: what is accepted, what a number is, how to write JSON

Gate: `tests/conformance/fastjson_conformance_test.cpp` (no external framework). Sensen registers it as
`test_fastjson_conformance_scalar`, `_avx2` and `_best`, one ctest case per string-scan tier (`FASTJSON_SIMD`), and
every tier must give the same answers.

## Parsing is strict (RFC 8259)

`fastjson::parse(text)` returns the value or a `json_error{code, message, line, column}`. It never returns a partial
value. These are errors:

| Input | Code |
|---|---|
| Raw bytes that are not UTF-8 (lone continuation, overlong forms, UTF-16 surrogates written as UTF-8, anything above U+10FFFF, a truncated sequence), in a string or a key | `invalid_unicode` |
| `\uD83D` without a following low surrogate, `\uDE00` alone, two high surrogates | `invalid_unicode` |
| `\u12G4`, `\u12` (bad or missing hex digits) | `invalid_string` (reported just past the first bad digit, as before) |
| A raw control byte (below 0x20, including NUL and newline) inside a string | `invalid_string` |
| `NaN`, `Infinity`, `+1`, `01`, `1.`, `.5`, `1e` | `invalid_syntax` / `invalid_number` |
| A number no representation holds (`1e5000`, an integer past 128 bits) | `invalid_number` |
| Truncation at any byte, trailing garbage, a trailing comma, a missing colon | `unexpected_end` / `invalid_syntax` / `extra_tokens` |
| Nesting deeper than 1000 | `max_depth_exceeded` |

A surrogate pair such as `😀` decodes to ONE code point (4-byte UTF-8). It used to decode to two 3-byte
sequences (CESU-8), which is not UTF-8.

Duplicate object keys: the last value wins.

`ondemand_document::parse` validates its input with the same grammar before it builds its structural index, so the
lazy API refuses exactly what `parse` refuses.

## Numbers

A JSON number is held in the narrowest representation that keeps its value:

- `double`, for anything with at most 17 significant digits inside double range (17 digits identify any double, and
  every shortest round-trip printer emits up to 17, e.g. `0.30000000000000004`);
- a 128-bit float, for more digits or an exponent past double range (`1e999` is a finite value here);
- a 128-bit signed or unsigned integer, for an integer a double would round (`9007199254740993`, `2^64 - 1`).

`is_number()` is true for **every** JSON number, whatever it is held in. Use `is_number_128()`, `is_int_128()` and
`is_uint_128()` only when the representation itself matters.

Reading a number:

| Call | Answers |
|---|---|
| `get_int64()`, `get_uint64()` | the exact integer (from `1`, `1.0`, `1e3` or a 128-bit integer in range), or a `json_error` (`not an integer`, `outside the requested range`, `not a number`). Never throws. Use for anything from outside. |
| `get_double()` | any finite number at its nearest double, or a `json_error` |
| `as_number()`, `as_float64()` | the nearest double (NaN for a non-number, as documented) |
| `as_int64()`, `as_uint64()`, `as_int128()`, `as_uint128()` | truncates a float toward zero; throws `std::out_of_range` for a value outside the type (this was undefined behaviour for floats and a silent wrap for 128-bit integers) |

## Writing

- `json_value::to_string()` writes valid JSON. A NaN or infinite double is written as `null` (the `JSON.stringify`
  rule); a 128-bit float prints the shortest text that reads back to the same value. Object member order is the hash
  map's and is **unspecified**.
- `fastjson::to_json(value)` is the checked form: the same text, or an error for a value with no JSON form (NaN,
  infinity, a string or key that is not UTF-8).
- `fastjson::escape_string(text)` returns the quoted, escaped string `to_string()` would write, or an error for
  non-UTF-8 text.
- `fastjson::writer` writes JSON straight into one string in **call order**:

```cpp
auto text = fastjson::writer{}
                .begin_object()
                .key("model").value(name)
                .key("max_tokens").value(std::int64_t{512})
                .key("stream").value(true)
                .key("messages").begin_array().raw(message.to_string()).end_array()
                .end_object()
                .finish();  // json_result<std::string>
```

The first call that cannot be part of one well-formed value (a value in an object without a key, two keys in a row,
a mismatched or unclosed container, a second top-level value, a NaN or infinite number, non-UTF-8 text, `raw()` text
that is not exactly one JSON value, an empty document) is recorded, later calls are ignored, and `finish()` returns
that error (`invalid_writer_sequence`, `invalid_number` or `invalid_unicode`) instead of text.

`to_string()`, `escape_string` and the writer share one escaper and one double formatter, so there is no second
copy of either.
