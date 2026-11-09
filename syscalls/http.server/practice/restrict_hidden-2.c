// name : validate_www.c
// date : 2026-11-09
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//--------------------------------------- main program ----------------------------------

// Returns true if the path contains a hidden segment (e.g., /.git, /...../)
// safely ignoring standard structural paths like "." or ".."
bool is_requesting_hidden(const char *path) {
    if (path == NULL)
        return false;

    for (size_t i = 0; path[i] != '\0'; i++) {
        // Find the start of a segment: either at the beginning or immediately following a '/'
        if (i == 0 || path[i - 1] == '/') {
            // Check if this segment starts with a dot
            if (path[i] == '.') {
                const char *segment = &path[i];

                // Safely skip standard "." and ".." structural directories
                if (strcmp(segment, ".") == 0 || strncmp(segment, "./", 2) == 0)
                    continue;
                if (strcmp(segment, "..") == 0 || strncmp(segment, "../", 3) == 0)
                    continue;

                // Any other leading dot (like ".git" or ".....") is a hidden path
                return true;
            }
        }
    }
    return false;
}

int main(void) {
    char *path = "//./.lolcat";
    if (is_requesting_hidden(path)) {
        printf("Yes, it is requesting hidden path\n");
        return EXIT_FAILURE;
    }
    printf("%s\n", path);
    return EXIT_SUCCESS;
}
