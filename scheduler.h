#ifndef SCHEDULER_H
#define SCHEDULER_H

/* ---------- 基本常量 ---------- */
#define MAX_TASKS 64
#define MAX_NAME  64
#define MAX_COURSES 64

#define DAYS          7
#define SLOTS_PER_DAY 96          /* 8:00–24:00，每 10 分钟一槽 */
#define SLOT_MINUTES  10
#define START_HOUR 8
#define LATE_NIGHT_START 90

/* ---------- 数据结构 ---------- */
typedef struct {
    char name[MAX_NAME];
    int duration_slots;        /* 需要占用的槽数（1 槽 = 10 分钟） */
    int deadline_day;    /* 0=周一 ... 6=周日 */
    int deadline_slot;   /* 0~96；截止时刻所在槽；96=当天24:00 */
    int importance;      /* 1~5，5 最重要 */
} Task;

typedef struct {
    char name[MAX_NAME];
    int  day;
    int  start_slot;
    int  end_slot;
} Course;

typedef struct {
    int busy;            // 1=已有课程占用，不可安排任务 
    int course_index;     // -1=不是课程；>=0=课程索引
    int task_index;      // -1=空闲；否则为被安排任务的索引 
} Slot;

typedef struct {
    Slot slots[DAYS][SLOTS_PER_DAY];
    Task tasks[MAX_TASKS];
    int task_count;
    Course courses[MAX_COURSES];
    int  course_count;
} Schedule;

/* ---------- 当前已实现的 cost 惩罚项 ---------- */
double penalty_deadline(const Schedule *s);
double penalty_late_night(const Schedule *s);
double penalty_rest_fragmentation(const Schedule *s);
double penalty_work_fragmentation(const Schedule *s);
double penalty_work_time(const Schedule *s);
double reward_rest_streak(const Schedule *s);

double   cost(const Schedule *s);
Schedule anneal(Schedule initial, int max_iter,
                double T0, double alpha, double T_min);
                
#endif /* SCHEDULER_H */
