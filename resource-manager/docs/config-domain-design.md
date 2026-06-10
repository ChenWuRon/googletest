# ConfigDomain Tree — Design

## Overview

ConfigDomain Tree 是 Resource Manager 的配置域树，将配置文件解析后的结构化表示为内存中的多叉树。

Per ADR-003 (updated per ADR-CONFLICT-001 resolution):

```
ROOT
 └── GROUP
      └── CONTROLLER
           └── ITEM
```

## Three Classes

### ConfigPath

Immutable path value type. Paths use bare node name segments:

```
/                          ROOT
/ssm_app                   GROUP("ssm_app")
/ssm_app/cpu               CONTROLLER("cpu") under GROUP("ssm_app")
/ssm_app/cpu/cpu.max       ITEM("cpu.max") under CONTROLLER("cpu")
```

Key methods: `parse()`, `to_string()`, `segments()`, `parent()`, `child()`, `is_root()`, `depth()`.

### ConfigNode

A single tree node. Owns its children via `std::map<std::string, std::unique_ptr<ConfigNode>>`. Holds a raw pointer to its parent (non-owning).

| Method | Returns | Description |
|--------|---------|-------------|
| `addChild(ptr)` | `ConfigNode*` | Adds child; validates type + unique name |
| `removeChild(name)` | `unique_ptr` | Detaches and returns child |
| `findChild(name)` | `ConfigNode*` | Looks up direct child by name |
| `findByPath(path)` | `ConfigNode*` | Traverses multi-segment path |
| `getPath()` | `ConfigPath` | Reconstructs path to this node |
| `parent()` | `ConfigNode*` | Returns parent (nullptr for orphan) |
| `children()` | const ref to map | Range-for iteration |

**Type validation** (enforced in `addChild`):

| Parent | Allowed child |
|--------|---------------|
| ROOT | GROUP |
| GROUP | CONTROLLER |
| CONTROLLER | ITEM |
| ITEM | (none — leaf) |

**Data accessors** (typed per node type):

| Accessor | Available for | Purpose |
|----------|--------------|---------|
| `mode()` | GROUP | ADR-004 Mode triplet |
| `match_rule()` | GROUP | Process match pattern |
| `value()` | ITEM | Resource limit value |
| `value_type()` | ITEM | ValueType enum |

### ConfigDomain

Top-level container. Owns the root `ConfigNode`.

| Method | Description |
|--------|-------------|
| `addGroup(name)` | Creates GROUP under ROOT |
| `addController(group, ctrl)` | Creates CONTROLLER under GROUP |
| `addItem(group, ctrl, item)` | Creates ITEM under CONTROLLER |
| `findByPath(string)` | Parse path + traverse tree |
| `groupCount()` | Number of groups |
| `nodeCount()` | Total nodes including root |

## Ownership

```
ConfigDomain
  └── unique_ptr<ConfigNode> root_
        └── map<string, unique_ptr<ConfigNode>> children_
              ├── "svc1" ── ConfigNode(GROUP) ── parent(raw ptr) ──┐
              │    └── map: {"cpu" ── ConfigNode(CONTROLLER)}      │
              │         └── map: {"cpu.max" ── ConfigNode(ITEM)}   │
              └── "svc2" ── ConfigNode(GROUP) ─────────────────────┘
```

Parent owns children via `unique_ptr`. Child holds raw pointer back to parent. No circular ownership, no manual `delete`.

## Example

```cpp
ConfigDomain domain;

// Build tree
auto* g = domain.addGroup("ssm_app");
g->mode().service = ServiceType::Systemd;
g->match_rule() = MatchRule{"ssm-app", "prefix"};

domain.addController("ssm_app", "cpu");
auto* i = domain.addItem("ssm_app", "cpu", "cpu.max");
i->value() = "100000 100000";
i->value_type() = ValueType::Quota;

// Query by path
auto* found = domain.findByPath("/ssm_app/cpu/cpu.max");
// found->value() == "100000 100000"

// Traverse
for (const auto& [gname, gnode] : domain.root().children()) {
    for (const auto& [cname, cnode] : gnode->children()) {
        for (const auto& [iname, inode] : cnode->children()) {
            // leaf items
        }
    }
}
```

## File Manifest

| File | Purpose |
|------|---------|
| `include/resource_manager/core/config_domain.h` | All class declarations |
| `src/core/config_domain.cpp` | All method implementations |
| `tests/core/test_config_domain.cpp` | Unit tests (~50 test cases) |
