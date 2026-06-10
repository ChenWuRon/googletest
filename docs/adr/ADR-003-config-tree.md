# ADR-003 Config Tree

Status: Accepted

Date: 2026-06-10

Authors: Project Team

Supersedes: ADR-CONFLICT-001

---

# Context

配置文件解析后必须转换为内存中的结构化表示。

这个表示必须支持：

* 高效访问 — 通过路径 O(depth) 定位节点
* 路径导航 — 从任意节点获取完整路径
* 遍历操作 — DFS 用于序列化，BFS 用于层级创建
* Diff 计算 — 支持 Hot Reload 增量更新
* 增量更新 — 最小化 Apply 变更

如果采用扁平结构（Flat Map、JSON DOM、Property Tree），会导致：

* 路径导航困难 — 丢失层次语义，路径需要独立维护
* 层次语义丢失 — 无法表达 group → controller → item 的自然层级
* Diff 计算复杂 — 扁平 Diff 需要上下文推断
* 增量更新难以实现 — 无法精确确定变更影响范围

因此需要定义一棵**配置域树（ConfigDomain Tree）**。

---

# Decision

配置解析后生成一棵多叉树，称为 ConfigDomain Tree。

整个树由 `ConfigDomain` 持有，所有操作通过 `ConfigDomain` 入口进行。

---

## 1. ConfigDomain Responsibilities

`ConfigDomain` 是配置树的顶层容器。

职责：

| 职责 | 说明 |
|------|------|
| 持有根节点 | 通过 `unique_ptr<ConfigNode>` 拥有 ROOT |
| 提供树入口 | `root()` 返回 ROOT 引用 |
| 路径查询 | `findByPath(path_string)` 解析路径并在树中定位 |
| 便捷构建 | `addGroup()`, `addController()`, `addItem()` 基于类型校验快速建树 |
| 节点计数 | `nodeCount()` 用于监控和调试 |
| 生命周期 | 析构时递归销毁整棵树 |

ConfigDomain 不负责：

* 配置文件解析（属于 ConfigParser）
* 资源规则生成（属于 RuleBuilder）
* 运行时状态管理（属于 RuntimeState）

---

## 2. ConfigNodeType

四种节点类型，构成严格的四层结构：

```
ROOT
 └── GROUP
      └── CONTROLLER
           └── ITEM
```

| 类型 | 命名规则 | 子节点类型 | 有效负载 |
|------|----------|------------|----------|
| ROOT | 空字符串 | GROUP | 无 |
| GROUP | 小写字母 + 连字符（如 `ssm-app`） | CONTROLLER | Mode, MatchRule |
| CONTROLLER | 小写字母（如 `cpu`, `memory`） | ITEM | 无 |
| ITEM | 小写字母 + 点号（如 `cpu.max`） | 无（叶子节点） | value, value_type |

类型校验规则（parent → child）：

```
ROOT       → GROUP          （允许）
GROUP      → CONTROLLER     （允许）
CONTROLLER → ITEM           （允许）
ITEM       → (none)         （ITEM 是叶子，禁止有子节点）
ROOT       → CONTROLLER     （禁止）
GROUP      → GROUP          （禁止）
CONTROLLER → CONTROLLER     （禁止）
```

违反类型校验的 `addChild()` 必须静默失败（返回 nullptr）。

---

## 3. Ownership Model

### 3.1 Ownership Rules

1. **父节点拥有子节点**。父节点通过 `std::unique_ptr<ConfigNode>` 持有每个子节点。
2. **子节点通过原始指针反向引用父节点**。`parent_` 是 `ConfigNode*`，不参与所有权。
3. **ConfigDomain 拥有整棵树的根节点**。`ConfigDomain::root_` 是 `unique_ptr<ConfigNode>`。
4. **不允许循环引用**。所有权是单向的：parent → child。
5. **不允许跨树引用**。一个节点只能属于一棵树。

### 3.2 Ownership Diagram

```
ConfigDomain
  │
  └── unique_ptr<ConfigNode>  (ROOT)
         │
         ├── map<string, unique_ptr<ConfigNode>>
         │     │
         │     ├── "group-a" ── ConfigNode(GROUP)
         │     │     │              │
         │     │     │  parent_ ────┘  (raw ptr, non-owning)
         │     │     │
         │     │     └── map: { "cpu" ── ConfigNode(CONTROLLER) }
         │     │                        │
         │     │                        └── map: { "cpu.max" ── ConfigNode(ITEM) }
         │     │
         │     └── "group-b" ── ConfigNode(GROUP)
         │
         └── node_count_  (size_t, cached total)
```

### 3.3 Lifespan

- ConfigDomain 析构 → ROOT 析构 → 递归销毁所有子节点（unique_ptr 自动释放）
- removeChild() 将子节点所有权转移给调用方（返回 unique_ptr）
- 被移除的节点 parent_ 置为 nullptr（孤儿化）

---

## 4. Canonical Path Format

### 4.1 Format

路径由节点名称按层级拼接而成，**不含类型字面量段**。

```
/<group-name>
/<group-name>/<controller-name>
/<group-name>/<controller-name>/<item-name>
```

示例：

| 节点 | 路径 |
|------|------|
| ROOT | `/` |
| GROUP "ssm_app" | `/ssm_app` |
| CONTROLLER "cpu" under GROUP "ssm_app" | `/ssm_app/cpu` |
| ITEM "cpu.max" under CONTROLLER "cpu" | `/ssm_app/cpu/cpu.max` |

### 4.2 废除的旧格式

以下格式被明确废除（ADR-CONFLICT-001 决议）：

```
/groups/<group-name>/controllers/<controller>/items/<item>
```

理由：`ConfigNodeType` 已经提供类型信息。在路径中嵌入 `groups`、`controllers`、`items` 等字面量段是重复元数据，增加了解析器复杂性。

### 4.3 根节点

根节点路径固定为 `/`，`depth() == 0`，`is_root() == true`。

---

## 5. Path Resolution Rules

### 5.1 Absolute Path Only

所有路径必须以 `/` 开头。不允许相对路径。

| 输入 | 结果 |
|------|------|
| `/ssm_app/cpu` | 合法，从根开始查找 |
| `ssm_app/cpu` | 非法，parse 返回 nullopt |
| `//ssm_app` | 非法，空段，parse 返回 nullopt |
| `/ssm_app/` | 非法，尾随斜杠，parse 返回 nullopt |
| `` (空串) | 非法，parse 返回 nullopt |

### 5.2 Deterministic Lookup

路径查找必须确定：**相同路径输入必须始终返回相同节点**（前提是树未被修改）。

保证方式：

* 子节点存储在 `std::map<std::string, unique_ptr<ConfigNode>>`（有序映射）
* 查找通过 `find()` 进行 O(log n) 的键比较
* 路径段依次在每一层查找，任一缺失立即返回 nullptr

### 5.3 No Duplicate Siblings

同一父节点下禁止重复子节点名称。

| 操作 | 结果 |
|------|------|
| addChild("cpu") 到 GROUP | 成功 |
| addChild("cpu") 到同一 GROUP | 失败，返回 nullptr |
| addChild("cpu") 到不同 GROUP | 成功，位于不同父节点 |

### 5.4 Path Parsing

`ConfigPath::parse()` 负责将字符串路径解析为段向量。

```text
"/ssm_app/cpu/cpu.max"  →  ["ssm_app", "cpu", "cpu.max"]
"/"                      →  []                     (root)
```

根路径的特殊处理：

* 字符串 `"/"` 解析为 `ConfigPath::root()`，深度为 0
* 空段（连续斜杠 `//`）被视为非法输入

### 5.5 Path Construction

`ConfigNode::getPath()` 从节点向上遍历父链，收集名称，反转后构造 `ConfigPath`。

```text
ITEM("cpu.max") under CONTROLLER("cpu") under GROUP("ssm_app")
  → 从 ITEM 向上收集: ["cpu.max", "cpu", "ssm_app"]
  → 反转: ["ssm_app", "cpu", "cpu.max"]
  → ConfigPath → "/ssm_app/cpu/cpu.max"
```

---

## 6. Tree Traversal Model

### 6.1 Depth-First Search (DFS)

DFS 用于序列化和全量 Apply。

顺序：GROUP → CONTROLLER → ITEM（先父后子，预序遍历）。

用途：

* 序列化为配置文件
* 全量 Apply 资源规则
* 深度拷贝

### 6.2 Breadth-First Search (BFS)

BFS 用于层级创建。

顺序：ROOT → 所有 GROUP → 所有 CONTROLLER → 所有 ITEM。

用途：

* 创建 cgroup 层级（必须先创建父目录）
* 按层校验依赖关系

### 6.3 迭代器支持

子节点迭代通过 `children()` 暴露 `const map&`，支持 range-based for：

```cpp
for (const auto& [name, node] : parent.children()) {
    // name: string, node: unique_ptr<ConfigNode>&
}
```

### 6.4 条件查询

遍历时支持按 ConfigNodeType 过滤：

```cpp
// 仅遍历 GROUP 节点
for (const auto& [name, node] : root.children()) {
    if (node->type() == ConfigNodeType::GROUP) { ... }
}
```

---

## 7. Mutation Rules

### 7.1 addChild

```text
signature: ConfigNode* addChild(unique_ptr<ConfigNode> child)
```

语义：

1. 校验 child 非空
2. 校验 child type 与当前节点 type 的父子关系（`is_valid_parent_child()`）
3. 校验 child name 在当前 children 中唯一
4. 设置 child.parent_ = this
5. 将 child 移入 children_ map
6. 返回 child 原始指针，调用方不拥有所有权

失败场景均返回 nullptr。

### 7.2 removeChild

```text
signature: unique_ptr<ConfigNode> removeChild(const string& name)
```

语义：

1. 在 children_ 中查找 name
2. 如果找到，从 map 中移除，parent_ 置 nullptr，将所有权返回给调用方
3. 如果未找到，返回 nullptr（不含 nullptr_t 值）

调用方获得子节点的所有权，可以销毁、暂存或移动到其他位置。

### 7.3 replaceChild（预留）

```text
signature: unique_ptr<ConfigNode> replaceChild(const string& name, unique_ptr<ConfigNode> new_child)
```

当前为预留 API，尚未实现。

语义（设计目标）：

1. 校验 new_child 非空
2. 校验 new_child type 与当前节点 type 的父子关系
3. 校验 new_child name 等于被替换的 name（或接受名称变更）
4. 从 map 中取出旧节点
5. 插入新节点，设置 parent_
6. 将旧节点所有权返回给调用方

用途：Hot Reload 中树形结构不变但节点内容变更。

---

## 8. Hot Reload Requirements

### 8.1 Diff Operations

两个 ConfigDomain Tree 之间计算 Diff。

Diff 类型：

| 类型 | 含义 | 触发操作 |
|------|------|----------|
| NodeAdded | 新树比旧树多出节点 | createGroup / setValue |
| NodeRemoved | 旧树存在但新树已删除 | removeGroup / 回滚 setValue |
| NodeModified | 节点存在但值（或子节点）变更 | setValue 更新 |
| NodeUnchanged | 无变化 | 跳过 |

### 8.2 Node Identity

节点标识由 **路径** 决定。

```text
NodeID = ConfigPath  (= /<group>/<controller>/<item>)
```

比较规则：

| 场景 | 视为同一节点？ |
|------|---------------|
| 相同路径，相同 type | 是 |
| 相同路径，不同 type | 冲突（非法变更，必须拒绝） |
| 不同路径，相同 name | 否 |

路径用作节点标识，因此禁止路径变更的原地修改。如果 group 需要重命名，必须 removeChild + addChild。

### 8.3 Diff Algorithm Requirements

Diff 算法的输入：

* 旧树 `ConfigDomain`（当前生效的配置）
* 新树 `ConfigDomain`（最新解析的配置）

Diff 算法的输出：

* `DiffResult`：有序的 `DiffNode` 列表

DiffNode 结构：

```text
DiffNode {
    path:      ConfigPath     // 发生变更的节点路径
    type:      DiffType       // Added / Removed / Modified / Unchanged
    old_value: optional<T>    // 旧值（Modified/Removed 时有值）
    new_value: optional<T>    // 新值（Added/Modified 时有值）
}
```

### 8.4 Incremental Apply

Apply 顺序必须遵循拓扑约束：

1. 先 Apply NodeAdded（自根向下：先 GROUP，再 CONTROLLER，再 ITEM）
2. 再 Apply NodeModified（任意顺序）
3. 最后 Apply NodeRemoved（自叶向上：先 ITEM，再 CONTROLLER，再 GROUP）

---

## 9. Serialization Requirements

### 9.1 Export

ConfigDomain Tree 可以导出为可读格式。

导出规则：

1. DFS 遍历
2. 每个节点输出一行，缩进表示层级
3. GROUP 节点输出 mode 和 match 信息
4. CONTROLLER 节点输出类型
5. ITEM 节点输出 name = value

示例输出：

```
/                         ROOT
  /ssm_app                GROUP mode=service match=ssm-app/prefix
    /ssm_app/cpu          CONTROLLER
      /ssm_app/cpu/cpu.max   ITEM cpu.max = 100000 100000
      /ssm_app/cpu/cpu.weight ITEM cpu.weight = 100
    /ssm_app/memory       CONTROLLER
      /ssm_app/memory/memory.max ITEM memory.max = 1G
```

### 9.2 Import

ConfigDomain Tree 可以从序列化格式重建。

导入规则：

1. 逐行读取
2. 解析缩进确定层级深度
3. 根据类型标签和路径创建节点
4. 完整的类型校验和唯一性校验在添加时执行

### 9.3 格式约定

| 元素 | 格式 |
|------|------|
| 缩进 | 两个空格 |
| 路径分隔 | `/` |
| GROUP 行 | `/<name>` + 类型标签 `GROUP` + 元数据 |
| CONTROLLER 行 | `/<name>/<ctrl>` + 类型标签 `CONTROLLER` |
| ITEM 行 | `/<name>/<ctrl>/<item>` + 类型标签 `ITEM` + `key = value` |
| 注释 | `#` 开头 |

---

## 10. Rejected Alternatives

### 10.1 Flat Map

以路径为键的扁平映射：

```cpp
std::map<std::string, NodeValue> flat_tree;
```

**被拒绝**，因为：

* 无法表达父子遍历关系
* 路径和层级语义分离，容易不一致
* 移动子树需要重写所有子路径
* Diff 需要字符串前缀匹配，效率低
* 无法做类型校验（任何路径可以映射到任何值）

### 10.2 JSON DOM

使用 JSON 对象/数组模拟树：

```json
{
    "groups": [
        {
            "name": "ssm_app",
            "controllers": [
                { "type": "cpu", "items": { "cpu.max": "100000 100000" } }
            ]
        }
    ]
}
```

**被拒绝**，因为：

* JSON 是通用格式，缺乏类型安全
* 节点无法直接嵌入指针引用（父/子导航）
* 路径查找需要递归遍历对象
* 需要额外层做校验和默认值处理
* 不适合需要高性能路径查找的场景

### 10.3 Property Tree

使用 Boost Property Tree 或类似 key-value 层级结构：

```
ssm_app.cpu.cpu.max = 100000 100000
```

**被拒绝**，因为：

* 点分隔路径和节点名称冲突（ITEM 名称包含点号，如 `cpu.max`）
* 无法区分路径段和节点属性
* 缺少类型系统（所有值都是字符串）
* 不支持 Mode、MatchRule 等复杂类型
* 无法约束层级深度和父子类型关系
* Diff 只能基于字符串比较，丢失语义信息

---

# Consequences

采用本决策后：

优点：

* 树结构与配置语法（group → controller → item）天然对应
* 路径访问模型直观、O(depth) 查找
* 确定性路径解析，便于调试和缓存
* Diff 模型支持精确的增量 Apply
* 类型校验防止非法层级（如 CONTROLLER 下嵌套 CONTROLLER）
* unique_ptr 所有权无手动内存管理，无循环引用风险
* 序列化格式透明，可读可调试

代价：

* 需要实现树结构和迭代器
* 需要实现 Diff 算法（Phase 4）
* 需要实现 DFS/BFS 遍历（预留接口）
* `replaceChild` 尚未实现，影响部分 Hot Reload 场景

---

# Related ADR

ADR-002 Config Grammar — 语法定义 group → controller → item
ADR-004 Mode System — GROUP 节点负载中的 Mode
ADR-005 Runtime State — ConfigState 使用 ConfigDomain Tree
ADR-011 Hot Reload — 依赖 Diff 模型做增量 Apply
