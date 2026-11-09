// name : validate_www.c
// date : 2026-11-09
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//--------------------------------------- main program ----------------------------------

bool starts_with(char *string, char *substring) {
    size_t stringlen = strlen(string);
    size_t substrlen = strlen(substring);
    if (stringlen < substrlen) {
        return false;
    }
    for (size_t i = 0; i < substrlen; i++) {
        if (string[i] != substring[i])
            return false;
    }
    return true;
}

bool is_requesting_hidden(char *path) {
    if (starts_with(path, "/.") && !starts_with(path, "/.."))
        return true;
    if (starts_with(path, "//")) {
        size_t len = strlen(path);
        for (size_t i = 0; i < len; i++) {
            if (strncmp(&path[i], "/", 1) == 0) {
                if (i + 1 < len && strncmp(&path[i + 1], ".", 1) == 0) {
                    if (i + 2 < len && strncmp(&path[i + 2], ".", 1) == 0)
                        return false;
                    return true;
                }
                i++;
            }
        }
    }
    return false;
}

int main(void) {
    char *path = "/.//...../lolcat";
    if (is_requesting_hidden(path)) {
        printf("Yes, it is requesting hidden path\n");
        return EXIT_FAILURE;
    }
    printf("%s\n", path);
    return EXIT_SUCCESS;
}
