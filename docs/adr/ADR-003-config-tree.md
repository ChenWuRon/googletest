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

每个节点通过路径唯一标识：

```
/groups/<group-name>
/groups/<group-name>/controllers/<controller-type>
/groups/<group-name>/controllers/<controller-type>/items/<item-name>
```

示例：

```
/groups/web-server
/groups/web-server/controllers/cpu
/groups/web-server/controllers/cpu/items/cpu.max
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

# Data Structures

```text
ConfigDomain {
    groups: Map<String, Group>
}

Group {
    name: String
    mode: Mode
    match: MatchRule
    controllers: Map<String, Controller>
}

Controller {
    type: String
    items: Map<String, Item>
}

Item {
    name: String
    value: String
    value_type: ValueType
}
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
