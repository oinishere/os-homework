#include "os.h"

void start_kernel(void)
{
	uart_init();
	uart_puts("Hello, RVOS!\n");

	int i = 123;
	kprintf("i=%d\n",i);

	// while(1) 
	// {
	// 	char hi = uart_getc();
	// 	if(hi == '\r' || hi == '\n')kprintf("\n");
	// 	else if(hi == 0)kprintf("");
	// 	else kprintf("%c",hi);
	// 	hi = 0;
	// }

	while(1)
	{
		int a = 0;
		char b[100];
		int c = 0;


		kscanf("%d %s %d",&a,b ,&c);
		kprintf("a=%d b=%s c=%d\n",a,b,c);


	}

}


