#include "scheduler.h"
#include <stddef.h>

#define DEADLINE_MISS_PENALTY 20
#define LATE_NIGHT_PENALTY_PER_SLOT 2
#define REST_FRAGMENTATION_PENALTY 3
#define WORK_FRAGMENTATION_PENALTY 5
#define OVERWORK_PENALTY 5
#define REST_STREAK_REWARD 5

double penalty_deadline(const Schedule *s) {//错过deadline惩罚，错过越多，任务越重要，惩罚越重
    if (s == NULL) {
        return 0;
    }

    double total = 0;

    for (int i = 0; i < s->task_count; i++) {
        int duration = s->tasks[i].duration_slots;
        if(duration <= 0){
            continue;
        }

        int ddl = s->tasks[i].deadline_day * SLOTS_PER_DAY
                + s->tasks[i].deadline_slot;

        int used = 0;
        for(int d = 0; d < DAYS; d++){
            for (int h = 0; h < SLOTS_PER_DAY; h++)
                {
                    if (s->slots[d][h].task_index == i){
                        int abs_index = d *SLOTS_PER_DAY +h;
                        if(abs_index < ddl){
                            used++;
                        }
                    }
                }
            
        }

        if(used > duration + 2){
            total += ((double)used - (double) duration) * ((double)used - (double) duration) * 0.5 * DEADLINE_MISS_PENALTY; //任务排太多也不是什么好事
            continue;
        }
        if(used < duration){
            double base;
            if(used == 0){
                base = (double)s->tasks[i].importance
                    * duration * 3;
            }
            else{
                base = (double)s->tasks[i].importance
                    * (double)duration / (double)used;
            }
            total += base * DEADLINE_MISS_PENALTY;
        }
    }

    return total;
}

double penalty_late_night(const Schedule *s) {//熬夜惩罚，熬夜时长越长罚的越重。这里取最晚时间，因为睡觉必须在最晚任务时间之后，否则前面即使在休息也要算熬夜。
    if (s == NULL) {
        return 0;
    }

    double total = 0;

    for (int d = 0; d < DAYS; d++) {
        int last=LATE_NIGHT_START - 1; // 89
        for (int h = LATE_NIGHT_START; h < SLOTS_PER_DAY; h++) {
            if (s->slots[d][h].task_index != -1) { // busy
                    last = h;//last是每一天最后一个任务所在时间槽
            }
        }
        int night_slots = last - LATE_NIGHT_START + 1;//熬夜时长
        total += night_slots * LATE_NIGHT_PENALTY_PER_SLOT;
    }

    return total;
}

double penalty_rest_fragmentation(const Schedule *s){//希望休息时间不要太零碎。10分钟作为中途休息可以接受，30分钟以上是长时间休息也挺好，20分钟休息时间太尴尬了，不好。
    if (s == NULL) {
        return 0;
    }
    
    double total = 0;

    for(int d = 0; d < DAYS; d++){
        int free_run = 0;

        for(int h = 0; h < SLOTS_PER_DAY; h++){
            int occupied = (s->slots[d][h].task_index != -1)
                        || (s->slots[d][h].busy == 1);
            
            if(!occupied){
                free_run++;
            }
            else{
                if(free_run == 2){
                    total += REST_FRAGMENTATION_PENALTY;
                }
            free_run = 0;
            }
        }//结尾不检查是否空闲，因为最后的空闲时间可以并入睡眠时间。
    }

    return total;
}

static int content_id(const Slot *slot) {//用来区分时间槽内容
    if (slot->task_index != -1) return slot->task_index;  /* 任务 */
    return slot->busy ? -2 : -1;                          /* 课程 / 空闲 */
}
double penalty_work_fragmentation(const Schedule *s){//希望工作时间段内不要频繁切换内容
    if (s == NULL) {
        return 0;
    }

    double total = 0;

    for (int d = 0; d < DAYS; d++) {
        int prev = -1;
        int switches = 0;

        for (int h = 0; h < SLOTS_PER_DAY; h++) {
            int cur = content_id(&s->slots[d][h]);

            if (cur != -1) {
                if (prev != -1 && prev != cur) {
                    switches++;
                }
                prev = cur;
            } else {
                total += (double)switches * switches * WORK_FRAGMENTATION_PENALTY;
                switches = 0;
                prev = -1;
            }
        }
        total += (double)switches * switches * WORK_FRAGMENTATION_PENALTY;
    }

    return total;
}

double penalty_work_time(const Schedule *s){//长度过长或过短的工作时间惩罚
    if (s == NULL) {
        return 0;
    }
    
    double total = 0;

    for(int d = 0; d < DAYS; d++){
        int work_streak = 0;

        for(int h = 0; h < SLOTS_PER_DAY; h++){           
            int occupied = (s->slots[d][h].task_index != -1)
                        || (s->slots[d][h].busy == 1);

            if(occupied){
                work_streak++;
            }
            else if(work_streak > 0){
                if(work_streak > 5){
                    double base = (work_streak - 5) * (work_streak - 5) *0.3;
                    total += base * OVERWORK_PENALTY;
                }
                if(work_streak < 5){
                    double base = (work_streak - 5) * (work_streak - 5) *0.1;
                    total += base * OVERWORK_PENALTY;
                }

                work_streak = 0;
            }
        }
        if(work_streak > 0){
            if(work_streak > 5){
                double base = (work_streak - 5) * (work_streak - 5) *0.3;
                total += base * OVERWORK_PENALTY;
            }
            if(work_streak < 5){
                double base = (work_streak - 5) * (work_streak - 5) *0.1;
                total += base * OVERWORK_PENALTY;
            }
        }
    }

    return total;
}

double reward_rest_streak(const Schedule *s){ //长度超过一小时的休息时间有奖励
    if (s == NULL) {
        return 0;
    }
    
    double total = 0;

    for(int d = 0; d < DAYS; d++){
        int rest_streak = 0;

        for(int h = 0; h < SLOTS_PER_DAY; h++){           
            int occupied = (s->slots[d][h].task_index != -1)
                        || (s->slots[d][h].busy == 1);

            if(!occupied){
                rest_streak++;
            }
            else {
                if(rest_streak > 24) {
                    rest_streak=24;
                }
                if(rest_streak > 6){
                    double base = (rest_streak - 6) * (rest_streak - 6) / 36.0;
                    total += base * REST_STREAK_REWARD;
                }
                rest_streak = 0;
            }
        }
        if(rest_streak > 24) {
                    rest_streak=24;
                }
        if(rest_streak > 0){
            double score = rest_streak * rest_streak / 36.0 *0.5; //早睡奖励。这里不检查是否大于一小时是因为休息与睡眠时间合并，肯定超过一小时了。
                total += score * REST_STREAK_REWARD;
        }
    }

    return total;
}

double cost(const Schedule *s){
    return penalty_deadline(s)
         + penalty_rest_fragmentation(s)
         + penalty_work_fragmentation(s)
         + penalty_late_night(s)
         + penalty_work_time(s)
         - reward_rest_streak(s);
}
