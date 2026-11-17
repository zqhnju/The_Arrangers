# The_Arrangers

> 输入两张表（课程表 + 待办任务表），用模拟退火排出一周计划，最后用网页可视化。

```text
courses.txt ─┐
             ├──> [C 程序] ──> schedule.txt  (给人看)
tasks.txt   ─┘                schedule.json (给网页看)
                                    │
                                    v
                              viewer.html
```

---

## 一、三个世界与分工

| 世界 | 内容 | 负责人 |
|---|---|---|
| **数据世界** | `Schedule` 结构体：7 天 × 96 格 + 任务清单 + 课程清单 | A（IO）：文本 ↔ 结构体 |
| **算法世界** | `cost()` 打分、`anneal()` 退火 | C：读写 `Schedule` 内存，反复优化 |
| **展示世界** | JSON + HTML 彩色网格 | B：结构体 → JSON → 网页 |

**通道只有两个**：A 的 `export_schedule_json()` 把结构体写成 JSON；C 的 `cost/anneal` 直接读写结构体内存。

---

## 二、时间槽：全项目统一语言

| 约定 | 值 |
|---|---|
| 一天槽数 | 96（8:00–24:00，每 10 分钟一槽） |
| 一周槽数 | 672 |
| 槽 0 / 槽 95 | 8:00–8:10 / 23:50–24:00 |
| 熬夜区 | 槽 90–95（23:00–24:00） |

**换算公式**：

```text
槽索引 = (小时 - 8) × 6 + 分钟 ÷ 10
小时   = 8 + 槽索引 ÷ 6
分钟   = (槽索引 % 6) × 10
24:00 -> 96（仅用于 deadline / 课程结束边界，不是真实槽）
```

例：`08:00`→0，`09:40`→10，`23:50`→95。

---

## 三、结构体

### 1. 常量（`scheduler.h`）

| 常量 | 值 | 含义 |
|---|---|---|
| `DAYS` | 7 | 一周 7 天 |
| `SLOTS_PER_DAY` | 96 | 一天 96 槽 |
| `SLOT_MINUTES` | 10 | 每槽 10 分钟 |
| `START_HOUR` | 8 | 每天从 8:00 开始 |
| `LATE_NIGHT_START` | 90 | 槽 ≥ 90 算熬夜 |
| `MAX_TASKS` | 64 | 任务数组容量 |
| `MAX_COURSES` | 64 | 课程数组容量 |
| `MAX_NAME` | 64 | 名字最长 63 字符 + `\0` |

**不要写魔法数字**，一律用这些名字。

### 2. `Task`：一个任务

```c
typedef struct {
    char name[MAX_NAME];
    int  duration_slots;   /* 总槽数 = 分钟 / 10 */
    int  deadline_day;     /* 0=周一 … 6=周日 */
    int  deadline_slot;    /* 0~96；96 = 当天 24:00 */
    int  importance;       /* 1~5 */
} Task;
```

| 字段 | 谁写 | 谁读 |
|---|---|---|
| `name` | A | A、B |
| `duration_slots` | A（分钟÷10） | C、A |
| `deadline_day` / `deadline_slot` | A | C、A |
| `importance` | A | C（权重）、B（颜色） |

**三个常见误解**：

- `duration_slots` 是**总时长**，不是最长连续段；可以拆到多天。
- `deadline_slot = 95` 表示 23:50 截止、**槽 95 不可用**；`96` 表示 24:00 截止、槽 95 可用。
- `importance` 不只是显示颜色，它是 `cost` 的权重。

### 3. deadline 的排他边界（重要）

```text
deadline_abs_exclusive = deadline_day * 96 + deadline_slot
abs_index              = day * 96 + slot

abs_index <  deadline_abs_exclusive   /* 截止前完成 */
abs_index >= deadline_abs_exclusive   /* 超时 */
```

| `deadline_slot` | 含义 | 当天可用槽 |
|---:|---|---|
| 0 | 08:00 截止 | 无 |
| 95 | 23:50 截止 | 0~94 |
| 96 | 24:00 截止 | 0~95 |

**规则**：

- 判断写 `<`，不写 `<=`。
- `96` 只出现在 `deadline_slot` 和课程结束时间，**不能写进 `slots[day][96]`**。
- 输入用 `24:00` 表示当天结束，**不要写 `00:00`**。
- 周日 24:00 截止是合法上界：`ddl = 6*96+96 = 672`，整周都算截止前。

### 4. `Course`：一门已有课程

```c
typedef struct {
    char name[MAX_NAME];
    int  day;
    int  start_slot;
    int  end_slot;      /* 边界排他：占用 [start, end) */
} Course;
```

| 字段 | 谁写 | 谁读 |
|---|---|---|
| `name` | A | A、B |
| `day` / `start_slot` / `end_slot` | A | A、B |

**关键约定**：课程占用 `[start_slot, end_slot)`。

- `08:00–09:40` → `start=0`、`end=10`，占槽 0~9，**槽 10 不占**。
- `end_slot` 可以取 96（上到 24:00），但**不能写进 `slots[day][96]`**。

**课程固定，退火不挪课程**。`Course` 和 `course_index` 只在 A 读/导出、B 显示时用。

### 5. `Slot`：一个格子

```c
typedef struct {
    int busy;           /* 1=课程，不可排任务 */
    int course_index;   /* -1=不是课程；>=0=课程下标 */
    int task_index;     /* -1=空闲；>=0=任务下标 */
} Slot;
```

**只有三种合法状态**：

| 状态 | `busy` | `course_index` | `task_index` |
|---|---|---|---|
| 空闲 | 0 | -1 | -1 |
| 课程 | 1 | k（≥0） | -1 |
| 任务 | 0 | -1 | i（≥0） |

课程看 `busy=1` + `course_index>=0`，任务看 `task_index>=0`，三者互斥。

读到 `busy=1, course_index=-1` 或 `busy=0, course_index>=0` 都是 bug。

### 6. `Schedule`：整张画布

```c
typedef struct {
    Slot slots[7][96];
    Task tasks[64];
    int  task_count;
    Course courses[64];
    int  course_count;
} Schedule;
```

- `slots` 管**时间**，`tasks` / `courses` 管**属性**，`task_index` / `course_index` 是两者的连接。
- 遍历任务用 `task_count`，遍历课程用 `course_count`，不能写 `MAX_TASKS` / `MAX_COURSES`。

### 7. 初始化：五个动作缺一不可

| 动作 | 原因 |
|---|---|
| `slots[d][h].busy = 0` | 默认没课 |
| `slots[d][h].course_index = -1` | 默认不是课程 |
| `slots[d][h].task_index = -1` | 默认空闲 |
| `task_count = 0` | 还没读任务 |
| `course_count = 0` | 还没读课程 |

**最大坑**：`memset(&s, 0, sizeof(s))` 会把 `course_index` 和 `task_index` 都变成 **0**，导致“课程 0 / 任务 0 占满全盘”。`busy`、两个 count 可以 `memset`，两个下标必须循环置 `-1`。

---

## 四、数据流（一次完整运行）

| 步 | 谁 | 做什么 |
|---|---|---|
| 1 | B | 运行 `scheduler.exe data/courses.txt data/tasks.txt out/` |
| 2 | B | `main` 初始化空表：`busy=0`、`course_index=-1`、`task_index=-1`、`task_count=0`、`course_count=0` |
| 3 | A | `load_existing_courses()`：`[start, end)` 区间写 `busy=1`、`course_index=k`；课程追加到 `courses[]`；`task_index` 保持 `-1` |
| 4 | A | `load_tasks()`：分钟÷10 → `duration_slots`；`HH:MM` → `deadline_slot`；追加到 `tasks[]`。**此步不摆格子** |
| 5 | C | `cost(&s)` → `c0` |
| 6 | C | `Schedule best = anneal(s, 50000, 100.0, 0.9995, 0.01)`，按值返回新表，原表不变；**只改任务格，课程不动** |
| 7 | C | `cost(&best)` → `c1` |
| 8 | A | `export_schedule_txt(&best, "out/schedule.txt")`；`export_schedule_json(&best, "out/schedule.json", c0, c1)`，同时生成 `web/viewer_data.js` |
| 9 | B | 打开 `web/viewer.html`，加载 `viewer.css` + `viewer_data.js` + `viewer.js`，画 7×96 网格 |

> **为什么生成 `viewer_data.js`？** Windows 本地 `file://` 下 JS 不能 `fetch` JSON（跨域），但能读 `.js`。所以 A 把 JSON 包成 `window.SCHEDULE_DATA = {...};`。

---

## 五、各模块实现要点

### 5.1 IO（A）

- **读课程**：`[start, end)` → `busy=1` + `course_index=k`；课程追加到 `courses[]`。用 `<` 不用 `<=`。
- **读任务**：只填 `tasks[]`，不摆格子。
- **写文本**：按天、按任务两种视角汇总；课程格通过 `course_index` 找课程名。
- **写 JSON**：`courses[]`（带 `name`）和 `tasks[].assigned[]` 两个数组。

**四个易错点**：

1. 边界：`09:40` 是结束时间，不占槽 10。
2. 校验：`09:05` 必须报错，不能取整。
3. 双写：课程格必须同时写 `busy=1` 和 `course_index=k`，只写 `busy` 导出时就没有课程名。
4. 初始化：`course_index` 和 `task_index` 默认必须 `-1`。

### 5.2 算法（C）

**`cost`**：五项惩罚 − 一项奖励，越小越好。

| 项 | 含义 |
|---|---|
| `penalty_deadline` | 截止前未完成 / 超排 |
| `penalty_late_night` | 23:00 后仍有任务 |
| `penalty_rest_fragmentation` | 20 分钟碎休息 |
| `penalty_work_fragmentation` | 工作段内频繁切换 |
| `penalty_work_time` | 连续工作过长/过短 |
| `reward_rest_streak` | 超过 1 小时的连续休息 |

> 当前 `content_id()` 把所有课程都当同一内容（返回 `-2`），**不同课程连续不算切换**。这不影响退火正确性；若要让 `work_fragmentation` 区分课程，改成 `return -1000 - course_index;`（值域别和任务 `0~63` 重叠）。改不改由 C 决定，不影响头文件。

**`anneal`**：随机做小改动 → 算新分 → 变好就接受，变差按温度概率接受 → 降温 → 记录最优。  
允许接受变差是为了跳出局部最优；温度越低越保守。**只改任务格，不碰 `busy` / `course_index` / `courses[]`。**

| 参数 | 变大 | 变小 |
|---|---|---|
| `T0` | 探索广、收敛慢 | 易早熟 |
| `alpha` | 降温慢、更稳 | 可能没搜完 |
| `max_iter` | 更充分、更慢 | 可能没收敛 |
| 权重 | 该项优先避免 | 该项容易被忽略 |

### 5.3 主程序（B）

```text
解析参数 → 初始化空表（五个动作） → load_courses → load_tasks
        → cost(c0) → anneal → cost(c1)
        → export_txt + export_json
```

**核心责任**：顺序正确、每步检查返回值、出错给提示而不是崩。  
**最大坑**：`memset` 清零会让 `course_index` / `task_index` 变 0。

### 5.4 可视化（B）

- HTML：标题、图例、7 列网格、统计区。
- JS：遍历 `courses` 画灰块并显示 `name`；遍历 `tasks[].assigned` 画彩色块（颜色按 `importance`）；填统计数字。

**三个易错点**：

1. JSON 里是槽索引，显示前必须在 JS 里换算成 `HH:MM`。
2. 同一任务可能有多段，画多个同色块，不连线。
3. 课程名直接从 `courses[].name` 取，不要从 `slots` 反查。

---

## 六、协作与接口

### 6.1 两个“宪法级”头文件

- **`scheduler.h`**：结构体 + `cost/anneal` 声明。**只有 C 能改**，改完通知 A、B。
- **`io.h`**：A 的六个函数声明。**C 维护声明，A 实现**。

**任何人加字段/改签名，先沟通，由 C 统一改，全员拉取。**

### 6.2 接口清单

| 调用 | 传入 | 返回 |
|---|---|---|
| `load_existing_courses(path, &s)` | 路径 + `Schedule*` | 0 / -1 |
| `load_tasks(path, &s)` | 路径 + `Schedule*` | 0 / -1 |
| `cost(&s)` | `Schedule*` | double |
| `anneal(initial, max_iter, T0, alpha, T_min)` | 初始表 + 参数 | 新 `Schedule`（按值） |
| `export_schedule_txt(&s, path)` | 表 + 路径 | 0 / -1 |
| `export_schedule_json(&s, path, c0, c1)` | 表 + 路径 + 两个分数 | 0 / -1 |
| `viewer.js` 读 `window.SCHEDULE_DATA` | 全局变量 | JS 对象 |

`anneal` **按值返回**：结构体虽大，但课程设计规模下拷贝开销可忽略，接口最简洁。

### 6.3 Git 流程

```text
main                 <- 稳定版本，只有 C 合并
├── feature/io       <- A
├── feature/main-web <- B
└── feature/anneal   <- C
```

**合并顺序**：

1. C 提交 `scheduler.h` + `io.h` 接口（含 `Course` / `course_index` / `course_count`；**注意 `course_index` 后要写分号**）。
2. A 实现 `io.c`：读课程双写 `busy` + `course_index` 并追加 `courses[]`；导出带课程名。合并。
3. B 写 `main.c` + `Makefile`：初始化五个动作；先注释 `anneal`，只测 IO/导出。合并。
4. C 提交 `cost.c` + `anneal.c`。合并。
5. B 恢复 `anneal` 调用，跑全流程，写 `viewer.*`（课程块显示名字）。
6. C 调参，A 补字段，B 更新演示。

### 6.4 联调四看

1. **课程字段**：`courses[]` 条数 = `courses.txt` 行数；每项有 `name` / `day` / `start` / `end`。
2. **JSON 字段**：`tasks[].assigned` 是否与 `schedule.txt` 一致。
3. **分数**：`c0`、`c1` 是否打印，`c1` 是否明显小于 `c0`。
4. **网页**：课程灰且显示名字、任务彩、23:00 后有红色区、统计数字与 `schedule.txt` 末尾一致。

---

## 七、常见误解

| 误解 | 正确 |
|---|---|
| 任务读进来就该摆格子 | 读任务只填 `tasks[]`，摆格子是退火的事 |
| 课程只写 `busy=1` 就行 | 还要写 `course_index` 并追加 `courses[]`，否则导出没有课程名 |
| 课程和任务都用 `busy=1` | 课程 `busy=1`+`course_index>=0`，任务 `task_index>=0`，互斥 |
| 同一任务的格子一定连续 | 任务可拆到多天，只能靠 `task_index` 识别 |
| 退火会挪课程 | 只挪任务；`busy`/`course_index`/`courses[]` 不变 |
| 网页可以直接 `fetch('schedule.json')` | 本地 `file://` 会被拦截，必须用 `viewer_data.js` |
| `anneal` 会修改传入的表 | 它内部复制，返回新表，原表不变 |
| 调参就是随便改数字 | 每次只改一个参数，记录指标，有对比才有结论 |

---

## 八、一句话总结

**A 把文本变成结构体、把结构体变回文本和 JSON；C 在结构体上反复微调、用加权惩罚打分；B 把结构体串成流水线、把 JSON 画成网页。三人只通过 `scheduler.h`、`io.h`、`data/*.txt`、`out/*.json` 四个契约交接，不越界改别人的文件。**
