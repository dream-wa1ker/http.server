// name : strchr.c
// date : 2026-11-08
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//--------------------------------------- main program ----------------------------------
int main(void) {
    puts("Using strchr");
    char *text = "Content-Type: text/html\r\n";
    // pointer to the colon;
    char *colon = strchr(text, ':');
    if (!colon) {
        fprintf(stderr, "Invalid text provided\n");
        exit(EXIT_FAILURE);
    }
    char *eol = strstr(text, "\r\n");
    eol += 2;
    if (!eol) {
        fprintf(stderr, "Invalid text provided with no end\n");
        exit(EXIT_FAILURE);
    }
    size_t length = eol - colon;
    char *value = text + length;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    printf("After Colon : %s\n", value);
    return EXIT_SUCCESS;
}
