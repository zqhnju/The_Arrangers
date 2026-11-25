window.SCHEDULE_DEMO_DATA = {
    init_cost: 808,
    final_cost: -458.1,
    courses: [
        { name: "高等数学", day: 0, start: 0, end: 10 },
        { name: "计算机基础", day: 1, start: 12, end: 21 },
        { name: "线性代数", day: 2, start: 6, end: 15 },
        { name: "大学英语", day: 3, start: 24, end: 33 },
        { name: "程序设计", day: 4, start: 12, end: 24 },
    ],
    tasks: [
        { name: "完成实验报告", importance: 5, assigned: [
            { day: 0, start: 18, end: 27 },
            { day: 2, start: 21, end: 27 },
        ] },
        { name: "阅读课件", importance: 2, assigned: [{ day: 1, start: 30, end: 36 }] },
        { name: "整理课堂笔记", importance: 3, assigned: [{ day: 3, start: 38, end: 42 }] },
        { name: "准备小组汇报", importance: 4, assigned: [
            { day: 4, start: 38, end: 47 },
            { day: 6, start: 88, end: 96 },
        ] },
        { name: "确认下周安排", importance: 1, assigned: [{ day: 5, start: 12, end: 13 }] },
    ],
};
