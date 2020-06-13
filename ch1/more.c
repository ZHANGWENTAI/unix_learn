#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <curses.h>
#include <term.h>
#include <string.h>
#include <sys/stat.h>

#define LINELEN 512

static unsigned long filesize = 0;
static unsigned long input_filesize = 0;
static FILE *out_stream = NULL;
static int filesum = 0;

int cols_more(FILE *fp);						// 获取终端列数
int rows_more(FILE *fp); 						// 获取终端行数
int char_to_terminal(int num);					// 调用 tputs 函数

void clear_more(int x, int y, FILE *fp);
void do_more(FILE *fp, char *filename);
int see_more(FILE *cmd, int pagelen, int add_line);

unsigned long get_filesize(FILE *);

int main(int ac, char *av[]) {
	FILE *fp;
	filesum = ac - 1;
	if (ac == 1) {
		do_more(stdin, NULL);
	} else {
		while (--ac) {
			if((fp = fopen(* ++av, "r")) != NULL) {
				filesize = 0;
				input_filesize = 0;
				filesize = get_filesize(fp);
				do_more(fp, *av);
				fclose(fp);
			} else {
				exit(1);
			}
		}
	}
	
	return 0;
}

void do_more(FILE *fp, char * filename) 
/*
 * read PAGELEN lines, then call see_more() for further instructions
 */
{
	char line[LINELEN];
	int num_of_lines = 0;
	int reply;
	FILE *fp_tty_in, *fp_tty_out;
	fp_tty_in = fopen("/dev/tty", "r");
	fp_tty_out = fopen("/dev/tty", "w");

	if (fp_tty_in == NULL || fp_tty_out == NULL) {
		exit(1);
	}
		
	struct termios new_setting, init_setting;
	tcgetattr(fileno(fp_tty_in), &init_setting);  // 获取终端的当前属性
	new_setting = init_setting;

	new_setting.c_lflag &= ~ICANON; // 非标准模式
	new_setting.c_lflag &= ~ECHO;
	new_setting.c_cc[VMIN] = 1;		// 读入1个字符，立即返回字符
	new_setting.c_cc[VTIME] = 0;

	if (tcsetattr(fileno(fp_tty_in), TCSANOW, &new_setting) != 0)  //设置新属性
		fprintf(stderr, "Could not set terminal attributes\n");

	int Pagelen = rows_more(fp_tty_in) - 1; // 终端行数
    int PageCol = cols_more(fp_tty_in);     // 终端列数
    int add_line;
    if(filesum > 1) // 显示的文件个数 > 1 那么把文件名也显示出来。
    {
        fprintf(fp_tty_out, "-------> %s <------- \n", filename);
        num_of_lines = 1;
    }

	while (fgets(line, LINELEN, fp) != NULL) {   // 按行取字符
		if (num_of_lines >= Pagelen) {
			reply = see_more(fp_tty_in, Pagelen, add_line);
			int prePage = Pagelen;
			Pagelen = rows_more(fp_tty_in) - 1;  // 终端行数
            PageCol = cols_more(fp_tty_in);      // 终端列数
			if (prePage < Pagelen)
				clear_more(Pagelen - 1, 0, fp_tty_out);
			else 
				clear_more(Pagelen, 0, fp_tty_out);
			if (reply == 0) 
				break;
			if (num_of_lines != Pagelen && reply == 1) // 当终端变大时，且按下时回车“enter”，应把number_line改为终端倒数第二行
				num_of_lines = Pagelen - 1;
			else 
				num_of_lines -= reply;
		}
		if (fputs(line, fp_tty_out) == EOF) {
			tcsetattr(fileno(fp_tty_in), TCSANOW, &init_setting);
			exit(1);
		}
		int line_len = strlen(line);
		input_filesize += (unsigned long) line_len;
		add_line = (line_len + PageCol - 1) / PageCol;
		num_of_lines += add_line;
	}

	tcsetattr(fileno(fp_tty_in), TCSANOW, &init_setting);
}
	
int see_more(FILE *cmd, int pagelen, int add_line) 
/* 
 * print message, wait for response, return # of lines to advance
 * q means no, space means yes, CR means one line
 */
{
	int c;
    if(filesize > 0 ) // 如果重定向的输入无法获取大小，则不要显示百分比。
        printf("\033[7m --more-- \033[m %lu%%", input_filesize * 100 / filesize);
    else
        printf("\033[7m --more-- \033[m ");

	while ((c = fgetc(cmd)) != EOF) {
		if (c == 'q') return 0;
		if (c == ' ') return pagelen;
		if (c == '\n' || c == '\r') // 非标准模式下，默认回车和换行之间的映射已不存在，所以检查回车符'\r'
			return add_line;
	}
	return 0;
}

void clear_more(int x, int y, FILE *fp) {
	char *el;
	char *cursor;
	out_stream = fp;
	cursor = tigetstr("cup");
	el = tigetstr("el");
	tputs(tparm(cursor, x, y), 1, char_to_terminal);

	tputs(el, 1, char_to_terminal);
}


int char_to_terminal(int num) {
    if(out_stream) putc(num, out_stream);
    return 0;
}

int rows_more(FILE *fp) {
    int nrows;
    setupterm(NULL, fileno(fp), (int *)0);
    nrows = tigetnum("lines");
    return nrows;
}

int cols_more(FILE *fp) {
    int ncols;
    setupterm(NULL, fileno(fp), (int *)0);
    ncols = tigetnum("cols");
    return ncols;
}

unsigned long get_filesize(FILE *fp) {
    struct stat buf;
    if(fstat(fileno(fp), &buf) < 0)
        return (unsigned long) 0;
    return (unsigned long) buf.st_size;
}
