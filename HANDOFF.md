# The Arrangers · B 工作交接

## 当前状态

- 项目仓库为 [zqhnju/The_Arrangers](https://github.com/zqhnju/The_Arrangers)，当前工作分支为 `feature/main-web`。用户已推送 B 的实现，核对时本地 `HEAD` 和 `origin/feature/main-web` 一致，均指向 `0d9f57f`（实现排程主程序和结果展示）。
- 当前检出的分支没有 A 的 `io.c`、真实课程和待办输入，也没有运行生成的 `web/viewer_data.js`。真实数据联调待完成。PR 是否已创建尚未确认。
- 用户明确回到原 README 的分工范围。B 做 C 程序入口、构建、结果展示和演示。本阶段先运行命令行程序，再打开网页加载结果。浏览器录入、运行排程、下载按钮和逐任务剩余时长属于讨论过的扩展，从未写入代码。讨论中的「剩余 50 分钟」是用假设需求 480 分钟减去示例已排 430 分钟的例子。

## 已交付的文件

| 文件 | 实现与关键约定 |
| --- | --- |
| [`main.c`](main.c) | 接收课程文件、待办文件、输出目录三个命令行参数。初始化 `Schedule` 后，按顺序调用 A 的读课程和读任务函数、C 的 `cost`、`anneal` 和 `cost`，再调用 A 的 TXT、JSON 导出函数。打印优化前后评分。读入、目录或导出失败会报错并返回非零。已有输出目录可以复用，创建新目录时父目录需已存在。 |
| [`Makefile`](Makefile) | 用 GCC 将 `main.c`、`io.c`、`cost.c`、`anneal.c` 编译链接成可执行文件，链接 `-lm`。Windows 下目标为 `scheduler.exe`，并将两个共享头文件列为构建依赖。 |
| [`web/viewer.html`](web/viewer.html)、[`viewer.css`](web/viewer.css) | 本地结果页。显示统计、图例、时间轴和 7 天 × 96 槽的网格。课程灰色，待办按重要度着色，23:00 后为红色斜纹。当前作者信息使用团队名。 |
| [`web/viewer.js`](web/viewer.js) | 读取 `window.SCHEDULE_DATA`，用 `day` 选列、用 `start + 1 / end + 1` 定位 CSS 网格线。一个任务的每个 `assigned` 区间各画一块，颜色按同一个 `importance`。将槽索引换算为时刻并计算已排任务时长和熬夜任务槽数。 |
| [`web/viewer_demo_data.js`](web/viewer_demo_data.js) | 无真实导出时，用户可点「查看示例排程」。示例包含跨天拆分、单槽任务、跨 23:00 和 24:00 结束边界。示例数据与 A 生成的实际数据使用不同的全局变量。 |
| [`B_答辩讲解.md`](B_答辩讲解.md) | 逐个讲解 C、Makefile、HTML、CSS、JavaScript，附演示流程、口述参考和可手算的练习。 |

## A、B、C 的交接点

- **A 的实现**见 [`io.h`](io.h) 的接口。A 的 `io.c` 读两份文本、导出 `schedule.txt` 和 `schedule.json`，并按 README 导出 `web/viewer_data.js`，内容形如 `window.SCHEDULE_DATA = {...};`。主程序负责调用这些接口。联调从仓库根目录运行，便于生成的相对路径 `web/` 指向页面目录。
- **C 的实现**在 [`cost.c`](cost.c) 和 [`anneal.c`](anneal.c)。`main.c` 先对读入后尚未放置待办的表评分，再将原表按值传给 `anneal`，拿回 `best` 并重新评分。两个共享头文件的维护归 C，接口变更按原 README 协作。
- **网页数据**使用 `courses[]` 的 `name/day/start/end`，以及 `tasks[]` 的 `name/importance/assigned[]`，每个分段带 `day/start/end`。星期从周一 `0` 开始，每天从 08:00 起每槽 10 分钟，区间为 `[start, end)`，`end: 96` 表示 24:00。页面目前从顶层读取 `init_cost` 和 `final_cost`，这两个 JSON 键名须用 A 的真实导出来确认。
- **统计口径**。页面的「已排任务时长」是各任务的有效分段槽数之和乘以 10。「熬夜任务槽数」是任务实际占用槽与 `[90, 96)` 的交集之和。C 的熬夜惩罚按 23:00 到当天最后一个任务槽的跨度计分。联调时确认 A 的文本报告采用哪一种口径，再让同名指标对齐。

## 已验证与复现

1. **网页可单独演示。** 双击 `web/viewer.html`，点击「查看示例排程」。示例应显示 **430 分钟、6 个熬夜任务槽**，周一高等数学为 08:00–09:40，分段的实验报告在周一、周三各出现一次。示例与模拟导出数据都通过了 Edge 本地文件测试，另外核对了 24:00 结束边界、零分显示、无效时间段跳过和课程名作为纯文本显示。
2. **主程序和构建经过替身验证。** 用临时 `io.c` 替身与真实 `cost.c`、`anneal.c` 构建并运行，检查了初始化、带空格和尾随反斜杠的输出目录、生成两个结果文件、缺参数和读入失败的退出行为。替身与浏览器检查脚本用于临时验证。实际 A 导出端的格式和值仍需联调。

## 接手后的下一步

1. 执行 `git fetch origin`，用 `git ls-tree -r --name-only origin/main -- io.c data` 查看 A 的代码和输入是否已经出现。用户在本机配置了 Clash 代理，常规 Git 命令会使用当前代理设置。
2. A 的实现合入后，在 `feature/main-web` 合并最新 `origin/main`。在 Windows PowerShell 用下面的命令构建和运行，按实际输入文件位置调整路径。

   ```powershell
   mingw32-make
   .\scheduler.exe .\data\courses.txt .\data\tasks.txt .\out
   ```

3. 按 [README 联调要求](README.md#64-联调时的四个必须一起看)，用一份真实课程表和 5 个任务运行。核对终端的初始与最终评分、`viewer_data.js` 的评分键和区间、`schedule.txt` 的时段和统计，再打开页面确认展示一致。确认后更新答辩演示与 PR 状态。
4. 用户要能讲清楚自己分工里的每段代码。已经练习过把 `day: 4, start: 89, end: 93` 换算成周五 22:50–23:30。下一题可以让用户解释为何 CSS 网格线是 `90 / 94`，然后回到 `main.c` 讲空表初始化、指针参数和错误返回。
