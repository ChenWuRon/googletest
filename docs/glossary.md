# Glossary

| 术语 | 英文 | 定义 |
|------|------|------|
| Resource Manager | Resource Manager | 基于 Linux cgroup 的资源编排框架 |
| Group | Group | 一组资源规则的集合，包含进程匹配规则和控制器配置 |
| Controller | Controller | cgroup 控制器，如 cpu、memory、cpuset |
| Item | Item | 控制器下的具体资源限制项，如 cpu.max |
| Mode | Mode | 用于绑定目标的三元组（service, namespace, entity） |
| ConfigState | Config State | 配置解析后的期望状态（ConfigDomain Tree） |
| RuntimeState | Runtime State | 运行时实际状态（PID、TID、Attach 状态等） |
| ConfigDomain Tree | ConfigDomain Tree | 配置解析后生成的多叉树结构 |
| Cgroup Driver | Cgroup Driver | 封装 cgroup 文件系统操作的接口 |
| Process Discovery | Process Discovery | 扫描 /proc 发现目标进程的能力 |
| Attach Engine | Attach Engine | 负责将进程绑定到 cgroup 的编排组件 |
| Monitor | Monitor | 检测 PID 变化并触发恢复的组件 |
| ControlFileType | Control File Type | 控制文件的统一抽象模型 |
| Recovery | Recovery | 进程重启后自动恢复资源绑定 |
| Hot Reload | Hot Reload | 不重启进程的情况下更新配置 |
| Attach | Attach | 将进程绑定到 cgroup 的操作 |
| Detach | Detach | 将进程从 cgroup 分离的操作 |
| Reattach | Reattach | 进程重启后重新绑定到 cgroup |
| ErrorCode | Error Code | 统一错误码枚举 |
| PID | Process ID | 进程标识符 |
| TID | Thread ID | 线程标识符 |
| cgroup v1 | cgroup v1 | 第一代 cgroup 接口 |
| cgroup v2 | cgroup v2 | 第二代 cgroup 接口（默认） |
| sysfs | sysfs | Linux 虚拟文件系统，cgroup 控制文件所在路径 |
| /proc | /proc | Linux 进程信息虚拟文件系统 |
