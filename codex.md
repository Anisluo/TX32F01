# TX32F01 / Cortex-M0 深入研究建议

本文从 MCU 专家视角，整理 TX32F01 这类 Cortex-M0 小资源芯片最值得深入研究的方向。重点不只是“外设能不能跑”，而是：在 24 MHz、32 KB Flash、4 KB SRAM、无 VTOR 的约束下，如何做出可靠、可升级、可测量实时性的产品级固件。

## 1. 无 VTOR Cortex-M0 Bootloader 架构

这是当前工程最有研究价值的方向之一。Cortex-M0 没有 VTOR，APP 不能像 Cortex-M3/M4 那样简单重定位中断向量表。

当前工程采用的思路是：

- Bootloader 固定链接在 `0x01000000`
- APP 链接到 `0x01002000`
- Bootloader 永久接管硬件中断向量表
- SRAM `0x20000000` 放软向量表
- APP 通过 `APP_PATCH/app_softvec.c` 注册中断入口
- Bootloader trampoline 将真实 IRQ 转发给 APP handler

值得深入研究的问题：

- Bootloader 如何安全跳转 APP：设置 MSP、取 APP ResetHandler、清理状态
- 无 VTOR 芯片如何支持 OTA 升级
- SysTick、PendSV、SVC 这类系统异常如何转发
- APP 中断函数为什么不能只靠普通 `*_Handler`
- FreeRTOS 放在 bootloader 后面时，PendSV/SVC/SysTick 如何接入软向量
- 软向量表占用 SRAM 前 96 B 后，APP scatter file 如何避让

建议实验：

- 用 `APP_COOP` 验证 SysTick 软向量注册
- 将 Timer / EXTI 例程改造成 bootloader-compatible APP
- 测量 bootloader trampoline 对中断响应增加的 cycle 数
- 故意不注册中断，观察“主循环能跑但中断不进”的失败模式

## 2. 4 KB SRAM 下的软件架构选择

TX32F01 只有 4 KB SRAM，这会强迫固件架构回到本质：每一个 task stack、queue、buffer、printf 都有明确成本。

当前工程里有三种典型路径：

- 裸机 APP
- FreeRTOS
- 协作式调度器 COOP

值得深入比较：

- FreeRTOS 每个任务独立栈的 SRAM 成本
- queue、semaphore、mutex、timer、event group 的资源增量
- 静态分配和动态 heap 的取舍
- 协作式调度器在 4 KB SRAM 芯片上的优势
- ISR 与主循环之间如何安全交换状态
- `volatile`、原子访问、临界区之间的边界

建议结论方向：

- 复杂同步或多任务训练：研究 FreeRTOS
- 小资源量产固件：优先研究协作式调度器 + 短 ISR + 状态机
- SRAM 紧张时，不要用 RTOS 掩盖架构问题

## 3. 中断延迟与实时性边界

Cortex-M0 没有 DWT cycle counter，不能直接读 CPU cycle。当前 `APP_IRQ` 使用 SysTick CVR 做中断延迟测量，这个方向非常值得深入。

值得研究的问题：

- 从硬件事件到 C handler 第一行的真实延迟
- WFI 唤醒后的中断响应时间
- 忙循环运行时的中断响应时间
- `PRIMASK` 全局关中断对最坏延迟的影响
- Bootloader 软向量转发增加多少固定开销
- CM0 没有 BASEPRI，临界区为什么比 M3/M4 更危险

建议实验：

- 测 `NONE / BUSY / CRIT_64 / CRIT_256 / CRIT_1024` 五种压力模式
- 记录 min、max、avg、histogram
- 用示波器或逻辑分析仪辅助验证 GPIO 翻转延迟
- 将结果映射到 ADC 采样、PWM 更新、通信 bit-bang 的 deadline

这个研究可以直接回答一个产品级问题：在当前固件中，最坏情况下 ISR 还能不能按时响应？

## 4. 片内 Flash 擦写可靠性与 OTA 掉电安全

Bootloader、meta、boot flag、CRC 是从 demo 走向产品的关键基础设施。Flash 操作一旦错，轻则 APP 损坏，重则 bootloader 被擦掉。

值得研究的问题：

- Flash sector 边界和擦写粒度
- APP 区、meta 区、boot flag 区如何隔离
- 传输中断电后如何恢复
- meta magic、size、CRC 如何设计
- `Flash_CRC(start, end)` 的 inclusive 范围语义
- 24 MHz 下为什么必须调用 `Flash_SCU24MHz_ClkCfg()`
- Bootloader 为什么不能轻易自升级

建议实验：

- 正常 YMODEM OTA
- 传输过程中断电
- 发送超过 APP 区大小的 bin
- 手动破坏 meta 或 CRC
- 多次 OTA 后检查 APP 是否仍稳定启动
- 验证 bootloader 永远不擦写自身区域

## 5. 低功耗、看门狗与调试状态

低功耗和看门狗经常在 demo 中被分开验证，但在产品里它们会互相影响。

值得研究的问题：

- STOP 模式前后时钟和外设是否需要重新初始化
- EXTI 唤醒是否可靠
- IWDT 在 sleep/stop 模式中是否继续计数
- debug halt 时 IWDT 和 Timer 是否继续运行
- OTA 期间如果 IWDT 开启，是否会中途复位
- Bootloader 阶段是否应该开 IWDT

建议实验：

- 进入 STOP 后由 EXTI 唤醒
- 唤醒后重新验证 UART、Timer、SysTick
- 开启 IWDT 后测试 APP 卡死复位
- OTA 期间故意延长传输时间，观察 IWDT 策略是否安全
- 验证 `SCU_RSR` 中的复位原因标志

## 6. ADC、VREF 与 VDD 测量精度

TX32F01 的 ADC 带内部参考相关寄存器，适合研究小 MCU 的模拟测量能力。

值得研究的问题：

- ADC 实际有效位数和噪声
- 内部 2.048 V / 4.096 V 参考的实际精度
- 用内部 VREF 反推 VDD 的误差
- 不同供电电压下 ADC 是否线性
- NTC 测温误差来源
- 连续采样时 UART 打印和中断负载是否引入抖动

建议实验：

- 用万用表或基准源对比 ADC 读数
- 在不同 VDD 下记录内部 VREF 换算结果
- 连续采样并统计均值、方差、峰峰值
- 比较不同 ADC clock divider 的噪声和速度

## 7. 外部 SPI NOR 环形日志

`APP_LOG` 是非常接近产品现场诊断的方向。小 MCU 没有文件系统，也没有大 RAM，但仍然需要记录关键事件。

值得研究的问题：

- append-only ring log 的格式设计
- 一页一条记录是否适合 256 B page 的 NOR Flash
- CRC 如何覆盖 header 和 payload
- 掉电后如何扫描恢复 head/tail
- sector erase 与 page program 如何调度
- 日志写入对实时任务的影响

建议实验：

- 读取 JEDEC ID
- 写入测试记录并断电重启
- 写满 Flash 后验证环形覆盖
- 手动破坏部分 page，验证恢复逻辑
- 在日志写入期间观察 ISR 延迟是否增加

## 8. 软件 CAN 与多节点时间同步

`APP_CAN` 使用 GPIO、Timer、EXTI 实现软件 CAN MAC。这不是量产替代硬件 CAN 的方案，但非常适合理解实时通信协议的底层机制。

值得研究的问题：

- open-drain wire-OR 总线
- bit arbitration
- bit stuffing
- CRC-15
- ACK slot
- error counter / bus-off
- SOF 时间戳
- 多节点 offset + skew 校正

建议实验：

- 单板短接 TX/RX 做 loopback
- 两块板做 master/slave 同步
- 用示波器观察 SYNC 和 FUP 帧
- 改变 bitrate，寻找 ISR 延迟带来的极限
- 注入错误帧或断线，观察错误计数变化

## 9. Timer、PWM、Capture 的真实精度

Timer 研究不应停留在“能输出 PWM”，更要关注频率、分辨率、抖动和中断更新窗口。

值得研究的问题：

- PWM 频率和分辨率的取舍
- Capture 输入边沿的时间戳误差
- 中断方式更新 PWM 的最大安全频率
- 软件延时和硬件 Timer 延时的差异
- SysTick 与外设 Timer 并行时的抖动

建议实验：

- 用逻辑分析仪测 PWM 占空比和周期误差
- 用另一路 Timer capture 测 PWM 输出
- 在 UART 打印、Flash 写入、临界区压力下测 PWM 抖动
- 评估是否能支撑电机控制、红外协议、软件通信等时序任务

## 10. HAL / BSP 质量审计

小厂或低成本 MCU 的 BSP 不能完全盲信。专家级使用方式应该是：HAL 可以用，但关键路径必须能回到寄存器层解释。

值得研究的问题：

- HAL 初始化顺序是否有隐藏依赖
- 中断标志位是否正确清除
- timeout 设计是否合理
- startup weak handler 是否完整
- 寄存器头文件与手册描述是否一致
- Flash、SCU、GPIO 这类底层模块是否有必须遵守的时序

建议实验：

- 对照 `Device/TX32F01/Include/TX32F01.h` 阅读 HAL
- 用最小裸寄存器代码验证 GPIO/UART/Timer/Flash
- 对每个外设记录“必须配置步骤”和“容易遗漏的标志位”
- 对 HAL 中不确定的地方做实测，不只看注释

## 推荐研究顺序

如果按研究含金量和工程价值排序，建议顺序如下：

1. 无 VTOR Cortex-M0 Bootloader + 软向量中断转发
2. 4 KB SRAM 下 FreeRTOS vs 协作式调度器
3. 中断延迟、临界区、实时性边界
4. Flash OTA、CRC、掉电恢复
5. 低功耗 + IWDT + 唤醒可靠性
6. ADC / VREF / VDD 精度建模
7. SPI NOR 环形日志
8. 软件 CAN 和多节点时间同步
9. Timer / PWM / Capture 抖动测量
10. HAL / BSP 质量审计

## 总结

TX32F01 这颗 MCU 最值得研究的核心，不是某一个单独外设，而是：

> 极小 Cortex-M0 如何做可靠、可升级、可测量实时性的产品级固件。

当前工程已经具备很好的研究基础：bootloader、YMODEM OTA、软向量表、FreeRTOS、协作式调度器、中断延迟实验、SPI NOR 日志、软件 CAN 等模块都已经围绕这个主题展开。后续最有价值的工作，是把这些实验变成可重复测量的数据，并沉淀出这颗芯片的设计边界。
