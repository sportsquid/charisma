#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdbool.h>

//macro to specify bit value of key pressed in combination with ctrl
#define CTRL_KEY(k) ((k) &(0x1f))

#define CHARISMA_VERSION "0.0.1"

//Append Buffer macro
#define ABUF_INIT {NULL, 0}

//global termios struct for original terminal config
struct editorConfig{
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios originalTermios;
};

struct editorConfig E;

struct abuf {
    char *b;
    int len;
};

void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab){
    free(ab->b);
}

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
    raw.c_cc[VTIME] = 1;

    if ( tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}


void editorDrawRows(struct abuf *ab){
    int y;
    for(y = 1; y <= E.screenrows; y++){
        if(y == E.screenrows/3){
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Charisma editor -- Version %s", CHARISMA_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols- welcomelen)/2;
            if(padding >0){
                abAppend(ab, "~", 1);
                padding--;
            }
            while(padding > 0 ){
                abAppend(ab, " ", 1);
                padding--;
            }
            abAppend(ab, welcome, welcomelen);
        } else{
            abAppend(ab, "~", 1);

        }


        abAppend(ab, "\x1b[K", 3); //why is this needed? deletes row

        if(y != E.screenrows) abAppend(ab, "\r\n", 2);
    }
}

void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); //hide cursor
    abAppend(&ab, "\x1b[H", 3); //move cursor (to 0,0?)

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy+1, E.cx+1); //move cursor to stored position
    abAppend(&ab, buf, strlen(buf));


    abAppend(&ab, "\x1b[?25h", 6); //show cursor
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
void editorMoveCursor(char key){
    switch(key){
        case 'w':
            E.cy--;
            break;
        case 'a':
            E.cx--;
            break;
        case 's':
            E.cy++;
            break;
        case 'd':
            E.cx++;
            break;
    }
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
        case 'w':
        case 'a':
        case 's':
        case 'd':
            editorMoveCursor(c);
            break;
    }
}
//returns -1 if an error occurs; 0 otherwise. reads in response from terminal after requesting cursor position with \x1b[6n
int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while(i < sizeof(buf) - 1){
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;

}
//returns -1 if an error occurs; 0 otherwise.
int getWindowSize(int *rows, int *cols){
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);

    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void initEditor(){
    E.cx = 10;
    E.cy = 10;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("get window size");
}

int main(){
    enterRawMode();
    initEditor();
    while (true){
        editorRefreshScreen();
        editorProcessKey();
    }
    return 0;
}
