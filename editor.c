#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdbool.h>

//macro to specify bit value of key pressed in combination with ctrl 
#define CTRL_KEY(k) ((k) &(0x1f))

//global termios struct for original terminal config
struct editorConfig{
	int screenrows;
	int screencols;
	struct termios originalTermios;
};

struct editorConfig E;

//error 
void die(const char *s){

	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

//exits raw mode by reinstating original termios struct stored in global variable
void exitRawMode(){
	if ( tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.originalTermios) == -1) die("tcsetattr");
}
/* Function to enter raw mode in temrinal. Below is a description of flags being changed
	IXON
	ICRNL
	BRKINT
	ISTRIP
	OPOST
	CS8
	ECHO
	ICANON
	IEXTEN
	ISIG
	VMIN specifies minimum imput befor reading
	VTIME specifies timeout on read() calls
	
*/
void enterRawMode(){
	struct termios raw;
	if( tcgetattr(STDIN_FILENO, &E.originalTermios) == -1 ) die("tcgetattr");
	atexit(exitRawMode);

	raw = E.originalTermios;
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	//raw.c_cc[VTIME] = 1;

	if ( tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}


void editorDrawRows(){
	for(int y = 1; y <= E.screenrows; y++){
		write(STDOUT_FILENO, "~", 1);

		if(y != E.screenrows) write(STDOUT_FILENO,"\r\n", 2);
	}
}

void editorClearScreen(){
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	editorDrawRows();
	write(STDOUT_FILENO, "\x1b[H", 3);
}

char editorReadKey(){
	int nread;
	char c;
	while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read"); 
	}
	return c;
}

void editorProcessKey(){
	char c = editorReadKey();

	switch(c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
		case CTRL_KEY('c'):
			break;
	}
}

int getWindowSize(int *rows, int *cols){
	struct winsize ws;
	if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		editorReadKey();
		return -1;
	} 
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return -1; 
	}
}

void initEditor(){
	if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("get window size"); 
}

int main(){
	enterRawMode();
	initEditor();
	while (true){
		editorClearScreen();
		editorProcessKey();
	}


	return 0;
} 
