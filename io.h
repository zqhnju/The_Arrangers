#ifndef IO_H
#define IO_H

#include "scheduler.h"

int load_existing_courses(const char *filename, Schedule *s);  /* 0 成功 */
int load_tasks(const char *filename, Schedule *s);             /* 0 成功 */
int export_schedule_txt(const Schedule *s, const char *filename);
int export_schedule_json(const Schedule *s, const char *filename,
                         double init_cost, double final_cost);

int  time_to_slot(int hour, int minute);        /* 失败返回 -1 */
void slot_to_time(int slot, char *buf, int n);  /* buf 至少 6 字节 */

void print_statistics(const Schedule *s);

#endif