# ADR-006 Cgroup Driver

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 需要操作 `/sys/fs/cgroup` 来创建 cgroup、设置资源限制、Attach 进程。

直接操作 sysfs 会导致：

* 业务代码与内核接口耦合
* v1/v2 差异渗透到上层
* 无法单元测试
* 难以模拟错误场景

因此需要定义一个统一的 Cgroup Driver 接口。

---

# Decision

定义 `ICgroupDriver` 接口，所有 cgroup 操作通过该接口完成。

## 接口定义

```text
interface ICgroupDriver {
    createGroup(path: String): Result
    removeGroup(path: String): Result
    setValue(path: String, file: String, value: String): Result
    getValue(path: String, file: String): Result<String>
    attachTask(path: String, tid: Int): Result
}
```

| 方法 | 说明 |
|------|------|
| createGroup | 创建 cgroup 目录 |
| removeGroup | 删除 cgroup 目录 |
| setValue | 写入控制文件（如 cpu.max） |
| getValue | 读取控制文件 |
| attachTask | 将任务（PID/TID）写入 cgroup.procs |

## 支持版本

| 版本 | 实现类 | 说明 |
|------|--------|------|
| v1 | CgroupV1Driver | 兼容 cgroup v1 接口 |
| v2 | CgroupV2Driver | 原生 cgroup v2 接口（默认） |
| mock | MockCgroupDriver | 用于单元测试 |

## 驱动选择

驱动选择在启动时完成，之后不可切换：

```text
driver = ICgroupDriver::create(cgroup_version);
```

---

# Consequences

采用本决策后：

优点：

* 业务层与内核接口解耦
* v1/v2 切换对上层透明
* Mock 驱动支持完整单元测试
* 接口职责单一

代价：

* 需要实现三套驱动
* 接口抽象需要覆盖所有操作

---

# Related ADR

ADR-009 Controller Model

ADR-010 Attach Engine
