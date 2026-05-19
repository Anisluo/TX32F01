#include <stdint.h>

/* 这些符号名必须和你的 .sct 区域名一致！
   不一致就改成和 .sct 相同的名字（或反过来改 .sct）。*/
extern uint32_t Image$$RW_IRAM1$$RW$$Base, Image$$RW_IRAM1$$RW$$Limit;
extern uint32_t Load$$RW_IRAM1$$RW$$Base;
extern uint32_t Image$$RW_IRAM1$$ZI$$Base, Image$$RW_IRAM1$$ZI$$Limit;

static void crt_min_init(void){
  uint32_t *s = &Load$$RW_IRAM1$$RW$$Base;
  uint32_t *d = &Image$$RW_IRAM1$$RW$$Base;
  while (d < &Image$$RW_IRAM1$$RW$$Limit) { *d++ = *s++; }
  d = &Image$$RW_IRAM1$$ZI$$Base;
  while (d < &Image$$RW_IRAM1$$ZI$$Limit) { *d++ = 0u; }
}


int app_main(void){
	while(1){
		
	

		
	}
	
}


/* 启动入口（startup.s 会跳到它）*/
void __mymain(void){
  crt_min_init();            // 最小C运行时初始化：拷data、清bss
  (void)app_main();          // 进你的主逻辑
  for(;;){}                  // 避免返回
}
