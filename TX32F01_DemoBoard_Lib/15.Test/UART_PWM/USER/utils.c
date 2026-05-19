#include "utils.h"
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>


//字符串对比
int utils_stricmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        ++a; ++b;
    }
    return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}

static const char* rtrim_end(const char *s)
{
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) --e;
    return e;
}

const char* utils_trim(const char *s)
{
    while (*s && isspace((unsigned char)*s)) ++s;
    return s;
}

const char* utils_find_space(const char *s)
{
    for (; *s; ++s) {
        if (isspace((unsigned char)*s)) return s;
    }
    return NULL;
}

const char* utils_skip_spaces(const char *s)
{
    while (*s && isspace((unsigned char)*s)) ++s;
    return s;
}

int utils_parse_int(const char *s, int defv)
{
    if (!s || !*s) return defv;
    int neg = 0; long v = 0;
    while (isspace((unsigned char)*s)) ++s;
    if (*s == '+' || *s == '-') { neg = (*s=='-'); ++s; }
    if (!isdigit((unsigned char)*s)) return defv;
    while (isdigit((unsigned char)*s)) { v = v*10 + (*s - '0'); ++s; }
    return neg ? -(int)v : (int)v;
}


int utils_parse_int_at(const char *s, int index, int defv) {
    if (!s || index < 0) return defv;
    const char *p = s;

    for (int i = 0; ; ++i) {
        // 跳过前导空白
        while (*p && isspace((unsigned char)*p)) ++p;

        // 本字段起点
        const char *tok_start = p;
        // 找到本字段的结束（逗号或字符串结尾）
        while (*p && *p != ',') ++p;
        const char *tok_end = p;

        // 去掉字段尾部空白
        while (tok_end > tok_start && isspace((unsigned char)tok_end[-1])) --tok_end;

        if (i == index) {
            if (tok_start == tok_end) return defv;  // 空字段
            // 复制到小缓冲里喂给 strtol
            char buf[64];
            size_t n = (size_t)(tok_end - tok_start);
            if (n >= sizeof(buf)) n = sizeof(buf) - 1;
            memcpy(buf, tok_start, n);
            buf[n] = '\0';

            errno = 0;
            char *endp;
            long v = strtol(buf, &endp, 0);        // base=0 支持 0x/0 前缀
            if (endp == buf || errno == ERANGE) return defv;
            if (v > INT_MAX) v = INT_MAX;
            if (v < INT_MIN) v = INT_MIN;
            return (int)v;
        }

        // 到结尾就没有下一个字段了
        if (!*p) break;
        // 跳过逗号，进入下一个字段
        ++p;
    }
    return defv;
}


unsigned utils_parse_uint(const char *s, unsigned defv)
{
    if (!s || !*s) return defv;
    while (isspace((unsigned char)*s)) ++s;
    if (!isdigit((unsigned char)*s)) return defv;
    unsigned v = 0;
    while (isdigit((unsigned char)*s)) { v = v*10u + (unsigned)(*s - '0'); ++s; }
    return v;
}