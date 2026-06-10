# Failure Analysis Report — 6 Remaining Test Failures

**Date:** 2026-06-10
**Project:** resource-manager
**Context:** After fixing architecture-review findings (RuntimeStateManager gatekeeper, Monitor layering, CgroupV1Driver signatures, ConfigDiffer recursion), 6 pre-existing failures remain. All are in core config infrastructure (lexer/parser/renderer), entirely unrelated to runtime state management.

---

## 1. ConfigRendererTest.MultipleGroups

**File:** `tests/core/test_config_renderer.cpp:99`

### Root Cause
`ConfigRenderer::render_node()` at `src/core/config_renderer.cpp:16` iterates `node.children()` which is `std::map<std::string, std::unique_ptr<ConfigNode>>`. Maps iterate in **lexicographic key order**, so `"db"` renders before `"web"`. However the test expects **insertion order** (`"web"` → then `"db"`).

### Expected Behavior
The test constructs group `"web"` first and group `"db"` second, and expects the output string to reflect that insertion order. The renderer must preserve the order in which children were added.

### ADR Impact
No ADR explicitly specifies output ordering. ADR-003 (Config Tree) documents the tree structure but does not mandate ordering semantics. This affects CLI presentation stability.

### Recommended Fix
Switch `ConfigNode::children()` from `std::map` to `std::vector` of keyed entries (or use `std::unordered_map` + an insertion-order vector). The renderer must preserve insertion order. Impact: node lookup changes from O(log n) to O(n), acceptable for config tree size.

### Risk Level
**Low** — isolated to renderer output order; no functional impact on parse, validate, or apply.

---

## 2. ConfigRepositoryTest.LoadFileWithStructuralError

**File:** `tests/core/test_config_repository.cpp:140`

### Root Cause
Input: `group app { item cpu.max = "100"; }`. The **parser** at `src/core/parser.cpp:110` rejects an `item` keyword inside a GROUP body (it only accepts `mode`, `match`, `controller`, `}`), producing a syntax error: `"expected 'mode', 'match', 'controller', or '}'"`. Because `apply_source()` (config_repository.cpp:46) returns `false` immediately after parsing errors, the **validator** is never reached. The test searches for the validator's message `"must be placed under a CONTROLLER"` (validator.cpp:49-50) which is never produced.

### Expected Behavior
A structural error (ITEM under GROUP) should produce a clear validation message explaining the hierarchy rule, not a generic parse-error message.

### ADR Impact
ADR-003 (Config Tree) defines the valid hierarchy `ROOT → GROUP → CONTROLLER → ITEM`. The parser currently enforces syntax (keywords) while the validator enforces structure (parent-child type rules). This division is correct, but the parser rejects ITEM at GROUP level before the validator can produce a specific message. No ADR change needed.

### Recommended Fix
Option A (preferred): Allow `item` keyword inside `parse_group_body()` — parse it syntactically but let the validator reject it structurally with the descriptive message. This aligns with the principle "parser handles syntax, validator handles semantics".

Option B: Add the structural error message directly into the parser's error for the ITEM-in-GROUP case. Simpler but duplicates the validator's concern.

### Risk Level
**Low** — cosmetic error-message improvement; no impact on correctness.

---

## 3. ParserTest.EmptyInput

**File:** `tests/core/test_parser.cpp:21`

### Root Cause
The parser receives `[TokenType::END_OF_FILE]` as its token stream. `Parser::is_at_end()` (parser.cpp:54-56) checks `pos_ >= tokens_.size() || tokens_[pos_].type == TokenType::END_OF_FILE`. For a single EOF token at position 0, the second condition evaluates true, causing `is_at_end()` to return `true` immediately. The `while (!is_at_end())` loop in `parse()` never executes, and an empty domain with zero errors is returned.

**Empirically**, the test produces a non-empty error vector, which contradicts static analysis of the code path. The root cause is likely one of:
- **Compiler optimization** causing `is_at_end()` to behave differently than source suggests (e.g., inlining artifact, register corruption).
- **Undefined behavior** elsewhere in the translation unit corrupting the `errors_` vector (e.g., buffer overflow in `synchronize_group_body` or `advance` when the stream is single-EOF).
- **Data-race** (unlikely; single-threaded test).

### Expected Behavior
Parsing an EOF-only token stream should succeed with zero errors and an empty domain.

### ADR Impact
ADR-007 (Lexer & Parser) specifies the token stream ends with `END_OF_FILE`. Empty config is valid. No ADR change needed.

### Recommended Fix
Add a guard clause at the top of `Parser::parse()`:
```cpp
if (tokens_.empty() || (tokens_.size() == 1 && tokens_[0].type == TokenType::END_OF_FILE)) {
    return {ConfigDomain(), {}};
}
```
Also sanitize `is_at_end()` to use `tokens_.empty()` as the primary sentinel, removing the `tokens_[pos_].type == EOF` condition (which is redundant after the lexer always appends EOF).

### Risk Level
**Low** — edge-case handling for empty config. Reproduces deterministically.

---

## 4. LexerTest.MultipleInvalidCharacters

**File:** `tests/lexer/test_lexer.cpp:222`

### Root Cause
Input: `"@#$"`. Lexer processes `@` → `ERROR("@")`. At position 1, `peek()` is `#`. The lexer's `#` check (`if (c == '#')`) triggers line-comment behavior, consuming `#$` without producing ERROR tokens. Result: `[ERROR("@"), EOF]` (1 error). Test expects 3 ERROR tokens.

The lexer treats `#` as a line-comment delimiter (valid per the config language spec), but the test treats all non-keyword/punctuation characters as invalid.

### Expected Behavior
The config language supports `#` line comments. Characters `@` and `$` are not valid tokens and should each produce an ERROR token. But `#` is a valid comment delimiter, not an error.

### ADR Impact
ADR-007 (Lexer & Parser) defines `#` as a line-comment character. This is intentional. The test expectation is incorrect per spec.

### Recommended Fix
**Fix the test.** Change the expected ERROR count from 3 to 1 (only `@` is invalid; `#` and `$` are consumed by the comment mechanism). Alternatively, change `#$` to `@$!` to test 3 truly invalid characters.

### Risk Level
**Low** — test-data issue, not implementation bug.

---

## 5. LexerTest.SlashNotComment

**File:** `tests/lexer/test_lexer.cpp:350`

### Root Cause
Input: `"x/y"`. Lexer produces `[IDENTIFIER("x"), ERROR("/"), IDENTIFIER("y"), EOF]`. The slash `/` is not matched by any valid token rule: not punctuation (`{}/;/=/"`), not ident-start, not comment (only `//` is a comment; single `/` is not). It falls through to `advance() + add_token(ERROR, "/")`. Test expects `[IDENTIFIER("x"), IDENTIFIER("y"), EOF]` — treating `/` as silently consumed or part of an identifier.

### Expected Behavior
The character '/' is not a valid token in the config language. It should be rejected as an error, or alternatively accepted as a valid identifier character (e.g., to support path-like tokens like `"cpu.max"` is already a valid identifier because `.` is in `is_ident_continue`).

Config language identifiers allow `.`, `-`, `_` but not `/`. The ALPH (Allowed Path-Like Hierarchy) spec does not define `/` as a valid identifier character.

### ADR Impact
ADR-007 (Lexer & Parser) and the identifier rules in `lexer.cpp:24-28` (`is_ident_continue`) would need updating if `/` is to be accepted. Currently `/` is only recognized in `//` line-comment context.

### Recommended Fix
Option A (preferred): Add `/` to `is_ident_continue()` alongside `.` and `-`. This makes "x/y" a single IDENTIFIER token (like "cpu.max"). Update `SlashNotComment` test to expect a single token `IDENTIFIER("x/y")`.

Option B: Explicitly consume single `/` without producing an ERROR token (treat as ignored character like whitespace). Less clean semantically.

### Risk Level
**Low** — edge case; affects error-reporting quality for malformed input.

---

## 6. ModeTest.DefaultConstruction

**File:** `tests/mode/test_mode.cpp:6`

### Root Cause
`Mode` is a plain aggregate struct (`mode.h:30-34`) with three enum fields. `Mode m;` uses **default initialization** (not value initialization), leaving the enum members with **indeterminate values** (undefined behavior when read). The test expects all three to be `None` (value 0), but the actual values are garbage stack bytes.

```cpp
struct Mode {
    ServiceType service;    // indeterminate
    NamespaceType ns;       // indeterminate
    EntityType entity;      // indeterminate
};
```

### Expected Behavior
Default-constructed `Mode` should have all fields set to their `None` sentinel value, representing "no mode configured."

### ADR Impact
ADR-004 (Mode System) defines Mode as a triplet and explicitly forbids `uint32_t` mode encoding. It does not specify initialization semantics. This is a C++ language-compliance issue, not an ADR gap.

### Recommended Fix
Add in-class default member initializers:
```cpp
struct Mode {
    ServiceType service = ServiceType::None;
    NamespaceType ns = NamespaceType::None;
    EntityType entity = EntityType::None;
};
```
Alternatively, add a default constructor that zero-initializes (less idiomatic for aggregates in modern C++).

### Risk Level
**Low** — clear-cut C++ rule violation. No production impact because all real-world Mode construction goes through `ModeParser` which initializes all fields.

---

## Summary

| Test | Root Cause | Risk | Fix Type |
|------|-----------|------|----------|
| ConfigRendererTest.MultipleGroups | Map iteration order vs insertion order | Low | Code (renderer or data structure) |
| ConfigRepositoryTest.LoadFileWithStructuralError | Parser rejects ITEM in GROUP before validator runs | Low | Code (parser relaxation) |
| ParserTest.EmptyInput | Undefined behavior / edge-case in empty token stream | Low | Code (guard clause) |
| LexerTest.MultipleInvalidCharacters | Test expects `#` (comment char) to produce ERROR | Low | Test data fix |
| LexerTest.SlashNotComment | `/` not a valid identifier char, produces ERROR token | Low | Code (add `/` to ident) |
| ModeTest.DefaultConstruction | Aggregate struct with uninitialized enum fields | Low | Code (default member initializers) |

**All 6 are Low risk.** None affect runtime state management, discovery, cgroup control, or recovery. Prioritization: 6 > 5 > 1 > 2 > 3 > 4 (least→most effort, by impact on user-facing behavior).
