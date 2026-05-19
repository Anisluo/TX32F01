#ifndef _SCPI_CORE_H_
#define _SCPI_CORE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 处理函数签名：param 为空或参数串；query=1 表示查询命令
typedef void (*ScpiHandler)(const char *param, int query);

typedef struct {
    const char *path;   // e.g. "SOUR:PWM:DUTY" 或 "*IDN"
    ScpiHandler handler;
} SCPI_Command;

void SCPI_ParseLine(const char *line);
void SCPI_Register(const char *path, ScpiHandler handler);

// 子系统注册入口（由各 scpi_xxx.c 实现并在 main 调用）
void SCPI_PWM_Register(void);
void SCPI_SYS_Register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCPI_CORE_H_