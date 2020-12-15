#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>

#define BUF_SIZE 100
#define FREE_MESSAGE '0'
#define SYSTEM_MESSAGE '1'
#define USER_MESSAGE '2'
#define MAX_NAME_SIZE 20

typedef enum{police,mafia,docter,soldier}jobs;
typedef enum{dead,alive}life;
typedef enum{use,unuse}capacity;

typedef struct{
    int valid;      //접속해있는 사람인지 아닌지 결정하는 변수
    int first;      //처음입장했는지 아닌지 묻는 함수
    int room;
    int whisper;        //귓속말 대상을 정하는 함수
    char type;
    int mafia_num;
    int citizen_num;
    char name[MAX_NAME_SIZE];
    char message[BUF_SIZE];
    //마피아게임을 위한 변수
    char play;      //마피아게임중인지 확인
    jobs job;   
    life live;
    capacity skill;  //능력을 썼는지 유무
	
}member;

void error_handling();
void read_routine(int sock);
void write_routine(int sock);
int read_buf(int sock);

member buf;
member me;

int main(int argc, char * argv[])
{
	int sock;
	pid_t pid;
	struct sockaddr_in serv_adr;
	
	if(argc!=3)
	{
		printf("Usage : %s <IP> <PORT> \n",argv[0]);
		exit(1);
	}
	sock = socket(PF_INET, SOCK_STREAM , 0);
	if(sock == -1)
		error_handling("socket() error");
	memset(&serv_adr , 0 , sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));
	if(connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) ==-1)
		error_handling("connect() error");
	
	//*********************************************소켓 연결과 관련된 코드
	pid = fork();
	if(pid == 0)
		write_routine(sock);
		//자식은 쓰기를 담당
	else
		read_routine(sock);
		//부모는 읽기를 담당
	close(sock);
	return 0 ;
	#include<termios.h>
}
//아래코드는 서버로 부터 데이터를 받아서 buf에 넣는 코드입니다.
int read_buf(int sock){
	//printf("들어왔다.");
	int full_len =0;
	int str_len = read(sock, (char*)&buf, sizeof(member));
	full_len = str_len;
	if(str_len == 0){
		return -1;
	}
	while(full_len < sizeof(member)){
		printf("받은 바이트 수 : <%d>\n",str_len);
		str_len = read(sock,(char*)(&buf+str_len),sizeof(member));
		full_len += str_len;
	}
	//printf("나온다.");
}

//curse로 꾸밀 때 아래부분을 꾸미면 될거 같습니다.
void read_routine(int sock)
{
	//printf("사용 법은 다음과 같습니다.\n");
	//printf("/end 종료하기\n/w 이름 채팅 (귓속말)");
	while(1)
	{
		printf("\n");
		if((read_buf(sock))==-1){
			break;
		}
		//printf("buf_type : %c ",buf.type);
		//buf.type은 세종류가 있고 이는 서버에서 데이터를 보낼때 결정합니다.
		switch(buf.type){
			case FREE_MESSAGE:
				printf("%s",buf.message);
				break;
			case SYSTEM_MESSAGE:
				printf("< 시스템 알림 > : %s ",buf.message);
				break;
			case USER_MESSAGE:
				printf("[%s] : %s",buf.name,buf.message);
				break;
		}
	}
	printf("< 시스템 알림 > : 다음에 또 뵐게요!\n");
}

//또한 이 부분도 쓰는 부분으로 꾸며야 할 것입니다.
void write_routine(int sock)
{
	while(1)
	{
		fgets(buf.message, BUF_SIZE, stdin);
		buf.message[strlen(buf.message)-1] = '\0';
		
		if(buf.message[0] == '/'){	//   '/'로 시작할시 특수 이벤트를 넣기 위해 분리했습니다. 아이디어 주시면 감사합니다.
			buf.type=SYSTEM_MESSAGE;
			if(!strcmp(buf.message,"/end")){	//이 if문은 연결을 닫는 코드로 크게 신경 안쓰셔도 됩니다.
				write(sock, (char*)&buf, sizeof(member));
				shutdown(sock , SHUT_WR);
			}
			write(sock, (char*)&buf, sizeof(member));
		}
		else{
			//이부분은 일반 채팅 부분입니다.
			buf.type = USER_MESSAGE;
			write(sock, (char*)&buf, sizeof(member));
		}
	}
}

void error_handling(char * message)
{
	fputs(message, stderr);
	fputc('\n',stderr);
	exit(1);
}

