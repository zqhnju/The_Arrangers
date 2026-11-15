#ifndef SCHEDULER_H
#define SCHEDULER_H

/* ---------- 基本常量 ---------- */
#define MAX_TASKS 64
#define MAX_NAME  64

#define DAYS          7
#define SLOTS_PER_DAY 96          /* 8:00–24:00，每 10 分钟一槽 */
#define SLOT_MINUTES  10
#define START_HOUR 8

/* ---------- 数据结构 ---------- */
typedef struct {
    char name[MAX_NAME];
    int duration_slots;        /* 需要占用的槽数（1 槽 = 10 分钟） */
    int deadline_day;    /* 0=周一 ... 6=周日 */
    int deadline_slot;   /* 0~95*/
    int importance;      /* 1~5，5 最重要 */
} Task;

typedef struct {
    int busy;            /* 1=已有课程占用，不可安排任务 */
    int task_index;      /* -1=空闲；否则为被安排任务的索引 */
} Slot;

typedef struct {
    Slot slots[DAYS][SLOTS_PER_DAY];
    Task tasks[MAX_TASKS];
    int task_count;
} Schedule;

/* ---------- 当前已实现的 cost 惩罚项 ---------- */
int penalty_deadline(const Schedule *s);
double   cost(const Schedule *s);
Schedule anneal(Schedule initial, int max_iter,
                double T0, double alpha, double T_min);
                
#endif /* SCHEDULER_H */