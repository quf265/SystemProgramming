/*
*   시스템프로그래밍 채팅서버
*   select기반 멀티플렉싱 서버, 룸 구현
*   더한다면 마피아게임으로 바꿀 수 있음
*   10조 : 신현석, 한지예, 정재희, Jose
*   
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUF_SIZE 100
#define MAX_MEMBER 100
#define MAX_NAME_SIZE 20
#define MAX_ROOM 100
#define EMPTY 0
#define FULL 1

#define FREE_MESSAGE '0'
#define SYSTEM_MESSAGE '1'
#define USER_MESSAGE '2'
//free는 메세지만 보내고 싶을때
//sysmtem은 system출력하고 싶을때
//user은 사용자이름 출력하고 싶을 때 사용한다.

void error_handling(char *buf);
typedef struct
{
    int valid;
    int first; //처음입장했는지 아닌지 묻는 함수
    int room;
    char type;
    char name[MAX_NAME_SIZE];
    char message[BUF_SIZE];
} member;

member member_list[MAX_MEMBER];
//사용자를 담는 코드
int member_num;

void new_member(int num);
void send_message(member buf, char type, int dest);
int alreay_print_room(int *room_list, int room_num, int fill_num);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    //소켓 설정을 위한 소켓 생성
    struct sockaddr_in serv_adr, clnt_adr;
    //서버쪽 주소와 클라이언트쪽 주소를 저장하기위한 구조체
    struct timeval timeout;
    //서버가 무한정 블로킹상태에 빠지지 않기 위한 timeout(연결을 기다릴때 계속 가만히 있을 수 있다.)
    fd_set reads, cpy_reads;
    //이건 현재 연결되어 있는 사용자를 비트로 표현하는 것이다. (1이면 연결 0 이면 비었음)
    //multiplexing을 위한 변수들이다.
    //TEST wdawfionawifnoaiegnoiafmajne Please delete this one using git
    //And this one
    socklen_t adr_sz;
    //주소 크기
    int fd_max, str_len, fd_num, i;
    int room_list[MAX_ROOM];

    member buf;
    //char buf[sizeof(member)];
    //이건 입력받는 버퍼이다.

    char buf_temp[BUF_SIZE];
    int room_check = 0;
    char message_type[10];
    
	if(argc != 2)
	{
		printf("Usage : %s <PORT> \n",argv[0]);
		exit(1);
	}
    //포트임의로 넣기위해서

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    //소켓을 생성한다. PF_INET은 IPv4을 의미
    //SOCK_STREAM은 TCP를 의미한다./*
    //0은 옵션으로 무시해도 됨
    if (serv_sock == -1)
        error_handling("socket() error");
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));
    //서버쪽 주소를 채우는 과정이다.

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    // bind는 이제 소켓에 주소를 할당하는 과정이다.
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");
    //listen은 이제 소켓이 들을 수 있음을 말한다. 즉 연결요청이 오면 들을 수 있다.

    FD_ZERO(&reads);
    //아무것도 연결안되어 있으니까 전부 0으로 초기화하는 매크로함수
    FD_SET(serv_sock, &reads);
    //서버 소켓을 read목록에 등록해주는 함수다.
    fd_max = serv_sock;
    printf("서버소켓 : %d", serv_sock);
    //fd_max는 read가 어디까지 채워져있는지 알려줌

    //서버가 지금부터 연결을 받을 수 있음
    while (1)
    {
        cpy_reads = reads;
        //select를 할경우 reads값이 다 바뀌는데 그럼 원본정보가 바뀌므로 그걸방지하기 위해 복사하는 과정
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;
        //5초마다 서버를 블로킹에서 풀어주려는 과정 select에서 서버가 멈추고 있기 때문이다.

        if ((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
        {
            break;
        }
        if (fd_num == 0)
        {
            //아무일도 안 일어났을 때 나오는 코드
            //printf("buf_state valid : %d first : %d room : %d message : %s\n",buf.valid,buf.first,buf.room,buf.message);
            for (int j = 0; j < fd_max + 1; j++)
            {
                if (member_list[j].valid == FULL)
                {
                    printf("< 방번호 : %d, 이름 : %s >\n", member_list[j].room, member_list[j].name);
                }
            }
            continue;
        }
        for (i = 0; i < fd_max + 1; i++)
        {
            if (FD_ISSET(i, &cpy_reads))
            {
                if (i == serv_sock) //서버연결 요청
                {
                    member_num++;
                    //연결요청이 왔으므로 멤버수 증가
                    adr_sz = sizeof(clnt_adr);
                    if (member_num < MAX_MEMBER)
                    {
                        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);

                        if (clnt_sock == -1) //어떤 이류로 인한 연결 실패
                        {
                            member_num--;
                            printf("accept() error\n");
                            i--;
                            continue;
                        }
                        else
                        {
                            FD_SET(clnt_sock, &reads);
                            if (fd_max < clnt_sock)
                                fd_max = clnt_sock;
                            new_member(fd_max);
                            strcpy(buf.message, "**********환영합니다! 사용하실 이름을 적어주세요.***********");
                            send_message(buf, SYSTEM_MESSAGE, clnt_sock);
                            printf("<사용자 연결>: %d\n", clnt_sock);
                        }
                    }
                }
                else
                {
                    str_len = read(i, (char *)&buf, sizeof(member));
                    int full_len = str_len;
                    while (full_len < sizeof(member))   //한번에 다 못받았을 때 다 받고 나서 다음 로직으로 가야한다.
                    {
                        if (str_len == 0)
                        {
                            break;
                        }
                        str_len = read(i, (char *)(&buf+str_len), sizeof(member));
                        full_len += str_len;
                    }
                    printf("From client : %s\n", buf.message);
                    if (full_len == 0 || !strcmp(buf.message, "/end")) //연결을 끊었을 때 
                    {
                        FD_CLR(i, &reads);
                        char name[MAX_NAME_SIZE];
                        int room = member_list[i].room;
                        member_list[i].valid = EMPTY;
                        member_list[i].room = EMPTY;  //처음 온것 초기화
                        member_list[i].first = EMPTY; //방정보 초기화
                        close(i);
                        printf("closed client: %d \n", i);
                        if (room != EMPTY)
                        {
                            strcpy(buf.message, "******< ");
                            strcat(buf.message, member_list[i].name);
                            strcat(buf.message, " > 님이 나가셨습니다.******");
                            for (int j = 0; j < fd_max + 1; j++)
                            {
                                if (member_list[j].room == room)
                                {
                                    send_message(buf, SYSTEM_MESSAGE, j);
                                }
                            }
                        }
                    }
                    //연결 요청 외의 것들을 다루는 곳
                    else
                    {
//*************************************************************************************************************************
//초기에 방설정과 이름을 정하는 함수 시작
                        if (member_list[i].first == EMPTY)
                        {
                            room_check = 0;
                            strcpy(member_list[i].name, buf.message);
                            strcpy(buf.message, "이름이 <");
                            strcat(buf.message, member_list[i].name);
                            strcat(buf.message, ">으로 설정이 완료되었습니다.");
                            printf("buf_message : %s \n", buf.message);
                            send_message(buf, SYSTEM_MESSAGE, i);

                            for (int j = 0; j < fd_max + 1; j++)
                            {
                                if (member_list[j].room != EMPTY)
                                {
                                    printf("%d번째가 %d방에소속되어 있습니다. ", j, member_list[j].room);
                                    room_check = 1;
                                }
                            }
                            if (room_check)
                            {
                                strcpy(buf.message, "현재 생성되어있는 방목록은 다음과 같습니다.");
                                buf.type = SYSTEM_MESSAGE;
                                send_message(buf, SYSTEM_MESSAGE, i);
                                memset((void *)room_list, 0, sizeof(int) * MAX_ROOM);
                                int fill_num = 0;
                                for (int j = 0; j < fd_max + 1; j++)
                                {
                                    if (member_list[j].room != EMPTY)
                                    {

                                        if (alreay_print_room(room_list, member_list[j].room, fill_num))
                                        {
                                            strcpy(buf.message, "< 방번호: ");
                                            sprintf(buf_temp, "%d", member_list[j].room);
                                            strcat(buf.message, buf_temp);
                                            strcat(buf.message, " >\n");
                                            room_list[fill_num++] = member_list[j].room;
                                            buf.type = FREE_MESSAGE;
                                            send_message(buf, SYSTEM_MESSAGE, i);
                                        }
                                    }
                                }

                                //다시 확인하기 위해 초기화시키는 코드
                                strcpy(buf.message, "입장하실 방을 입력하시거나 또는 새로운 방번호를 입력해주세요.");
                                send_message(buf, SYSTEM_MESSAGE, i);
                            }
                            else
                            {
                                strcpy(buf.message, "방이 없습니다. 새로운 방번호를 입력해주세요.");
                                send_message(buf, SYSTEM_MESSAGE, i);
                            }
                            member_list[i].first = FULL;
                        }
                        else if (member_list[i].room == EMPTY)
                        {
                            member_list[i].room = atoi(buf.message);
                            if (member_list[i].room == 0)
                            {
                                strcpy(buf.message, "0번방은 사용할 수 없습니다. 다시 입력해주세요.");
                                send_message(buf, SYSTEM_MESSAGE, i);
                            }
                            else
                            {
                                strcpy(buf_temp, "< ");
                                strcat(buf_temp, buf.message);
                                strcat(buf_temp, " >");
                                strcat(buf_temp, "방에 입장하셨습니다.");
                                strcpy(buf.message, buf_temp);
                                send_message(buf, SYSTEM_MESSAGE, i);
                                for (int j = 0; j < fd_max + 1; j++)
                                {
                                    if (member_list[j].room == member_list[i].room && i != j)
                                    {
                                        strcpy(buf.message, "*******방에 <");
                                        strcat(buf.message, member_list[i].name);
                                        strcat(buf.message, ">님이 입장하셨습니다.*******");
                                        send_message(buf, SYSTEM_MESSAGE, j);
                                    }
                                }
                            }
                        }
//*************************************************************************************************************************
                        //초기에 방설정과 이름을 정하는 함수끝
                        else
                        {
                            //특수 이벤트 처리
                            if (buf.type == SYSTEM_MESSAGE)
                            {
                                char *ptr = strtok(buf.message, " ");
                                if (!strcmp(ptr, "/w"))
                                {
                                    int check = 0;
                                    ptr = strtok(NULL, " ");
                                    char name[MAX_NAME_SIZE];
                                    if (strlen(ptr) >= MAX_NAME_SIZE)
                                    {
                                        strcpy(buf.message, "이름은 20글자까지입니다.");
                                        send_message(buf, SYSTEM_MESSAGE, i);
                                    }
                                    else
                                    {
                                        strcpy(name, ptr);

                                        ptr = strtok(NULL, " ");
                                        for (int j = 0; j < fd_max + 1; j++)
                                        {
                                            //귓속말은 같은 방에 없어도 가능하다.
                                            if (!strcmp(name, member_list[j].name) && j != i)
                                            {
                                                check = 1;
                                                strcpy(buf.message, ptr);
                                                strcpy(buf.name, name);
                                                send_message(buf, USER_MESSAGE, j);
                                                break;
                                            }
                                        }
                                        if (check == 0)
                                        {
                                            strcpy(buf.message, "접속하지 않은 사용자입니다.");
                                            send_message(buf, SYSTEM_MESSAGE, i);
                                        }
                                    }
                                }
                            }
                            //일반 채팅
                            else
                            {
                                strcpy(buf.name, member_list[i].name);
                                for (int j = 0; j < fd_max + 1; j++)
                                {
                                    if (member_list[i].room == member_list[j].room)
                                    {
                                        send_message(buf, USER_MESSAGE, j);
                                    }
                                }
                            }
                        }
                    }
                } //else 괄호
            }
        } //이까지가 select for문이다.

    } //while문 닫는 괄호void send_message(member buf);
}

void send_message(member buf, char type, int dest)
{
    printf("buf.message : %s\n", buf.message);
    buf.type = type;
    write(dest, (char *)&buf, sizeof(member));
    printf("보냈습니다.\n");
}

int alreay_print_room(int *room_list, int room_num, int fill_num)
{
    for (int i = 0; i < fill_num; i++)
    {
        if (room_num == room_list[i])
        {
            return 0;
        }
    }
    return 1;
}

//멤버를 등록하는 함수
//이건 정확히 필요할지 확실하지 않음
void new_member(int num)
{

    member_list[num].valid = FULL;
}

void error_handling(char *buf)
{
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}
