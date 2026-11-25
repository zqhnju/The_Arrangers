
/* ==============================
   头文件
   ============================== */
#include <stdio.h>
#include <string.h>
#include "io.h"


/* ==============================
   1. time_to_slot()
   将小时和分钟转换为时间槽
   ============================== */
int time_to_slot(int hour, int minute)
{
    /* 检查分钟是否合法 */
    if (minute < 0 || minute >= 60)
        return -1;

    /* 检查是否为10分钟的整数倍 */
    if (minute % SLOT_MINUTES != 0)
        return -1;

    /* 特殊处理24:00 */
    if (hour == 24 && minute == 0)
        return SLOTS_PER_DAY;

    /* 检查小时范围 */
    if (hour < START_HOUR || hour >= 24)
        return -1;

    /* 计算时间槽 */
    return (hour - START_HOUR) * (60 / SLOT_MINUTES)
           + minute / SLOT_MINUTES;
}


/* ==============================
   2. slot_to_time()
   将时间槽转换为时间字符串
   ============================== */
void slot_to_time(int slot, char *buf, int n)
{
    int total_minutes;
    int hour;
    int minute;

    /* 检查参数 */
    if (buf == NULL || n <= 0)
        return;

    if (n < 6) {
        buf[0] = '\0';
        return;
    }

    /* 检查时间槽范围 */
    if (slot < 0 || slot > SLOTS_PER_DAY) {
        snprintf(buf, n, "ERROR");
        return;
    }

    /* 转换为总分钟数 */
    total_minutes = START_HOUR * 60
                    + slot * SLOT_MINUTES;

    /* 计算小时和分钟 */
    hour = total_minutes / 60;
    minute = total_minutes % 60;

    /* 转换为字符串 */
    snprintf(buf, n, "%02d:%02d", hour, minute);
}


/* ==============================
   3. load_existing_courses()
   读取已有课程
   ============================== */
int load_existing_courses(const char *filename, Schedule *s)
{
    FILE *fp;
    char line[256];

    char name[MAX_NAME];
    char start_time[6];
    char end_time[6];
    char extra;

    int day;
    int start_hour, start_minute;
    int end_hour, end_minute;
    int start_slot, end_slot;

    int line_number = 0;
    int count;
    int index;
    int h;

    if (filename == NULL || s == NULL)
        return -1;

    if (s->course_count < 0 ||
        s->course_count > MAX_COURSES)
        return -1;

    /* 打开课程文件 */
    fp = fopen(filename, "r");

    if (fp == NULL) {
        printf("无法打开课程文件：%s\n", filename);
        return -1;
    }

    /* 逐行读取 */
    while (fgets(line, sizeof(line), fp) != NULL) {

        line_number++;

        /* 跳过空行 */
        if (line[strspn(line, " \t\r\n")] == '\0')
            continue;

        /* 读取课程信息 */
        count = sscanf(
            line,
            " %63s %d %5s %5s %c",
            name,
            &day,
            start_time,
            end_time,
            &extra
        );

        if (count != 4) {
            printf("课程文件第%d行格式错误\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 检查星期 */
        if (day < 0 || day >= DAYS) {
            printf("第%d行星期不合法\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 拆分开始时间 */
        if (sscanf(start_time, "%2d:%2d",
                   &start_hour, &start_minute) != 2) {
            printf("第%d行开始时间格式错误\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 拆分结束时间 */
        if (sscanf(end_time, "%2d:%2d",
                   &end_hour, &end_minute) != 2) {
            printf("第%d行结束时间格式错误\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 转换为时间槽 */
        start_slot = time_to_slot(start_hour, start_minute);
        end_slot = time_to_slot(end_hour, end_minute);

        if (start_slot < 0 || end_slot < 0 ||
            start_slot >= end_slot ||
            start_slot >= SLOTS_PER_DAY ||
            end_slot > SLOTS_PER_DAY) {
            printf("第%d行课程时间不合法\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 检查课程数量 */
        if (s->course_count >= MAX_COURSES) {
            printf("课程数量超过最大限制\n");
            fclose(fp);
            return -1;
        }

        /* 确定课程存储位置 */
        index = s->course_count;

        /* 检查时间是否冲突 */
        for (h = start_slot; h < end_slot; h++) {

            Slot *slot = &s->slots[day][h];

            if (slot->busy != 0 ||
                slot->course_index != -1 ||
                slot->task_index != -1) {

                printf("第%d行课程时间与已有安排冲突\n",
                       line_number);

                fclose(fp);
                return -1;
            }
        }

        /* 保存课程信息 */
        {
            Course *course = &s->courses[index];

            snprintf(course->name, sizeof(course->name),
                     "%s", name);

            course->day = day;
            course->start_slot = start_slot;
            course->end_slot = end_slot;
        }

        /* 标记课程占用的时间槽 */
        for (h = start_slot; h < end_slot; h++) {

            s->slots[day][h].busy = 1;
            s->slots[day][h].course_index = index;
            s->slots[day][h].task_index = -1;
        }

        /* 课程数量增加 */
        s->course_count++;
    }

    /* 检查文件读取错误 */
    if (ferror(fp)) {
        printf("读取课程文件时发生错误\n");
        fclose(fp);
        return -1;
    }

    /* 关闭文件 */
    fclose(fp);

    return 0;
}



int load_tasks(const char *filename, Schedule *s)
{
    FILE *fp;
    char line[256];

    char name[MAX_NAME];
    char deadline_time[6];
    char extra;

    int duration_minutes;
    int duration_slots;

    int deadline_day;
    int deadline_hour;
    int deadline_minute;
    int deadline_slot;

    int importance;
    int line_number;
    int count;
    int index;

    Task *task;

    /* 检查传入参数 */
    if (filename == NULL || s == NULL)
        return -1;

    /* 检查当前任务数量 */
    if (s->task_count < 0 || s->task_count > MAX_TASKS)
        return -1;

    /* 打开任务文件 */
    fp = fopen(filename, "r");

    if (fp == NULL) {
        printf("无法打开任务文件：%s\n", filename);
        return -1;
    }

    line_number = 0;

    /* 逐行读取任务文件 */
    while (fgets(line, sizeof(line), fp) != NULL) {

        line_number++;

        /* 跳过空行 */
        if (line[strspn(line, " \t\r\n")] == '\0')
            continue;

        /* 读取一行中的五个字段 */
        count = sscanf(
            line,
            " %63s %d %d %5s %d %c",
            name,
            &duration_minutes,
            &deadline_day,
            deadline_time,
            &importance,
            &extra
        );

        /* 检查字段数量 */
        if (count != 5) {
            printf("任务文件第%d行格式错误\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 检查任务耗时 */
        if (duration_minutes <= 0 ||
            duration_minutes % SLOT_MINUTES != 0) {

            printf("第%d行任务耗时不合法\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 检查截止星期 */
        if (deadline_day < 0 || deadline_day >= DAYS) {

            printf("第%d行截止日期不合法\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 检查截止时间格式是否为 HH:MM */
        if (strlen(deadline_time) != 5 ||
            deadline_time[2] != ':' ||
            deadline_time[0] < '0' ||
            deadline_time[0] > '9' ||
            deadline_time[1] < '0' ||
            deadline_time[1] > '9' ||
            deadline_time[3] < '0' ||
            deadline_time[3] > '9' ||
            deadline_time[4] < '0' ||
            deadline_time[4] > '9') {

            printf("第%d行截止时间格式错误\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 将时间字符串拆分为小时和分钟 */
        deadline_hour =
            (deadline_time[0] - '0') * 10
            + deadline_time[1] - '0';

        deadline_minute =
            (deadline_time[3] - '0') * 10
            + deadline_time[4] - '0';

        /* 将截止时间转换为时间槽 */
        deadline_slot = time_to_slot(
            deadline_hour,
            deadline_minute
        );

        if (deadline_slot < 0) {

            printf("第%d行截止时间不合法\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 检查重要程度 */
        if (importance < 1 || importance > 5) {

            printf("第%d行重要程度不合法\n", line_number);
            fclose(fp);
            return -1;
        }

        /* 检查任务数量是否超过上限 */
        if (s->task_count >= MAX_TASKS) {

            printf("任务数量超过最大限制\n");
            fclose(fp);
            return -1;
        }

        /* 将分钟数转换为时间槽数量 */
        duration_slots = duration_minutes / SLOT_MINUTES;

        /* 确定任务存储位置 */
        index = s->task_count;

        /* 获取当前任务的结构体指针 */
        task = &s->tasks[index];

        /* 保存任务信息 */
        snprintf(task->name, sizeof(task->name),
                 "%s", name);

        task->duration_slots = duration_slots;
        task->deadline_day = deadline_day;
        task->deadline_slot = deadline_slot;
        task->importance = importance;

        /* 任务数量增加 */
        s->task_count++;
    }

    /* 检查文件读取是否发生错误 */
    if (ferror(fp)) {
        printf("读取任务文件时发生错误\n");
        fclose(fp);
        return -1;
    }

    /* 关闭文件 */
    fclose(fp);

    return 0;
}


int export_schedule_txt(const Schedule *s, const char *filename)
{
    FILE *fp;

    int day;
    int i;
    int h;

    int start;
    int end;
    int task_index;
    int assigned_slots;

    char start_time[6];
    char end_time[6];

    const char *day_names[DAYS] = {
        "星期一", "星期二", "星期三", "星期四",
        "星期五", "星期六", "星期日"
    };

    /* 检查参数 */
    if (s == NULL || filename == NULL)
        return -1;

    if (s->course_count < 0 || s->course_count > MAX_COURSES ||
        s->task_count < 0 || s->task_count > MAX_TASKS)
        return -1;

    /* 打开文件，以写入模式创建 TXT 文件 */
    fp = fopen(filename, "w");

    if (fp == NULL) {
        printf("无法创建文件：%s\n", filename);
        return -1;
    }

    fprintf(fp, "========== The Arrangers 学习计划 ==========\n\n");

    /* 按星期输出安排 */
    for (day = 0; day < DAYS; day++) {

        fprintf(fp, "========== %s ==========\n", day_names[day]);

        /* 输出当天课程 */
        fprintf(fp, "\n【课程安排】\n");

        for (i = 0; i < s->course_count; i++) {

            if (s->courses[i].day != day)
                continue;

            slot_to_time(s->courses[i].start_slot,
                         start_time, 6);

            slot_to_time(s->courses[i].end_slot,
                         end_time, 6);

            fprintf(fp, "%s  %s - %s\n",
                    s->courses[i].name,
                    start_time,
                    end_time);
        }

        /* 输出当天任务 */
        fprintf(fp, "\n【学习任务安排】\n");

        h = 0;

        while (h < SLOTS_PER_DAY) {

            task_index = s->slots[day][h].task_index;

            /* 当前时间槽没有安排有效任务 */
            if (task_index < 0 || task_index >= s->task_count) {
                h++;
                continue;
            }

            /* 记录任务开始时间槽 */
            start = h;
            end = h + 1;

            /* 寻找连续安排的同一项任务 */
            while (end < SLOTS_PER_DAY &&
                   s->slots[day][end].task_index == task_index) {
                end++;
            }

            /* 将时间槽转换为时分 */
            slot_to_time(start, start_time, 6);
            slot_to_time(end, end_time, 6);

            /* 输出任务名称和时间 */
            fprintf(fp, "%s  %s - %s\n",
                    s->tasks[task_index].name,
                    start_time,
                    end_time);

            h = end;
        }

        fprintf(fp, "\n");
    }

    /* 输出任务安排统计 */
    fprintf(fp, "========== 任务安排统计 ==========\n");

    for (i = 0; i < s->task_count; i++) {

        assigned_slots = 0;

        /* 统计该任务一周内被安排的时间槽数量 */
        for (day = 0; day < DAYS; day++) {

            for (h = 0; h < SLOTS_PER_DAY; h++) {

                if (s->slots[day][h].task_index == i)
                    assigned_slots++;
            }
        }

        fprintf(fp, "%s：已安排 %d 分钟 / 需要 %d 分钟\n",
                s->tasks[i].name,
                assigned_slots * SLOT_MINUTES,
                s->tasks[i].duration_slots * SLOT_MINUTES);
    }

    /* 检查文件写入是否发生错误 */
    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }

    /* 关闭文件 */
    if (fclose(fp) != 0)
        return -1;

    return 0;
}

/* ==========================================
   辅助函数：将字符串写入 JSON 文件
   ========================================== */
static void write_json_string(FILE *fp, const char *str)
{
    const unsigned char *p;

    fputc('"', fp);

    p = (const unsigned char *)str;

    while (*p != '\0') {

        if (*p == '"' || *p == '\\') {
            fputc('\\', fp);
            fputc(*p, fp);
        }
        else if (*p == '\n') {
            fputs("\\n", fp);
        }
        else if (*p == '\r') {
            fputs("\\r", fp);
        }
        else if (*p == '\t') {
            fputs("\\t", fp);
        }
        else if (*p < 32) {
            fprintf(fp, "\\u%04x", (unsigned int)*p);
        }
        else {
            fputc(*p, fp);
        }

        p++;
    }

    fputc('"', fp);
}


/*export_schedule_json()将排程结果导出为 JSON 文件 */
int export_schedule_json(const Schedule *s,
                         const char *filename,
                         double init_cost,
                         double final_cost)
{
    FILE *fp;

    int i;
    int day;
    int h;
    int first;

    /* 检查参数 */
    if (s == NULL || filename == NULL)
        return -1;

    /* 检查任务和课程数量 */
    if (s->course_count < 0 ||
        s->course_count > MAX_COURSES ||
        s->task_count < 0 ||
        s->task_count > MAX_TASKS)
        return -1;

    /* 打开 JSON 文件 */
    fp = fopen(filename, "w");

    if (fp == NULL) {
        printf("无法创建 JSON 文件：%s\n", filename);
        return -1;
    }

    /* 写入 JSON 对象的开头 */
    fprintf(fp, "{\n");

    /* 写入初始代价和最终代价 */
    fprintf(fp, "  \"init_cost\": %.6f,\n", init_cost);
    fprintf(fp, "  \"final_cost\": %.6f,\n", final_cost);

    /* ======================================
       导出课程信息
       ====================================== */
    fprintf(fp, "  \"courses\": [\n");

    for (i = 0; i < s->course_count; i++) {

        if (i > 0)
            fprintf(fp, ",\n");

        fprintf(fp, "    {\n");

        fprintf(fp, "      \"name\": ");
        write_json_string(fp, s->courses[i].name);
        fprintf(fp, ",\n");

        fprintf(fp, "      \"day\": %d,\n",
                s->courses[i].day);

        fprintf(fp, "      \"start_slot\": %d,\n",
                s->courses[i].start_slot);

        fprintf(fp, "      \"end_slot\": %d\n",
                s->courses[i].end_slot);

        fprintf(fp, "    }");
    }

    fprintf(fp, "\n  ],\n");

    /* ======================================
       导出任务信息
       ====================================== */
    fprintf(fp, "  \"tasks\": [\n");

    for (i = 0; i < s->task_count; i++) {

        if (i > 0)
            fprintf(fp, ",\n");

        fprintf(fp, "    {\n");

        fprintf(fp, "      \"name\": ");
        write_json_string(fp, s->tasks[i].name);
        fprintf(fp, ",\n");

        fprintf(fp, "      \"duration_slots\": %d,\n",
                s->tasks[i].duration_slots);

        fprintf(fp, "      \"deadline_day\": %d,\n",
                s->tasks[i].deadline_day);

        fprintf(fp, "      \"deadline_slot\": %d,\n",
                s->tasks[i].deadline_slot);

        fprintf(fp, "      \"importance\": %d\n",
                s->tasks[i].importance);

        fprintf(fp, "    }");
    }

    fprintf(fp, "\n  ],\n");

    /* ======================================
       导出任务时间槽安排
       ====================================== */
    fprintf(fp, "  \"task_slots\": [\n");

    first = 1;

    for (day = 0; day < DAYS; day++) {

        for (h = 0; h < SLOTS_PER_DAY; h++) {

            if (s->slots[day][h].task_index < 0)
                continue;

            if (s->slots[day][h].task_index >= s->task_count) {
                fclose(fp);
                return -1;
            }

            if (!first)
                fprintf(fp, ",\n");

            fprintf(fp,
                    "    {\"day\": %d, \"slot\": %d, \"task_index\": %d}",
                    day,
                    h,
                    s->slots[day][h].task_index);

            first = 0;
        }
    }

    fprintf(fp, "\n  ]\n");

    /* 写入 JSON 对象的结尾 */
    fprintf(fp, "}\n");

    /* 检查文件写入错误 */
    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }

    /* 关闭文件 */
    if (fclose(fp) != 0)
        return -1;

    return 0;
}


void print_statistics(const Schedule *s)//输出一周的排程 
{
    int day;
    int h;
    int i;
    int task_index;

    int course_slots = 0;
    int task_slots = 0;

    int completed_tasks = 0;
    int uncompleted_tasks = 0;

    int assigned_slots[MAX_TASKS];

    /* 检查参数 */
    if (s == NULL) {
        printf("日程表为空，无法统计\n");
        return;
    }

    if (s->task_count < 0 || s->task_count > MAX_TASKS ||
        s->course_count < 0 || s->course_count > MAX_COURSES) {
        printf("日程数据不合法\n");
        return;
    }

    /* 初始化每项任务的已安排时间槽数量 */
    for (i = 0; i < MAX_TASKS; i++) {
        assigned_slots[i] = 0;
    }

    /* 遍历一周所有时间槽 */
    for (day = 0; day < DAYS; day++) {

        for (h = 0; h < SLOTS_PER_DAY; h++) {

            /* 统计课程占用时间 */
            if (s->slots[day][h].busy == 1) {
                course_slots++;
            }

            /* 获取当前时间槽的任务编号 */
            task_index = s->slots[day][h].task_index;

            /* 统计任务占用时间 */
            if (task_index >= 0 &&
                task_index < s->task_count) {

                assigned_slots[task_index]++;
                task_slots++;
            }
        }
    }

    /* 统计任务安排情况 */
    for (i = 0; i < s->task_count; i++) {

        if (s->tasks[i].duration_slots > 0 &&
            assigned_slots[i] >= s->tasks[i].duration_slots) {

            completed_tasks++;
        }
        else {
            uncompleted_tasks++;
        }
    }

    /* 输出统计结果 */
    printf("\n========== 排程统计 ==========\n");

    printf("课程数量：%d\n", s->course_count);
    printf("任务数量：%d\n", s->task_count);

    printf("课程占用时间：%d 分钟\n",
           course_slots * SLOT_MINUTES);

    printf("任务安排时间：%d 分钟\n",
           task_slots * SLOT_MINUTES);

    printf("达到所需安排时长的任务：%d\n",
           completed_tasks);

    printf("未达到所需安排时长的任务：%d\n",
           uncompleted_tasks);

    /* 输出每项任务的具体安排情况 */
    printf("\n========== 任务详情 ==========\n");

    for (i = 0; i < s->task_count; i++) {

        printf("%s：已安排 %d / %d 分钟\n",
               s->tasks[i].name,
               assigned_slots[i] * SLOT_MINUTES,
               s->tasks[i].duration_slots * SLOT_MINUTES);
    }

    printf("==============================\n");
}


