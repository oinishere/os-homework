#include "os.h"

/*
 * ref: https://github.com/oinishere/os-homework.git
 */

static int _vsnprintf(char * out, size_t n, const char* s, va_list vl)
{
	int format = 0;
	int longarg = 0;
	size_t pos = 0;
	for (; *s; s++) {
		if (format) {
			switch(*s) {
			case 'l': {
				longarg = 1;
				break;
			}
			case 'p': {
				longarg = 1;
				if (out && pos < n) {
					out[pos] = '0';
				}
				pos++;
				if (out && pos < n) {
					out[pos] = 'x';
				}
				pos++;
			}
			case 'x': {
				long num = longarg ? va_arg(vl, long) : va_arg(vl, int);
				int hexdigits = 2*(longarg ? sizeof(long) : sizeof(int))-1;
				for(int i = hexdigits; i >= 0; i--) {
					int d = (num >> (4*i)) & 0xF;
					if (out && pos < n) {
						out[pos] = (d < 10 ? '0'+d : 'a'+d-10);
					}
					pos++;
				}
				longarg = 0;
				format = 0;
				break;
			}
			case 'd': {
				long num = longarg ? va_arg(vl, long) : va_arg(vl, int);
				if (num < 0) {
					num = -num;
					if (out && pos < n) {
						out[pos] = '-';
					}
					pos++;
				}
				long digits = 1;
				for (long nn = num; nn /= 10; digits++);
				for (int i = digits-1; i >= 0; i--) {
					if (out && pos + i < n) {
						out[pos + i] = '0' + (num % 10);
					}
					num /= 10;
				}
				pos += digits;
				longarg = 0;
				format = 0;
				break;
			}
			case 's': {
				const char* s2 = va_arg(vl, const char*);
				while (*s2) {
					if (out && pos < n) {
						out[pos] = *s2;
					}
					pos++;
					s2++;
				}
				longarg = 0;
				format = 0;
				break;
			}
			case 'c': {
				if (out && pos < n) {
					out[pos] = (char)va_arg(vl,int);
				}
				pos++;
				longarg = 0;
				format = 0;
				break;
			}
			default:
				break;
			}
		} else if (*s == '%') {
			format = 1;
		} else {
			if (out && pos < n) {
				out[pos] = *s;
			}
			pos++;
		}
    	}
	if (out && pos < n) {
		out[pos] = 0;
	} else if (out && n) {
		out[n-1] = 0;
	}
	return pos;
}

static char out_buf[1000]; // buffer for _vprintf()

static int _vprintf(const char* s, va_list vl)
{
	int res = _vsnprintf(NULL, -1, s, vl);
	if (res+1 >= sizeof(out_buf)) {
		uart_puts("error: output string size overflow\n");
		while(1) {}
	}
	_vsnprintf(out_buf, res + 1, s, vl);
	uart_puts(out_buf);
	return res;
}

static char in_buf[1000]; // save
static char in_buf_last;
static int has_last ;

static int _oinscanf(int mode)// 0 = int , 1 = string
{
    char hi;
    size_t pos = 0;

	if(has_last) 
	{
        in_buf[pos++] = in_buf_last;
		has_last = 0;
    } 
	
    
	while( 1 ) 
	{
		hi = uart_getc();
		if(hi == '\r' || hi == '\n' || hi == ' ' || hi == '\t') 
		{

		}
		else
		{
			in_buf[pos] = hi;
			pos ++;
			break;
		}
	}


    while (1) {
        hi = uart_getc();
        if(hi == '\r' || hi == '\n' || hi == ' ' || hi == '\t') 
		{

            in_buf[pos] = '\0';
            break;
        }

		if(mode == 0)
		{
			if(( hi > '9' || hi < '0') )
			{
				in_buf_last = hi;
				has_last = 1;
				in_buf[pos] = '\0';
				break;
			}
		}

        in_buf[pos] = hi;
		pos ++;
    }

	if(pos >= 1000)
	{
		kprintf("too long");
	}

	return pos;
}



int kprintf(const char* s, ...)
{
	int res = 0;
	va_list vl;
	va_start(vl, s);
	res = _vprintf(s, vl);
	va_end(vl);
	return res;
}



// input
// +
// %d 
// 
// &a

int kscanf(const char* s, ...)
{
	int res = 0;

	// & 的變數
	va_list var;
	va_start(var, s);


	
	char* format = (char*)s;
	for( ; *format ; format ++ )
	{

		if(*format == '%')
		{
			// 讀的東西 -> in_buf

			format ++;
			if(*format == 'd')
			{
				int inputSize = _oinscanf(0);
				int inputIdx = 0; 
				int* curp = va_arg(var, int*);

				inputIdx = 0; 
				int cur = 0;
				int negative = 1;

				if(in_buf[inputIdx] == '-')
				{
					inputIdx ++;
					cur = 0;
					negative = -1;
				}

				for(  ; inputIdx < inputSize ; inputIdx ++)
				{
					if(in_buf[inputIdx] < '0' || in_buf[inputIdx] > '9')
					{
						kprintf("no!\n");
					}
					cur *= 10;
					cur += in_buf[inputIdx] - '0';
				}
				
				*curp = cur * negative;
				res ++;
				
			}
			else if(*format == 's')
			{
				int inputSize = _oinscanf(1);
				int inputIdx = 0; 
				char* curp = va_arg(var, char*);


				inputIdx = 0; 

				for(  ; inputIdx < inputSize ; inputIdx ++)
				{
					*curp = in_buf[inputIdx];
					curp ++;
				}

				*curp = '\0';
				res ++;
				
			}
			else
			{
				kprintf("no\n");
			}
		}
		else
		{
			continue;
		}



	}



	va_end(var);

	return res;
}

void panic(char *s)
{
	kprintf("panic: ");
	kprintf(s);
	kprintf("\n");
	while(1){};
}
