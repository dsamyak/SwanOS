#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

int    strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, int n);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, int n);
char  *strcat(char *dst, const char *src);
char  *strchr(const char *s, int c);
void  *memset(void *ptr, int val, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void   itoa(int num, char *buf, int base);
int    atoi(const char *s);
int    isdigit(int c);
int    isspace(int c);
char  *trim(char *s);

#endif
