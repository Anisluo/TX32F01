#ifndef _UTILS_H_
#define _UTILS_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int utils_stricmp(const char *a, const char *b); // 不区分大小写比较
const char* utils_trim(const char *s);           // 去前后空白(返回首指针)
const char* utils_find_space(const char *s);     // 找第一个空格
const char* utils_skip_spaces(const char *s);    // 跳过连续空格
int         utils_parse_int(const char *s, int defv);
unsigned    utils_parse_uint(const char *s, unsigned defv);
int utils_parse_int_at(const char *s, int index, int defv);//解析字符串数据

#ifdef __cplusplus
}
#endif

#endif // _UTILS_H_