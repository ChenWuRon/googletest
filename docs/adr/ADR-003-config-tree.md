# ADR-003 Config Tree

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

配置文件解析后必须转换为内存中的结构化表示。

这个表示必须支持：

* 高效访问
* 路径导航
* 遍历操作
* Diff 计算
* 增量更新

如果采用扁平结构，会导致：

* 路径导航困难
* 层次语义丢失
* Diff 计算复杂
* 增量更新难以实现

因此需要定义一棵**配置域树（ConfigDomain Tree）**。

---

# Decision

配置解析后生成一棵多叉树，称为 ConfigDomain Tree。

## 树结构

```
ROOT
 └── GROUP
      └── CONTROLLER
           └── ITEM
```

每个节点包含：

| 节点类型 | 内容 |
|----------|------|
| ROOT | 树根，包含所有 group |
| GROUP | group 定义，包含 match 规则、mode、controller 列表 |
| CONTROLLER | 控制器定义，包含 item 列表 |
| ITEM | 具体资源限制项，包含名称、值、类型信息 |

## 路径访问模型

每个节点通过路径唯一标识。

路径由节点名称按层级拼接而成，不含类型字面量段。

```
/<group-name>
/<group-name>/<controller-name>
/<group-name>/<controller-name>/<item-name>
```

示例：

```
/web-server
/web-server/cpu
/web-server/cpu/cpu.max
```

根节点路径为 `/`。

`ConfigNodeType` 已经提供类型信息，路径中不再需要 `groups`、`controllers`、`items` 等字面量段。

删除的旧格式（已废弃）：

```
/groups/<group-name>/controllers/<controller>/items/<item>
```

## 遍历模型

支持以下遍历方式：

| 方式 | 说明 |
|------|------|
| 深度优先 | 用于序列化、全量 Apply |
| 广度优先 | 用于层级创建（先创建 group，再创建 controller） |
| 路径查询 | 根据路径定位节点 |
| 条件查询 | 根据 mode、controller 类型等过滤 |

## Diff 模型

两个 ConfigDomain Tree 之间计算 Diff：

| Diff 类型 | 含义 |
|-----------|------|
| NodeAdded | 新增节点 |
| NodeRemoved | 删除节点 |
| NodeModified | 节点值变更 |
| NodeUnchanged | 无变化 |

Diff 结果用于增量 Apply。

---

# Implementation: Tree Model

ConfigDomain Tree 使用多叉树实现，节点通过 `ConfigNode` 类表示。

## ConfigNode

每个 `ConfigNode` 包含：

| 字段 | 说明 |
|------|------|
| type | ConfigNodeType (ROOT/GROUP/CONTROLLER/ITEM) |
| name | 节点名称 |
| parent | 父节点指针（原始指针，不拥有） |
| children | 子节点映射 (map<string, unique_ptr<ConfigNode>>) |
| value | 仅 ITEM 节点：资源限制值 |
| value_type | 仅 ITEM 节点：值类型 (ValueType) |
| mode | 仅 GROUP 节点：Mode 三元组 |
| match_rule | 仅 GROUP 节点：进程匹配规则 |

## 所有权

- 父节点通过 `unique_ptr` 拥有子节点
- 子节点通过原始指针引用父节点
- ConfigDomain 拥有根节点

## ConfigPath

路径通过 node name 拼接，不含类型字面量段。

```
/                     ROOT
/ssm_app              GROUP("ssm_app")
/ssm_app/cpu          CONTROLLER("cpu") under GROUP("ssm_app")
/ssm_app/cpu/cpu.max  ITEM("cpu.max") under CONTROLLER("cpu")
```

---

# Consequences

采用本决策后：

优点：

* 树结构与配置语法天然对应
* 路径访问模型直观、高效
* Diff 模型支持增量更新
* 遍历模型灵活

代价：

* 需要实现树结构
* 需要实现 Diff 算法

---

# Related ADR

ADR-002 Config Grammar

ADR-011 Hot Reload
