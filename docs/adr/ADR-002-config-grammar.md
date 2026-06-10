# ADR-002 Config Grammar

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 需要一个配置文件来描述资源规则。

配置文件必须回答以下问题：

* 给谁配置 —— 目标进程或线程
* 配置什么 —— CPU、Memory、Cpuset 等资源
* 配置多少 —— 具体的资源配额

如果语法设计不合理，会导致：

* 配置冗余
* 表达力不足
* 解析复杂
* 用户难以理解

因此需要定义一种清晰、可扩展的配置语法。

---

# Decision

配置文件采用三段式结构：**group → controller → item**。

## 关键字

| 关键字 | 用途 |
|--------|------|
| `group` | 定义一个资源组，包含一组进程匹配规则 |
| `controller` | 定义一个资源控制器，如 cpu、memory、cpuset |
| `item` | 定义一个具体的资源限制项，如 cpu.max、memory.max |
| `mode` | 指定匹配模式（service / namespace / entity） |
| `match` | 指定进程匹配规则 |
| `value` | 指定资源配额值 |

## 命名规则

| 元素 | 规则 | 示例 |
|------|------|------|
| group 名称 | 小写字母 + 连字符 | `web-server`, `db-cluster` |
| controller 名称 | 小写字母，对应内核控制器名 | `cpu`, `memory`, `cpuset`, `io`, `pids` |
| item 名称 | 小写字母 + 点号，对应控制文件 | `cpu.max`, `memory.max`, `cpuset.cpus` |

## 嵌套规则

```
group                    # 顶层
  └── controller         # 嵌套在 group 内
       └── item          # 嵌套在 controller 内
```

一个配置文件包含多个 group。
一个 group 包含多个 controller。
一个 controller 包含多个 item。

## 校验规则

| 规则 | 说明 |
|------|------|
| group 名称唯一 | 同一文件中禁止重复 group 名称 |
| controller 类型合法 | 必须是内核支持的 cgroup 控制器 |
| item 名称合法 | 必须是对应 controller 的有效控制文件 |
| value 格式合法 | 必须符合对应控制文件的格式要求 |
| mode 必须指定 | 每个 group 必须指定至少一种 mode |
| match 不能为空 | 至少指定一条匹配规则 |

---

# Grammar

```
group <name> {
    mode <service | namespace | entity>;

    match <pattern> {
        type <exact | prefix | wildcard>;
    }

    controller <type> {
        item <name> = <value>;
        item <name> = <value>;
    }

    controller <type> {
        item <name> = <value>;
    }
}
```

## 示例

```
group web-server {
    mode service;
    match "nginx" {
        type prefix;
    }

    controller cpu {
        item cpu.max = "100000 100000";
        item cpu.weight = "100";
    }

    controller memory {
        item memory.max = "1G";
        item memory.high = "800M";
    }
}
```

---

# Consequences

采用本决策后：

优点：

* 语法表达力强，group → controller → item 层次清晰
* 易于解析，无需复杂的状态机
* 易于扩展，新增 controller 或 item 无需修改语法
* 用户直观，配置结构与 cgroup 文件系统结构对应

代价：

* 需要编写解析器
* 需要提供详细的语法文档

---

# Related ADR

ADR-003 Config Tree
