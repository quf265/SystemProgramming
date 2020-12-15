/*
*   시스템프로그래밍 채팅서버
*   select기반 멀티플렉싱 서버, 룸 구현
*   더한다면 마피아게임으로 바꿀 수 있음
*   10조 : 신현석, 한지예, 정재희, Jose
*   
*/

/*  작동원리
*   모든 소켓과 대화는 main에서 처리한다.(연결, 끊기, 메세지 전송)
*   병렬처리 + mutex 많이 < 단일처리 + mutex 없음 (mutex안쓰는게 더 낫다.)
*   단 마피아 게임의 시스템 메세지의 경우 thread에서 전달함
*   thread에서는 오직 mafia게임에서 signal로 시간알려주기, 능력다루기(게임진행), 사망시키기(게임진행) 등만 다룬다.
*   thread를 호출할 때는 그방에서 소속되어 있는 모든 소켓을 넘겨준다.
*   thread에서는 낮, 밤 변수를 바꿀 수 있고 게임 진행상황을 FALSE로 바꿀 수 있다.
*   게임을 하다가 중간에 나가는 경우가 있을 수 있으므로 항상 thread에서는 valid를 TRUE인지 체크한다.
*   현재 구현된 것(v1) : 연결, 이름(중복이름검사), 방목록 보여줌, 입장, 입장문구 ,귓속말, 차단, 채팅, 퇴장
*   향후 할 것(v2) :  게임이 시작된 방 못 들어가게 하기, 마피아 게임전용 메세지 함수 만들기, thread생성, thread마스크로 signal 보내기, thread에서 시스템메세지 보내기
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>

#define BUF_SIZE 100
#define MAX_MEMBER 100
#define MAX_NAME_SIZE 20
#define MAX_ROOM 100
#define EMPTY 0
#define FULL 1
#define TRUE 1
#define FALSE 0
#define error_message(x) {printf("%s\n",x);}
#define SWAP(x,y,z) { z = x; x = y; y =z;}

#define FREE_MESSAGE '0'
#define SYSTEM_MESSAGE '1'
#define USER_MESSAGE '2'
//free는 메세지만 보내고 싶을때
//sysmtem은 system출력하고 싶을때
//user은 사용자이름 출력하고 싶을 때 사용한다.

int room_max;


void error_handling(char *buf);
typedef enum{police,mafia,docter,soldier,citizen}jobs;
typedef enum{dead,alive}life;
typedef enum{use,unuse}capacity;
typedef enum{noon,ninght}today;     

typedef struct
{
    int valid;      //접속해있는 사람인지 아닌지 결정하는 변수
    int first;      //처음입장했는지 아닌지 묻는 함수
    int room;
    int whisper;        //귓속말 대상을 정하는 함수
    char type;
    char name[MAX_NAME_SIZE];
    char message[BUF_SIZE];
    //마피아게임을 위한 변수
    char play;      //마피아게임중인지 확인
    jobs job;   
    life live;
    capacity skill;  //능력을 썼는지 유무

} member;
//사용자 변수

typedef struct{
    int block_member[MAX_MEMBER];
}blocking;

typedef struct{
    int startgame;          //게임이 시작하고 있는지 아닌지 확인하는 변수
    int room_number;        //이방은 실제로 어느방인가
    int mem_number;         //처음 참가하고 있는 인원
    today day;               //지금 현재 낮인가 밤인가
    int exit_number;            //나간인원
    int * member_list;      //참가하고 있는 전원의 파일디스크립터를 넣는 변수
}room_info;

struct arg{
    int room_number;
    int mem_number;
};

blocking blocking_list[MAX_MEMBER]; //차단하기위한 변수
member member_list[MAX_MEMBER];
room_info room_mafia[MAX_ROOM];     //마피아하는 방을 위한 변수




//사용자를 담는 코드
int member_num;
void new_member(int num);

//사용자 초기화와 관련된 함수
int checking_name(member buf, int fd_max);      //중복이름을 검사하는 함수
int alreay_print_room(int *room_list, int room_num, int fill_num);  //방리스트를 검사하는 함수
void first_enter(member buf, int i, int fd_max);        //처음들어왔을 때 이름설정 도와주는 함수
void first_room(member buf, int i , int fd_max);        //처음왔을 때 방설정을 도와주는 함수
void out_room(member buf, fd_set *reads, int i, int fd_max); //나갈때 정리하는 함수

//채팅과 관련된 함수
void message_task(member buf, int i, int fd_max);           //메세지를 다루는 함수
void send_message(member buf, char type, int dest);     //메세지를 보내는 함수

//마피아와 관련된 함수
pthread_t * make_pthread(void);     //thread생성을 위한 함수
int for_mafia_room(int);       //빈방을 찾아준다.
int start_mafia(int, int);           //마피아 게임을 시작하는 함수(쓰레드에 넘겨줄것들을 만들고 쓰레드를 생성함)
void *mafia_game(void *);   //마피아게임 thread다
int initial_game(int ,int );         //직업을 설정해준다.
int mafia_number(int , int * , int *);     //마피아가 몇명 살아있는지 반환하는 함수
void mafia_chat(int , int);


int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;              //소켓 설정을 위한 소켓 생성
    struct sockaddr_in serv_adr, clnt_adr; //서버쪽 주소와 클라이언트쪽 주소를 저장하기위한 구조체
    struct timeval timeout;                //서버가 무한정 블로킹상태에 빠지지 않기 위한 timeout(연결을 기다릴때 계속 가만히 있을 수 있다.)
    fd_set reads, cpy_reads;               //이건 현재 연결되어 있는 사용자를 비트로 표현하는 것이다. (1이면 연결 0 이면 비었음)
    socklen_t adr_sz;
    int fd_max, str_len, fd_num, i;
    int room_list[MAX_ROOM]; //이까지는 소켓을 위한 변수

    member buf;              //버퍼 이걸로 통신함
    char buf_temp[BUF_SIZE]; //메세지 옮기기위한 변수
    int room_check = 0;      //방리스트 보여줄때 쓰는 변수
    char message_type[10];   //메세지 타입(FREE,USER,SYSTEM)

    if (argc != 2)
    {
        printf("Usage : %s <PORT> \n", argv[0]); //포트임의로 넣기위해서
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error"); // bind는 이제 소켓에 주소를 할당하는 과정이다.

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error"); //listen은 이제 소켓이 들을 수 있음을 말한다. 즉 연결요청이 오면 들을 수 있다.

    FD_ZERO(&reads); //아무것도 연결안되어 있으니까 전부 0으로 초기화하는 매크로함수

    FD_SET(serv_sock, &reads); //서버 소켓을 read목록에 등록해주는 함수다.

    fd_max = serv_sock;
    printf("서버소켓 : %d", serv_sock); //fd_max는 read가 어디까지 채워져있는지 알려줌

    srand((long)time(NULL));
    //서버가 지금부터 연결을 받을 수 있음
    while (1)
    {
        cpy_reads = reads; //select를 할경우 reads값이 다 바뀌는데 그럼 원본정보가 바뀌므로 그걸방지하기 위해 복사하는 과정
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000; //5초마다 서버를 블로킹에서 풀어주려는 과정 select에서 서버가 멈추고 있기 때문이다.

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
                    while (full_len < sizeof(member)) //한번에 다 못받았을 때 다 받고 나서 다음 로직으로 가야한다.
                    {
                        if (str_len == 0)
                        {
                            break;
                        }
                        str_len = read(i, (char *)(&buf + str_len), sizeof(member));
                        full_len += str_len;
                    }
                    printf("From client : %s\n", buf.message);
                    if (full_len == 0 || !strcmp(buf.message, "/end")) //연결을 끊었을 때
                    {
                        out_room(buf, &reads, i, fd_max);
                    }
                    //연결 요청 외의 것들을 다루는 곳
                    else
                    {
                        //*************************************************************************************************************************
                        //초기에 방설정과 이름을 정하는 함수 시작
                        if (member_list[i].first == EMPTY)
                        {
                            first_enter(buf, i, fd_max);
                        }
                        else if (member_list[i].room == EMPTY)
                        {
                            first_room(buf, i, fd_max);
                        }
                        //*************************************************************************************************************************
                        //초기에 방설정과 이름을 정하는 함수끝
                        else
                        {
                            message_task(buf, i, fd_max);
                        }
                    }
                } //else 괄호
            }
        } //이까지가 select for문이다.

    } //while문 닫는 괄호void send_message(member buf);
} //main끝


/**************************************** 마피아 코드 *******************************************************/
//마피아 게임

void mafia_chat(int i, int fd_max){     //일단 만들어 놓음 이건 마피아게임일 하는 방이면 이 채팅기법으로 넘어온다.

}

int mafia_number(int room_pos, int * mafia_num, int * live_num){
    int mem_number = room_mafia[room_pos].mem_number;
    *mafia_num = 0;
    *live_num = 0;
    for(int i = 0 ; i < mem_number ; i++){
        if(member_list[room_mafia[room_pos].member_list[i]].valid == TRUE){
            if(member_list[room_mafia[room_pos].member_list[i]].job == mafia){
                (*mafia_num)++;
            }
            else{
                (*live_num)++;
            }
        }
        
    }    
    return 0;
}
void *mafia_game(void *args)
{
    //printf("thread 들어온다.");

    int room_pos = (*(struct arg *)args).room_number;
    int first_number = (*(struct arg *)args).mem_number;
    int live_number = first_number;
    int mafia_num;
    member buf;
    free(args);     //필요없음 더이상

    while(1)
    {
        mafia_number(room_pos, &mafia_num, &live_number);
        if(mafia_num * 2 >= live_number){
            break;
        }

    }
    
    
    /*  디버깅 코드
    int j = 0;
    strcpy(buf.message, "hello what a good mafia");
    while (j < 5)
    {
        for (int i = 0; i < number; i++)
        {
            send_message(buf, SYSTEM_MESSAGE, room_mafia[0].member_list[i]);
        }
        j++;
    }*/
    return NULL;
}

int initial_game(int mem_number, int room_pos)
{
    jobs *job;
    job = (jobs *)malloc(sizeof(jobs) * mem_number);
    jobs temp;
    int j;
    for(int i = 0 ; i < mem_number ; i++){
        job[i] = citizen;
    }
    if (mem_number <= 5)
    { //4명일 때            마피아 : 2, 경찰 : 1
        job[0] = mafia;
        job[1] = police;
    }
    else if (mem_number <= 7)
    { //5명에서 6명일 때    마피아 : 2, 경찰 : 1, 의사 : 1
        job[0] = mafia;
        job[1] = mafia;
        job[2] = police;
        job[3] = docter;
    }
    else if (mem_number < 10)
    { //7명에서 9명일때     마피아 : 3, 경찰 : 1, 의사 : 1, 군인 : 1
        job[0] = mafia;
        job[1] = mafia;
        job[2] = mafia;
        job[3] = police;
        job[4] = docter;
        job[5] = soldier;
    }
    else
    { //10명에서 12명일 때   마피아 : 3, 경찰 : 1, 의사 : 1, 군인 : 2   
        job[0] = mafia;
        job[1] = mafia;
        job[2] = mafia;
        job[3] = police;
        job[4] = docter;
        job[5] = soldier;
        job[6] = soldier;
    }
    for(int i = 0 ; i < mem_number ; i++){
        j = rand()%mem_number;
        SWAP(job[i],job[j],temp);
    }
    //섞었다.
    for (int i = 0; i < mem_number; i++)
    {
        member_list[room_mafia[room_pos].member_list[i]].play = TRUE;   //게임진행중 초기화
        member_list[room_mafia[room_pos].member_list[i]].job = job[i];  //직업설정
        member_list[room_mafia[room_pos].member_list[i]].skill = TRUE;  //능력도 다 초기화 해준다.
        member_list[room_mafia[room_pos].member_list[i]].live = TRUE;   //아직 살아있음을 해줌
    }
    return 0;
}

//마피아 게임 시작
int start_mafia(int i, int fd_max){     //이미 게임중인지 확인했고 인원은 4명에서 12명사이다.
    int * mem_number = (int *)malloc(sizeof(int));
    *mem_number = 0;
    int temp_member[12];    //최대 열두명이니까
    int error = FALSE;
    int room_pos = 0;
    pthread_t * mafia_thread;
    pthread_t temp;
    for(int j = 0 ; j < fd_max+1 ; j++){
        if(member_list[j].room == member_list[i].room){     //i를 포함해서 다넣는다.
            if((*mem_number)>=12){
                error = TRUE;
                break;
            }
            temp_member[*mem_number]=j;
            (*mem_number)++;  
        }
    }
    if((*mem_number) <= 3 || error ){
        free(mem_number);
        error = TRUE;
        return error;
    }
    //지금 부터 만든다.
    room_pos=for_mafia_room(i);     //마피아하는 방들은 따로 모아서 관리할 것이다.
    printf("<%d>\n",*mem_number);
    room_mafia[room_pos].member_list = (int *)malloc(sizeof(int)*(*mem_number));
    for(int j = 0 ; j< fd_max+1 ; j++){
       room_mafia[room_pos].member_list[j] = temp_member[j];        //소켓을 전부 등록해주는 과정
    }
    if(initial_game(*mem_number, room_pos)){
        error_message("직업 설정 실패");
        return -1;
    }
    mafia_thread = make_pthread();
    struct arg * args = (struct arg *)malloc(sizeof(struct arg));
    args->room_number = room_pos;
    args->mem_number = *mem_number;
    free(mem_number);
    if(pthread_create(mafia_thread, NULL, mafia_game , args)!=0){       //create_pthread error
        error_message("쓰레드 생성 실패");
        return -1;
    }
    room_mafia[room_pos].startgame = TRUE;
    for(int j = 0 ; j < *mem_number ; j++){
        printf("%s\n",member_list[temp_member[j]].name);
    }
    room_mafia[room_pos].room_number = member_list[i].room;     //어느방에서 하고 있는지 가르쳐준다.
    
    return 0;   //마피아게임을 만들었다.
}

int for_mafia_room(int i){        //빈방을 찾아준다.
    int j;
    for(j = 0  ; j <= room_max ; j++){
        if(room_mafia[j].startgame == FALSE){
            break;
        }
    }
    if(j >= room_max){
        room_max = j+1;
    }
    return j;   //빈방을 넘겨준다.
}

/**************************************** 마피아 코드 *******************************************************/

void out_room(member buf, fd_set *reads, int i, int fd_max)
{
    FD_CLR(i, reads);
    char name[MAX_NAME_SIZE];
    int room = member_list[i].room;
    /*
    if(member_list[room].play==TRUE){
        for(int j = 0 ; j < room_max ; j++){
            if(room_mafia[j].room_number == room){          
                room_mafia[j].exit_number++;
                break;
            }
        }
    }*/
    memset(&member_list[i],EMPTY,sizeof(member_list[i]));       //전부 0으로 초기화시킨다.
    memset((void *)blocking_list[i].block_member, FALSE, sizeof(blocking_list[i].block_member)); //차단정보 초기화
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

void first_room(member buf, int i, int fd_max)
{
    char buf_temp[BUF_SIZE];
    member_list[i].room = atoi(buf.message);
    if (member_list[i].room == 0)
    {
        strcpy(buf.message, "0번방은 사용할 수 없습니다. 다시 입력해주세요.");
        send_message(buf, SYSTEM_MESSAGE, i);
        return;
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

void first_enter(member buf, int i, int fd_max)
{
    int room_check = 0;
    int room_list[MAX_ROOM];
    char buf_temp[BUF_SIZE]; //메세지 옮기기위한 변수

    if (checking_name(buf, fd_max))
    {
        strcpy(member_list[i].name, buf.message);
        strcpy(buf.message, "이름이 <");
        strcat(buf.message, member_list[i].name);
        strcat(buf.message, ">으로 설정이 완료되었습니다.");
        printf("buf_message : %s \n", buf.message);
        send_message(buf, SYSTEM_MESSAGE, i);
    }
    else{
        strcpy(buf.message,"중복되는 이름이 있습니다. 다른이름을 선택해주세요.");
        send_message(buf,SYSTEM_MESSAGE,i);
        return;     
    }

    for (int j = 0; j < fd_max + 1; j++)
    {
        if (member_list[j].room != EMPTY)
        {
            printf("%d번째가 %d방에소속되어 있습니다. ", j, member_list[j].room);    //디버깅용 코드
            room_check = 1; //이건 필요함
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

void message_task(member buf, int i, int fd_max)    
{
    //특수 이벤트 처리
    if (buf.type == SYSTEM_MESSAGE)
    {
        char *ptr = strtok(buf.message, " ");
        if (!strcmp(ptr, "/w")) //귓속말
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
                    if (!strcmp(name, member_list[j].name) && j != i)
                    {
                        if (member_list[j].room == member_list[i].room)
                        {   
                            check = 1;      //차단한지 알면 곤란하니까
                            member_list[i].whisper = j; // 귓속말 설정을 해준다. 앞으로 채팅을 치면 계속 채팅이  그사람에게 간다.
                            strcpy(buf.message,"귓속말이 [");
                            strcat(buf.message,name);
                            strcat(buf.message,"]으로 설정되었습니다.");
                            send_message(buf,SYSTEM_MESSAGE,i);
                            break;
                            /*
                            if (blocking_list[j].block_member[i] == FALSE) //차단도 안당해있어야함
                            {
                                
                                strcpy(buf.message, ptr);
                                strcpy(buf.name, name);
                                send_message(buf, USER_MESSAGE, j);
                                break;
                            }*/
                        }
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
        else if(!strcmp("/nw",buf.message)){        //not whisper 귓속말 설정 종료
            if(member_list[i].whisper == EMPTY){
                strcpy(buf.message,"귓속말대상이 없습니다.");
                send_message(buf,SYSTEM_MESSAGE,i);
            }
            else{
                member_list[i].whisper = EMPTY;
                strcpy(buf.message,"귓속말이 해제되었습니다.");
                send_message(buf,SYSTEM_MESSAGE,i);
            }
        }
        else if (!strcmp("/b", ptr)) //차단
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
                for (int j = 0; j < fd_max + 1; j++)
                {
                    if (!strcmp(name, member_list[j].name) && j != i)
                    {
                        if (member_list[j].room == member_list[i].room)
                        {
                            check = 1;
                            blocking_list[i].block_member[j] = TRUE;
                            strcpy(buf.message, "[");
                            strcat(buf.message, name);
                            strcat(buf.message, "]님을 차단했습니다.");
                            send_message(buf, SYSTEM_MESSAGE, i);
                            break;
                        }
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
        else if(!strcmp("/start", buf.message)){        //마피아 게임 시작
            if (member_list[i].play == TRUE)
            {
                strcpy(buf.message, "이미 게임을 하고 있습니다.");
                send_message(buf, SYSTEM_MESSAGE, i);
            }
            else{
                if(start_mafia(i, fd_max)){     //실패했을 경우
                    printf("방을 만드는데 실패했습니다.");
                    strcpy(buf.message,"최소 인원은 4명 최대인원은 12명입니다.");
                    send_message(buf,SYSTEM_MESSAGE,i);
                }
                else{
                    printf("마피아게임이 시작됩니다.");     //청소 코드를 여기 넣을까 생각중
                }
            }
        }
    }
    //일반 채팅
    else        
    {
        strcpy(buf.name, member_list[i].name);
        if (member_list[i].whisper && member_list[member_list[i].whisper].valid == TRUE)     //귓속말이 있을 때
        {
            send_message(buf,USER_MESSAGE,member_list[i].whisper);
        }
        else
        {
            for (int j = 0; j < fd_max + 1; j++)
            {
                if (member_list[i].room == member_list[j].room)
                {
                    if (blocking_list[j].block_member[i] == FALSE) //차단안당했을 때만 보내기
                    {
                        send_message(buf, USER_MESSAGE, j);
                    }
                }
            }
        }
    }
}

int checking_name(member buf, int fd_max){

    for(int j = 0 ; j < fd_max+1; j++){
        printf("검사중입니다.\n");
        if(!strcmp(member_list[j].name,buf.message)){
            return 0;
        }
    }
    return 1;
}

void send_message(member buf, char type, int dest)
{
    printf("buf.message : %s\n", buf.message);      //디버깅용
    buf.type = type;
    write(dest, (char *)&buf, sizeof(member));
    printf("보냈습니다.\n");        //디버깅용
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
    member_list[num].valid = TRUE;
}

void error_handling(char *buf)
{
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}


pthread_t * make_pthread(void){  
    pthread_t * temp = (pthread_t*)malloc(sizeof(pthread_t));
    return temp;
}