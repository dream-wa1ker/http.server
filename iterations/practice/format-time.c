// name : format-time.c
// date : 2026-11-09
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

void get_current_time(char *buffer, size_t size) {
    time_t raw_time;
    struct tm *time_info;
    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(buffer, size, "%d/%b/%Y %T", time_info);
}

//--------------------------------------- main program ----------------------------------
int main(void) {
    while (true) {
        char time[24] = {0};
        get_current_time(time, sizeof(time));
        size_t len = snprintf(NULL, 0, "- [%s]\n", time);
        char *line = malloc(len + 1);
        if (!line)
            return EXIT_FAILURE;
        snprintf(line, len + 1, "- [%s]\n", time);
        line[len] = '\0';
        printf("%s", line);
        free(line);
        sleep(1);
    }
    return EXIT_SUCCESS;
}
