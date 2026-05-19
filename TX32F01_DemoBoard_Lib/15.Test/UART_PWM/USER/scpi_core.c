
#include "scpi_core.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#ifndef SCPI_TABLE_MAX
#define SCPI_TABLE_MAX  48
#endif


//定义SCPI指令表
static SCPI_Command s_table[SCPI_TABLE_MAX];
static int s_count = 0;

//注册SCPI指令
void SCPI_Register(const char *path, ScpiHandler handler)
{
    if (s_count < SCPI_TABLE_MAX) {
        s_table[s_count].path    = path;
        s_table[s_count].handler = handler;
        s_count++;
    } else {
        printf("ERR:Table full\r\n");
    }
}

// 解析一行：分离 cmd 和 param，并识别查询符 '?'
void SCPI_ParseLine(const char *line)
{
    char cmd[96];
    const char *param = NULL;
    int is_query = 0;

    // 去首尾空白
    const char *trimmed = utils_trim(line);

    // 拆分命令与参数（以空格为界）
    const char *sp = utils_find_space(trimmed);
    if (sp) {
        size_t n = (size_t)(sp - trimmed);
        if (n >= sizeof(cmd)) n = sizeof(cmd) - 1;
        memcpy(cmd, trimmed, n);
        cmd[n] = '\0';
        param = utils_skip_spaces(sp + 1);
        if (param && *param == '\0') param = NULL;
    } else {
        strncpy(cmd, trimmed, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
    }

    // 结尾 '?' → 查询
    size_t L = strlen(cmd);
    if (L > 0 && cmd[L - 1] == '?') {
        cmd[L - 1] = '\0';
        is_query = 1;
    }

    // 遍历表匹配（不区分大小写）
    for (int i = 0; i < s_count; ++i) {
        if (utils_stricmp(cmd, s_table[i].path) == 0) {
            s_table[i].handler(param ? param : "", is_query);
            return;
        }
    }

    printf("ERR:Unknown '%s'\r\n", cmd);
}



