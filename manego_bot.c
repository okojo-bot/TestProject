#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>

double komi = 6.5;

#define B_SIZE     9
#define WIDTH      (B_SIZE + 2)
#define BOARD_MAX  (WIDTH * WIDTH)

int board[BOARD_MAX] = {
  3,3,3,3,3,3,3,3,3,3,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,0,0,0,0,0,0,0,0,0,3,
  3,3,3,3,3,3,3,3,3,3,3
};

int dir4[4] = { +1,+WIDTH,-1,-WIDTH };
int dir8[8] = { +1,+WIDTH,-1,-WIDTH,  +1+WIDTH, +WIDTH-1, -1-WIDTH, -WIDTH+1 };
int ko_z;

#define MAX_MOVES 1000
int record[MAX_MOVES];
int moves = 0;

int get_z(int x,int y)
{
    return y*WIDTH + x;
}

int get81(int z)
{
    int y = z / WIDTH;
    int x = z - y*WIDTH;
    if ( z==0 ) return 0;
    return x*10 + y;
}

char *get_char_z(int z)
{
    int x,y,ax;
    static char buf[16];
    sprintf(buf,"pass");
    if ( z==0 ) return buf;
    y = z / WIDTH;
    x = z - y*WIDTH;
    ax = x-1+'A';
    if ( ax >= 'I' ) ax++;
    sprintf(buf,"%c%d",ax,B_SIZE+1 - y);
    return buf;
}

int flip_color(int col)
{
    return 3 - col;
}

void prt(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf( stderr, fmt, ap );
    va_end(ap);
}

void send_gtp(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf( stdout, fmt, ap );
    va_end(ap);
}

int check_board[BOARD_MAX];

void count_liberty_sub(int tz, int color, int *p_liberty, int *p_stone)
{
    int z,i;

    check_board[tz] = 1;
    (*p_stone)++;
    for (i=0;i<4;i++) {
        z = tz + dir4[i];
        if ( check_board[z] ) continue;
        if ( board[z] == 0 ) {
            check_board[z] = 1;
            (*p_liberty)++;
        }
        if ( board[z] == color ) count_liberty_sub(z, color, p_liberty, p_stone);
    }
}

void count_liberty(int tz, int *p_liberty, int *p_stone)
{
    int i;
    *p_liberty = *p_stone = 0;
    for (i=0;i<BOARD_MAX;i++) check_board[i] = 0;
    count_liberty_sub(tz, board[tz], p_liberty, p_stone);
}

void take_stone(int tz,int color)
{
    int z,i;

    board[tz] = 0;
    for (i=0;i<4;i++) {
        z = tz + dir4[i];
        if ( board[z] == color ) take_stone(z,color);
    }
}

const int FILL_EYE_ERR = 1;
const int FILL_EYE_OK  = 0;

int put_stone(int tz, int color, int fill_eye_err)
{
    int around[4][3];
    int un_col = flip_color(color);
    int space = 0;
    int wall  = 0;
    int mycol_safe = 0;
    int capture_sum = 0;
    int ko_maybe = 0;
    int liberty, stone;
    int i;

    if ( tz == 0 ) { ko_z = 0; return 0; }  // pass

    for (i=0;i<4;i++) {
        int z, c, liberty, stone;
        around[i][0] = around[i][1] = around[i][2] = 0;
        z = tz+dir4[i];
        c = board[z];  // color
        if ( c == 0 ) space++;
        if ( c == 3 ) wall++;
        if ( c == 0 || c == 3 ) continue;
        count_liberty(z, &liberty, &stone);
        around[i][0] = liberty;
        around[i][1] = stone;
        around[i][2] = c;
        if ( c == un_col && liberty == 1 ) { capture_sum += stone; ko_maybe = z; }
        if ( c == color  && liberty >= 2 ) mycol_safe++;
    }

    if ( capture_sum == 0 && space == 0 && mycol_safe == 0 ) return 1; // suicide
    if ( tz == ko_z                                        ) return 2; // ko
    if ( wall + mycol_safe == 4 && fill_eye_err            ) return 3; // eye
    if ( board[tz] != 0                                    ) return 4;

    for (i=0;i<4;i++) {
        int lib = around[i][0];
        int c   = around[i][2];
        if ( c == un_col && lib == 1 && board[tz+dir4[i]] ) {
            take_stone(tz+dir4[i],un_col);
        }
    }

    board[tz] = color;

    count_liberty(tz, &liberty, &stone);
    if ( capture_sum == 1 && stone == 1 && liberty == 1 ) ko_z = ko_maybe;
    else ko_z = 0;
    return 0;
}

void print_board()
{
    int x,y;
    const char *str[4] = { ".","X","O","#" };

    prt("   ");
    for (x=0;x<B_SIZE;x++) prt("%c",'A'+x+(x>7));
    prt("\n");
    for (y=0;y<B_SIZE;y++) {
        prt("%2d ",B_SIZE-y);
        for (x=0;x<B_SIZE;x++) {
            prt("%s",str[board[get_z(x+1,y+1)]]);
        }
        if ( y==4 ) prt("  ko_z=%d,moves=%d",get81(ko_z), moves);
        prt("\n");
    }
}

void init_board()
{
    int i,x,y;
    for (i=0;i<BOARD_MAX;i++) board[i] = 3;
    for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) board[get_z(x+1,y+1)] = 0;
    moves = 0;
    ko_z = 0;
}

int OK = 0;
int NG = 1;

int add_moves(int z, int color)
{
    int err = put_stone(z, color, FILL_EYE_OK);
    if ( err != 0 ) {
         prt("Err!\n");
         return NG; 
        }
    record[moves] = z;
    moves++;
    print_board();
    
    return OK;
}

#define STR_MAX 256
#define TOKEN_MAX 3

int main()
{
    char str[STR_MAX];
    char sa[TOKEN_MAX][STR_MAX];
    char seps[] = " ";
    char *token;
    const char *str_board[4] = { ".","X","O","#" };

    int x,y,z,ax, count;
	int mane_x = 0;
    int mane_y = 0;
    int passflag = 0;
    int resign_flag = 0;
    int chk_add_move = 0;

    srand( (unsigned)time( NULL ) );
    init_board();

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    for (;;) {
        if ( fgets(str, STR_MAX, stdin)==NULL ) break;
        count = 0;
        token = strtok( str, seps );
        while ( token != NULL ) {
            strcpy(sa[count], token);
            count++;
            if ( count == TOKEN_MAX ) break;
            token = strtok( NULL, seps );
        }

        if ( strstr(sa[0],"boardsize")     ) {
            send_gtp("= \n\n");
        } else if ( strstr(sa[0],"clear_board")   ) {
            init_board();
            send_gtp("= \n\n");
        } else if ( strstr(sa[0],"quit") ) {
            break;
        } else if ( strstr(sa[0],"protocol_version") ) {
            send_gtp("= 2\n\n");
        } else if ( strstr(sa[0],"name")          ) {
            send_gtp("= MANEGO_BOT\n\n");
        } else if ( strstr(sa[0],"version")       ) {
            send_gtp("= 1.1.1\n\n");
        } else if ( strstr(sa[0],"list_commands" ) ) {
            send_gtp("= boardsize\nclear_board\nquit\nprotocol_version\n"
                     "name\nversion\nlist_commands\nkomi\ngenmove\nplay\n\n");
        } else if ( strstr(sa[0],"komi") ) {
            komi = atof( sa[1] );
            send_gtp("= \n\n");
        } else if ( strstr(sa[0],"genmove") ) {
            int color = 1;
            if ( tolower(sa[1][0])=='w' ) color = 2;

            prt("passflag:%d\n",passflag);
            prt("resign_flag:%d\n",resign_flag);
            x = (B_SIZE + 1)/2;
            y = (B_SIZE + 1)/2;

            /**********************************/
            /* 天元の石が取られたら、投了する */
            /**********************************/
            if(moves != 0 &&
               color == 1 &&
               (str_board[board[get_z(x,y)]] ==".")){

                /* 次の対局に備え変数を初期化する */
                mane_x =0;
                mane_y =0;
                passflag = 0;

                send_gtp("= resign\n\n");
            }

            /******************************/
            /* 天元に打たれたら、投了する */
            /******************************/
            else if(mane_x == (B_SIZE + 1)/2 &&
               mane_y == (B_SIZE + 1)/2){

                /* 次の対局に備え変数を初期化する */
                mane_x =0;
                mane_y =0;
                passflag = 0;

                send_gtp("= resign\n\n");
            }

            /**********************/
            /* パスにはパスで返す */
            /**********************/
            else if(passflag == 1){

                z = 0;

                /* 次の対局に備え変数を初期化する */
                mane_x =0;
                mane_y =0;
                passflag = 0;

                prt("z:%d\n",z);

                send_gtp("= %s\n\n",get_char_z(z));
            }

            /**********************/
            /* 返せない着手は投了 */
            /**********************/
            else if(resign_flag == 1){
                /* 次の対局に備え変数を初期化する */
                mane_x =0;
                mane_y =0;
                passflag = 0;
                resign_flag = 0;
                send_gtp("= resign\n\n");
            }
            else{

                x = (B_SIZE + 1)/2;
                y = (B_SIZE + 1)/2;

                if(moves != 0){
                    x = mane_x;
                    y = mane_y;
                }

                z = get_z(x, B_SIZE-y+1);
                chk_add_move = add_moves(z, color);
                if (chk_add_move == NG) {

                    prt("add_moves NG!!\n");
                    send_gtp("= resign\n\n");
                }
                else{
                    /* COMの着手用 */
                    prt("COMの着手\n");
                    prt("passflag:%d\n",passflag);
                    prt("x:%d\n",x);
                    prt("y:%d\n",y);
                    prt("z:%d\n",z);
                    prt("mane_x:%d\n",mane_x);
                    prt("mane_y:%d\n",mane_y);

                    send_gtp("= %s\n\n",get_char_z(z));
                }
            }

        } else if ( strstr(sa[0],"play") ) {
            int color = 1;
            passflag = 0;

            if ( tolower(sa[1][0])=='w' ) color = 2;
            ax = tolower( sa[2][0] );
            x = ax - 'a' + 1;
            if ( ax >= 'i' ) x--;
            y = atoi(&sa[2][1]);
            z = get_z(x, B_SIZE-y+1);

            /* 相手がパスした場合 */
            if ( tolower(sa[2][0])=='p' ){

                z = 0;  // pass
                passflag = 1;
            }

            chk_add_move = add_moves(z, color);
            if (chk_add_move == NG) {
                    resign_flag	= 1;
                    prt("add_moves NG!\n");
                    prt("resign_flag:%d\n",resign_flag);
            }


            /* 人間の着手用 */
            prt("人間の着手\n");
            prt("x:%d\n",x);
            prt("y:%d\n",y);
            prt("z:%d\n",z);

            mane_x = B_SIZE -x +1;
            mane_y = B_SIZE -y +1;
            prt("mane_x:%d\n",mane_x);
            prt("mane_y:%d\n",mane_y);

            send_gtp("= \n\n");
        } else {
            send_gtp("? unknown_command\n\n");
        }
    }
    return 0;
}
