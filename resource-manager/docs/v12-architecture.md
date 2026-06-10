# v12 Architecture — Resource Manager

## Overview

当前代码库实现阶段 (截至 2026-06)，覆盖 Issue-010 ~ Issue-027。

```
ConfigLoader    文件读取 → string
     │
     ▼
Lexer           string → Token 流
     │
     ▼
Parser          Token 流 → ConfigDomain Tree
     │
     ▼
Validator       结构校验: 层级/关键字/重复/缺失 value
     │
     ▼
ConfigRepository 编排 load→lex→parse→validate
     │
     ├── ConfigDiffer    old/new tree diff → ADDED / REMOVED / MODIFIED
     └── ConfigRenderer  tree → config text (Hot Reload 用)
```

## Core Data Types

### ConfigNode

```
ConfigNodeType: ROOT | GROUP | CONTROLLER | ITEM

ConfigNode {
    type_
    name_
    value_              // ITEM only
    source_line_
    source_column_
    parent_             // raw ptr, non-owning
    children_           // map<string, unique_ptr<ConfigNode>>
}
```

Children lookup: O(log n) via `std::map`.

### ConfigDomain

```
ConfigDomain {
    root_               // unique_ptr<ConfigNode> (ROOT)
}

  └── ROOT
       └── GROUP "web"
            └── CONTROLLER "cpu"
                 ├── ITEM "cpu.max"     value="100000 100000"
                 └── ITEM "cpu.weight"  value="100"
```

## Data Flow

```
loadFromFile(path)
  → ConfigLoader::loadFromFile     (file → string)
  → Lexer::tokenize                (string → Token[])
  → Parser::parse                  (Token[] → ConfigDomain)
  → Validator::validate            (domain → validation errors)
  → replace internal domain        (on success)

loadFromString(content)
  → apply_source                   (skip ConfigLoader, same pipeline)

replace(domain)
  → full replacement, no merge, error cleared

getRoot()
  → const ConfigDomain& (immutable snapshot)
```

## Validation Rules

| Rule | Logic | Error |
|------|-------|-------|
| 层级 | GROUP under ROOT, CONTROLLER under GROUP, ITEM under CONTROLLER | `"must be placed under..."` |
| 关键字 | node type must match parent type | (same as hierarchy) |
| 重复名称 | no duplicate names at same level | `"duplicate name 'x' at /path"` |
| 缺失 value | ITEM.value() must not be empty | `"ITEM 'x' is missing a value"` |

## Diff (Hot Reload)

```
DiffResult {
    added        // nodes in newTree but not oldTree
    removed      // nodes in oldTree but not newTree
    modified     // ITEMs with changed value (old_value + new_value)
}
```

## Token Types

```
GROUP CONTROLLER ITEM MODE MATCH TYPE
IDENTIFIER STRING
LBRACE RBRACE SEMICOLON EQUALS
ERROR END_OF_FILE
```

## Pipeline Example

```cpp
// load
ConfigRepository repo;
repo.load("/etc/resource-manager/groups.conf");

// read
const auto& domain = repo.getRoot();
const auto* g = domain.root().findChild("web");
const auto* c = g->findChild("cpu");
const auto* i = c->findChild("cpu.max");
// i->value() == "100000 100000"

// diff
auto result = ConfigDiffer::diff(old_domain, new_domain);
// result.added, result.removed, result.modified

// serialize back
std::string text = ConfigRenderer::render(domain);
```

## File Manifest

| File | Lines | Content |
|------|-------|---------|
| `include/resource_manager/core/config_domain.h` | 80 | ConfigNode + ConfigDomain |
| `src/core/config_domain.cpp` | 41 | tree operations |
| `include/resource_manager/lexer/token.h` | 38 | TokenType, Token |
| `include/resource_manager/lexer/lexer.h` | 35 | Lexer class |
| `src/lexer/lexer.cpp` | 145 | Lexer impl |
| `include/resource_manager/core/parser.h` | 55 | ParseError, ParseResult, Parser |
| `src/core/parser.cpp` | 268 | Parser impl |
| `include/resource_manager/core/validator.h` | 34 | ValidationError, Validator |
| `src/core/validator.cpp` | 77 | Validator impl |
| `include/resource_manager/core/config_repository.h` | 40 | ConfigRepository |
| `src/core/config_repository.cpp` | 75 | Repository impl |
| `include/resource_manager/core/config_loader.h` | 16 | ConfigLoader |
| `src/core/config_loader.cpp` | 20 | Loader impl |
| `include/resource_manager/core/config_differ.h` | 39 | DiffType, DiffResult, ConfigDiffer |
| `src/core/config_differ.cpp` | 56 | Differ impl |
| `include/resource_manager/core/config_renderer.h` | 16 | ConfigRenderer |
| `src/core/config_renderer.cpp` | 44 | Renderer impl |
