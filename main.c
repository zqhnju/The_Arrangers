#include "io.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

static int ensure_output_directory(const char *path)
{
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0777);
#endif
    if (result == 0 || errno != EEXIST)
        return result;

    struct stat info;
    if (stat(path, &info) != 0)
        return -1;
    if (!S_ISDIR(info.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    return 0;
}

static int export_schedule(const Schedule *schedule, const char *directory,
                           double initial_cost, double final_cost)
{
    size_t length = strlen(directory);
    const char *separator = directory[length - 1] == '/' ? "" : "/";
#ifdef _WIN32
    if (directory[length - 1] == '\\')
        separator = "";
#endif
    size_t capacity = length + sizeof("/schedule.json");
    char *path = malloc(capacity);
    if (path == NULL) {
        fputs("Could not allocate memory for the output path.\n", stderr);
        return -1;
    }

    snprintf(path, capacity, "%s%sschedule.txt", directory, separator);
    int result = export_schedule_txt(schedule, path);
    if (result == 0) {
        snprintf(path, capacity, "%s%sschedule.json", directory, separator);
        result = export_schedule_json(schedule, path, initial_cost, final_cost);
    }
    if (result != 0)
        fprintf(stderr, "Could not export the schedule to %s\n", path);

    free(path);
    return result;
}

int main(int argc, char *argv[])
{
    if (argc != 4 || argv[3][0] == '\0') {
        fprintf(stderr, "Usage: %s COURSES_FILE TASKS_FILE OUTPUT_DIR\n", argv[0]);
        return EXIT_FAILURE;
    }

    puts("The Arrangers");

    Schedule initial = {0};
    for (int day = 0; day < DAYS; day++) {
        for (int slot = 0; slot < SLOTS_PER_DAY; slot++) {
            initial.slots[day][slot].course_index = -1;
            initial.slots[day][slot].task_index = -1;
        }
    }

    if (load_existing_courses(argv[1], &initial) != 0) {
        fprintf(stderr, "Could not load courses from %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    if (load_tasks(argv[2], &initial) != 0) {
        fprintf(stderr, "Could not load tasks from %s\n", argv[2]);
        return EXIT_FAILURE;
    }
    if (ensure_output_directory(argv[3]) != 0) {
        fprintf(stderr, "Could not use output directory %s (%s)\n",
                argv[3], strerror(errno));
        return EXIT_FAILURE;
    }

    double initial_cost = cost(&initial);
    Schedule best = anneal(initial, 50000, 100.0, 0.9995, 0.01);
    double final_cost = cost(&best);
    printf("Initial cost: %.2f\nFinal cost: %.2f\n", initial_cost, final_cost);

    if (export_schedule(&best, argv[3], initial_cost, final_cost) != 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
