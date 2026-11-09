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

int main(void) {
    char *path = "/www/www/lolcat.txt";
    if (starts_with(path, "./www/") == false) {
        printf("invalid path");
        return EXIT_FAILURE;
    }
    printf("%s\n", path);
    return EXIT_SUCCESS;
}
