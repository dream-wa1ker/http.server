// name : get_mime_type.c
// date : 2026-11-08
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *get_mime_type(char *filepath) {
    // search the string from reverse;
    char *ptr = strrchr(filepath, '.');
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;
    printf("extension : %s\n", ptr);
    if (ptr == NULL) {
        return "application/octet-stream";
    }

    typedef const struct {
        const char *ext;
        const char *mimetype;
    } mime;
    mime mime_table[] = {
        // web and text files;
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "text/javascript"},
        {".mjs", "text/javascript"},
        {".txt", "text/plain"},
        {".csv", "text/csv"},
        {".md", "text/markdown"},
        {".xml", "application/xml"},
        {".json", "application/json"},
        {".jsonld", "application/ld+json"},
        {".webmanifest", "application/manifest+json"},
        {".xhtml", "application/xhtml+xml"},

        // images;
        {".png", "image/png"},
        {".apng", "image/apng"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".svg", "image/svg+xml"},
        {".ico", "image/vnd.microsoft.icon"},
        {".bmp", "image/bmp"},
        {".avif", "image/avif"},
        {".tif", "image/tiff"},
        {".tiff", "image/tiff"},

        // audio;
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".ogg", "audio/ogg"},
        {".oga", "audio/ogg"},
        {".opus", "audio/opus"},
        {".aac", "audio/aac"},
        {".m4a", "audio/mp4"},
        {".flac", "audio/flac"},
        {".mid", "audio/midi"},
        {".midi", "audio/midi"},

        // video;
        {".mp4", "video/mp4"},
        {".m4v", "video/mp4"},
        {".webm", "video/webm"},
        {".ogv", "video/ogg"},
        {".mpeg", "video/mpeg"},
        {".mpg", "video/mpeg"},
        {".avi", "video/x-msvideo"},
        {".mov", "video/quicktime"},
        {".mkv", "video/x-matroska"},

        // fonts;
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".eot", "application/vnd.ms-fontobject"},

        // documents;
        {".pdf", "application/pdf"},
        {".doc", "application/msword"},
        {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".xls", "application/vnd.ms-excel"},
        {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {".ppt", "application/vnd.ms-powerpoint"},
        {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {".epub", "application/epub+zip"},

        // compressed files and archives;
        {".zip", "application/zip"},
        {".gz", "application/gzip"},
        {".tar", "application/x-tar"},
        {".7z", "application/x-7z-compressed"},
        {".rar", "application/vnd.rar"},
        {".bz2", "application/x-bzip2"},
        {".xz", "application/x-xz"},

        // application and binary files;
        {".wasm", "application/wasm"},
        {".bin", "application/octet-stream"},
        {".exe", "application/octet-stream"}};

    size_t elements = sizeof(mime_table) / sizeof(mime_table[0]);
    for (size_t i = 0; i < elements; i++) {
        if (strcmp(ptr, (mime_table[i]).ext) == 0) {
            return mime_table[i].mimetype;
        }
    }
    return "application/octet-stream";
}

//--------------------------------------- main program ----------------------------------
int main(void) {
    const char *mime = get_mime_type("image.png");
    printf("Mime : %s\n", mime);
    return EXIT_SUCCESS;
}
