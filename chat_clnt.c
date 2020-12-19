/*

gcc filename.c -lncursesw -lpthread -lcurses -o filename

--------------------------------------------------------------
|		win_read											|
|	-----------------------------------------				|
|	|										|				|
|	|										|				|
|	|	win_msg_scroll						|				|
|	|										|				|
|	|										|				|
|	|										|				|
|	----------------------------------------				|
|															|
--------------------------------------------------------------
--------------------------------------------------------------
|		win_write											|
|	-----------------------------------------				|
|	|	win_input_str						|				|
|	------------------------------------------				|
|															|
--------------------------------------------------------------

*/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<termios.h>
#include<locale.h>
#include<curses.h>
#include<ncursesw/ncurses.h>
#include<signal.h>
#include<pthread.h>

#define BUF_SIZE 100
#define MAX_NAME_SIZE 20

#define FREE_MESSAGE '0'
#define SYSTEM_MESSAGE '1'
#define USER_MESSAGE '2'
#define WHISPER_MESSAGE '3'
#define MAFIA_MESSAGE '4'

#define SYSTEM_COLOR 1
#define WHISPER_COLOR 3

typedef enum { police, mafia, docter, soldier }jobs;
typedef enum { dead, alive }life;
typedef enum { use, unuse }capacity;

typedef struct
{
	char valid; //접속해있는 사람인지 아닌지 결정하는 변수
	char first; //처음입장했는지 아닌지 묻는 함수
	char room;
	char type;
	int mafia_num;
	int citizen_num;
	char name[MAX_NAME_SIZE];
	char message[BUF_SIZE];
	//마피아게임을 위한 변수
	char play; //마피아게임중인지 확인
	jobs job;
	life live;
	capacity skill; //능력을 썼는지 유무
	char vote_num;   //투표를 얼만큼 받았는지 설정
	char skill_target; //스킬을 누구에게 쓸건지 정하는 함수
} member;
//사용자 변수

void error_handling();
void read_routine(void *socket);
void write_routine(void *socket);
int read_buf(int sock);

member buf;
member me;

WINDOW *win_read, *win_msg_scroll, *win_write, *win_input_str;
char temp[BUF_SIZE];	//buf.message에다가 바로 저장하면 메세지를 입력하는 도중 다른 사용자로부터 채팅이 날라오면 저장된 값이 바뀌기 때문에 temp사용

int main(int argc, char * argv[])
{
	int sock;
	pthread_t read_thread, write_thread;
	struct sockaddr_in serv_adr;

	if (argc != 3)
	{
		printf("Usage : %s <IP> <PORT> \n", argv[0]);
		exit(1);
	}
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		error_handling("socket() error");
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));
	if (connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("connect() error");

	//*********************************************소켓 연결과 관련된 코드

	setlocale(P_ALL, "ko_KR.utf8");				//ncurses에서 한글을 사용하기 위함
	initscr();

	use_default_colors();
	start_color();
	init_pair(SYSTEM_COLOR, COLOR_RED, -1);
	init_pair(WHISPER_COLOR, COLOR_YELLOW, -1);

	win_read = newwin(LINES - 8, COLS - 2, 0, 0);			//window 생성
	win_msg_scroll = newwin(LINES - 10, COLS - 8, 1, 2);
	win_write = newwin(5, COLS - 2, LINES - 8, 0);
	win_input_str = newwin(1, COLS - 6, LINES - 6, 2);

	scrollok(win_msg_scroll, TRUE);
	box(win_read, 0, 0);
	box(win_write, 0, 0);

	refresh();
	wrefresh(win_msg_scroll);
	wrefresh(win_read);
	wrefresh(win_write);
	wrefresh(win_input_str);

	signal(SIGINT, SIG_IGN);							//signal 무시
	signal(SIGQUIT, SIG_IGN);

	memset(&buf, 0, sizeof(buf));

	pthread_create(&read_thread, NULL, (void *)read_routine, (void *)&sock);		//쓰레드 사용
	pthread_create(&write_thread, NULL, (void *)write_routine, (void *)&sock);
	pthread_join(read_thread, NULL);
	pthread_join(write_thread, NULL);

	close(sock);
	/*
	getch();							//window 삭제 -> 여기도??
	delwin(win_msg_scroll);
	delwin(win_read);
	delwin(win_write);
	delwin(win_input_str);
	endwin();
	*/
	return 0;

}
//아래코드는 서버로 부터 데이터를 받아서 buf에 넣는 코드입니다.
int read_buf(int sock) {
	//printf("들어왔다.");
	int full_len = 0;
	int str_len = read(sock, (char*)&buf, sizeof(member));
	full_len = str_len;
	if (str_len == 0) {
		return -1;
	}
	while (full_len < sizeof(member)) {
		str_len = read(sock, (char*)&buf + str_len, sizeof(member));
		full_len += str_len;
	}
	//printf("나온다.");
}

//curse로 꾸밀 때 아래부분을 꾸미면 될거 같습니다.
void read_routine(void *socket)
{
	int sock = *((int *)socket);
	while (1)
	{
		if ((read_buf(sock)) == -1) {
			break;
		}
		/*
		if((read_buf(sock))==0){		//상대측이 소켓을 닫았을 때 종료함을 말함
			break;
		}*/
		//printf("buf_type : %c ",buf.type);
		//buf.type은 세종류가 있고 이는 서버에서 데이터를 보낼때 결정합니다.
		switch (buf.type) {
		case FREE_MESSAGE:
			wprintw(win_msg_scroll, "%s\n", buf.message);
			break;
		case SYSTEM_MESSAGE:
			wattron(win_msg_scroll, COLOR_PAIR(SYSTEM_COLOR));
			wprintw(win_msg_scroll, "< 시스템 알림 > : %s\n", buf.message);
			wattroff(win_msg_scroll, COLOR_PAIR(SYSTEM_COLOR));
			break;
		case USER_MESSAGE:
			wprintw(win_msg_scroll, "[%s] : %s\n", buf.name, buf.message);
			break;
		case WHISPER_MESSAGE:
			wattron(win_msg_scroll, COLOR_PAIR(WHISPER_COLOR));
			wprintw(win_msg_scroll, "[%s] : /w %s\n", buf.name, buf.message);
			wattroff(win_msg_scroll, COLOR_PAIR(WHISPER_COLOR));
			break;
		case MAFIA_MESSAGE:
			wclear(win_msg_scroll);
			wattron(win_msg_scroll, COLOR_PAIR(SYSTEM_COLOR));
			wprintw(win_msg_scroll, "< 시스템 알림 > : /w %s\n", buf.message);
			wattroff(win_msg_scroll, COLOR_PAIR(SYSTEM_COLOR));
		default:
			break;
		}

		wrefresh(win_msg_scroll);

		wmove(win_input_str, strlen(temp), 0);	//원래 메세지 쓰던 위치로 돌아가야함
		wrefresh(win_input_str);
	}

	//종료 후 입력창 비움
	wmove(win_input_str, 0, 0);
	wclrtobot(win_input_str);
	wrefresh(win_input_str);
}

//또한 이 부분도 쓰는 부분으로 꾸며야 할 것입니다.
void write_routine(void *socket)
{
	int sock = *((int *)socket);
	while (1)
	{
		wgetstr(win_input_str, temp);
		strncpy(buf.message, temp, BUF_SIZE);
		if (buf.message[0] == '/') {	//   '/'로 시작할시 특수 이벤트를 넣기 위해 분리했습니다. 아이디어 주시면 감사합니다.
			buf.type = SYSTEM_MESSAGE;
			if (!strcmp(buf.message, "/end")) {	//이 if문은 연결을 닫는 코드로 크게 신경 안쓰셔도 됩니다.
				write(sock, (char*)&buf, sizeof(member));

				getch();							//여기서 window 삭제 -> main에서도 삭제?
				delwin(win_msg_scroll);
				delwin(win_read);
				delwin(win_write);
				delwin(win_input_str);
				endwin();

				shutdown(sock, SHUT_WR);
			}
			write(sock, (char*)&buf, sizeof(member));
		}
		else {
			//이부분은 일반 채팅 부분입니다.
			buf.type = USER_MESSAGE;
			write(sock, (char*)&buf, sizeof(member));
		}

		//입력 후 입력창 비움
		wmove(win_input_str, 0, 0);
		wclrtobot(win_input_str);
		wrefresh(win_input_str);
	}
}

void error_handling(char * message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
