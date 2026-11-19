#include "scheduler.h"
#include <stdlib.h>
#include <math.h>

/* 辅助函数：生成 [0, 1) 随机浮点数 */
static double rand01(void) {
    return (double)rand() / (double)RAND_MAX;
}

/* 辅助函数：检查是否是空闲槽 */
static int slot_is_free(const Slot *slot) {
    return !slot->busy && slot->task_index == -1;
}

/* 辅助函数：在指定位置尝试安排任务，遇到非空闲槽即停止 */
static void try_place(Schedule *s, int t, int day, int start, int len) {
    for (int i = 0; i < len; i++) {
        int h = start + i;
        if (h >= SLOTS_PER_DAY) break;
        if (!slot_is_free(&s->slots[day][h])) break;
        s->slots[day][h].task_index = t;
    }
}

/* 贪心初始解：按截止时间和重要性排序，尽力塞满 */
static Schedule greedy_initial(const Schedule *base) {
    Schedule s = *base;
    int order[MAX_TASKS];
    for (int i = 0; i < s.task_count; i++) order[i] = i;

    /* 插入排序（按 deadline 升序，importance 降序） */
    for (int i = 1; i < s.task_count; i++) {
        int key = order[i];
        int kd = s.tasks[key].deadline_day * SLOTS_PER_DAY + s.tasks[key].deadline_slot;
        int j = i - 1;
        while (j >= 0) {
            int jd = s.tasks[order[j]].deadline_day * SLOTS_PER_DAY + s.tasks[order[j]].deadline_slot;
            if (jd > kd || (jd == kd && s.tasks[order[j]].importance < s.tasks[key].importance)) {
                order[j + 1] = order[j];
                j--;
            } else break;
        }
        order[j + 1] = key;
    }

    /* 依次安排 */
    for (int k = 0; k < s.task_count; k++) {
        int t = order[k];
        int need = s.tasks[t].duration_slots;
        int ddl = s.tasks[t].deadline_day * SLOTS_PER_DAY + s.tasks[t].deadline_slot;

        for (int d = 0; d < DAYS && need > 0; d++) {
            if (d * SLOTS_PER_DAY >= ddl) break; /* 这天整体超时了 */
            for (int h = 0; h < SLOTS_PER_DAY && need > 0; h++) {
                int abs_idx = d * SLOTS_PER_DAY + h;
                if (abs_idx >= ddl) break;
                if (!slot_is_free(&s.slots[d][h])) continue;

                int run = 0;
                while (h + run < SLOTS_PER_DAY && slot_is_free(&s.slots[d][h + run])
                       && abs_idx + run < ddl) {
                    run++;
                }
                int put = run < need ? run : need;
                for (int i = 0; i < put; i++) {
                    s.slots[d][h + i].task_index = t;
                }
                need -= put;
                h += put - 1;
            }
        }
    }
    return s;
}

/* 邻域操作：随机添加、删除、移动 */
static void random_move(Schedule *s) {
    if (s->task_count == 0) return;
    int t = rand() % s->task_count;

    switch (rand() % 3) {
    case 0: { /* 添加一段 */
        // 1. 计算该任务目前已经排了多少槽
        int current_assigned = 0;
        for (int d = 0; d < DAYS; d++)
            for (int h = 0; h < SLOTS_PER_DAY; h++)
                if (s->slots[d][h].task_index == t) current_assigned++;

        int need = s->tasks[t].duration_slots - current_assigned;
        
        // 2. 如果已经排满了，就不再添加
        if (need <= 0) return;

        // 3. 随机寻找一个空闲起点
        int d = rand() % DAYS;
        int h = rand() % SLOTS_PER_DAY;
        if (!slot_is_free(&s->slots[d][h])) return;

        // 4. 计算该起点往后连续空闲的长度，且不能超过缺口 need
        int max_len = 0;
        while (h + max_len < SLOTS_PER_DAY && 
               slot_is_free(&s->slots[d][h + max_len]) && 
               max_len < need) {
            max_len++;
        }
        
        // 5. 在允许范围内随机填入一段
        if (max_len > 0) {
            int put = 1 + rand() % max_len;
            for (int i = 0; i < put; i++) {
                s->slots[d][h + i].task_index = t;
            }
        }
        break;
    }
    case 1: { /* 移除若干槽 */
        int positions[DAYS * SLOTS_PER_DAY][2], n = 0;
        for (int d = 0; d < DAYS; d++)
            for (int h = 0; h < SLOTS_PER_DAY; h++)
                if (s->slots[d][h].task_index == t) {
                    positions[n][0] = d;
                    positions[n][1] = h;
                    n++;
                }
        if (n == 0) return;
        int k = 1 + rand() % 4;
        while (k-- > 0 && n > 0) {
            int idx = rand() % n;
            s->slots[positions[idx][0]][positions[idx][1]].task_index = -1;
            positions[idx][0] = positions[n - 1][0];
            positions[idx][1] = positions[n - 1][1];
            n--;
        }
        break;
    }
    case 2: { /* 移动单个槽 */
        int positions[DAYS * SLOTS_PER_DAY][2], n = 0;
        for (int d = 0; d < DAYS; d++)
            for (int h = 0; h < SLOTS_PER_DAY; h++)
                if (s->slots[d][h].task_index == t) {
                    positions[n][0] = d;
                    positions[n][1] = h;
                    n++;
                }
        if (n == 0) return;
        int pick = rand() % n;
        int d1 = rand() % DAYS, h1 = rand() % SLOTS_PER_DAY;
        if (!slot_is_free(&s->slots[d1][h1])) return;
        s->slots[positions[pick][0]][positions[pick][1]].task_index = -1;
        s->slots[d1][h1].task_index = t;
        break;
    }
    }
}

/* 模拟退火主函数 */
Schedule anneal(Schedule initial, int max_iter, double T0, double alpha, double T_min) {
    /* 1. 获取贪心初始解，避免从白板开始 */
    Schedule cur = greedy_initial(&initial);
    Schedule best = cur;
    double cur_cost = cost(&cur);
    double best_cost = cur_cost;
    double T = T0;

    /* 2. 退火主循环 */
    for (int it = 0; it < max_iter && T > T_min; it++) {
        Schedule next = cur;
        random_move(&next);
        double next_cost = cost(&next);
        double delta = next_cost - cur_cost;

        /* 接受更优解 或 以概率接受更劣解 */
        if (delta < 0 || rand01() < exp(-delta / T)) {
            cur = next;
            cur_cost = next_cost;
            if (cur_cost < best_cost) {
                best = cur;
                best_cost = cur_cost;
            }
        }
        T *= alpha;
    }

    return best;
}