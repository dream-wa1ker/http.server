// name : first_line.c
// date : 2026-11-09
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//--------------------------------------- main program ----------------------------------

void parse_request_line(char *request) {
    char *eol;
    if ((eol = strstr(request, "\r\n")) == NULL) {
        puts("nope");
    }
    char req_line[256 + 16 + 16] = {0};
    printf("%s\n", eol);
    size_t len = strlen(request) - strlen(eol);
    strncpy(req_line, request, len);
    printf("%s", req_line);
}

int main(void) {
    char *request = "HTTP/1.1 /somepath /lol\r\n"
                    "Wth\r\n";
    parse_request_line(request);
    return EXIT_SUCCESS;
}
