#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000 // 캐시 전체 사이즈
#define MAX_OBJECT_SIZE 102400 // 캐시에 들어갈 객체 하나 사이즈

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

/*  트랜잭션 처리함수, GET Request이나 HEAD Request 들어오면 정적인지 동적인지 파악하여 각각의 함수를 실행 */
void doit(int fd);
void parse_uri(char *uri, char *hostname, char *path,int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port);
void *thread(void *vargsp);
void init_cache(void);
int reader(int connfd, char *url);
void writer(char *url, char *buf);

/* cache 구조체 */
typedef struct {
    char *url;      /* url 담을 변수(같은 요청인지 아닌지 확인) */
    int *flag;      /* 캐시가 비어있는지(0) 이미 차 있는지(1) 구분할 변수 */
    int *cnt;       /* 최근 방문 순서 나타내기 위한 변수 - 최근 방문하면 0, 방문 안 할수록 +1 */
    char *content;  /* 클라이언트에 보낼 내용 담겨있는 변수 */
} Cache_info;

Cache_info *cache;  /* cache 변수 선언 */
int readcnt;        /* 현재 크리티컬 섹션에 있는 reader의 수를 세는 변수 */
sem_t mutex, w; 

/* end server info(브라우저 테스트를 위한) */
// static const char *end_server_host = "localhost";   /* end server의 hostname은 현재 localhost */
// static const int end_server_port = 9190;            /* proxy 서버의 소켓 번호 +1 */

/*
* argc: 메인 함수에 전달 되는 데이터의 수
* argv: 메인 함수에 전달 되는 실질적인 정보 (여기선 proxy에서 연결 대기 상태로 만들어주고 싶은 포트 번호가 들어감)
* ./proxy "8888"이 argv
*/
int main(int argc, char **argv) {
    /* 프로그램 실행 시 port를 안썼으면 */
    // telnet localhost ~여기에 port번호 안 썼으면~
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    init_cache();                          /* 캐시 생성 */
    int listenfd, *connfd;                  
    // 듣기식별자: 클라이언트로부터의 연결 요청 승낙 가능 상태 시 생성됨
    // 연결식별자: 서버가 연결 요청을 수락할 때마다 생성되고, 서버가 클라이언트에 서비스 하는 동안에만 존재
    char hostname[MAXLINE], port[MAXLINE]; /* hostname: 접속한 클라이언트 ip, port: 접속한 클라이언트 port */
    socklen_t clientlen;                   /* socklen_t 는 소켓 관련 매개 변수에 사용되는 것으로 길이 및 크기 값에 대한 정의를 내려준다 */
    struct sockaddr_storage clientaddr;    /* 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소 */
                                           /* 어떤 타입의 소켓 구조체가 들어올지 모르기 때문에 충분히 큰 소켓 구조체로 선언 */
    pthread_t tid;                         /* thread ID는 스레드가 생성되면 스레드 id인 tid가 생성됨 */                          

    /* listenfd: 이 포트에 대한 프록시의 듣기 소켓 오픈 */
    // telnet localhost 8888
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);                                                 /* 소켓 구조체 크기 */
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                      /* 연결 요청 큐에 아무것도 없을 경우 기본적으로 연결이 생길때까지 호출자를 막아둠, 즉 대기 상태 (./proxy 8888 했을 때의 경우) */
                                                                                        /* telnet localhost 8888로 연결요청이 생기면 연결 요청 큐에 요청이 담김
                                                                                           addr 내에 클라이언트의 소켓주소가 담기고
                                                                                           connfd가 생겨나는 것 */
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); /* clientaddr: SA 구조체로 형변환, 클라이언트 소켓 정보를 가져옴 */

        // telnet localhost 8888을 실행했을 때 출력됨 (편하게 생각하면 telnet localhost 8888은 클라이언트가 연결요청을 서버에 보낸 것임 -> Accept 함수 실행되고 connfd가 생성됨)
        printf("Accepted connection from (%s, %s)\n", hostname, port);                  /* 어떤 주소와 포트 번호를 가진 client가 들어왔는지 print */

        Pthread_create(&tid, NULL, thread, (void *)connfd);

        // doit(connfd);                                                                /* 트랜잭션 수행 */
        
        /* 연결이 끝났다고 print 하고 식별자(파일 디스크립트)를 닫아줌 */
        // printf("endoffile\n");
        // Close(connfd);
    }
}

void *thread(void *vargs) {                 // 매개변수: 클라이언트와 프록시의 연결식별자
    int connfd = *((int *)vargs);           // 클라이언트와 프록시가 연결되어 있는 식별자       
    Pthread_detach(pthread_self());         // 연결가능한 상태로 스레드가 유지되면 스레드 하나를 종료시켜도 연결된 스레드가 죽을 때까지 기다림
                                            // 죽이면 자기 자신만 죽이고 싶어서 자신을 분리해주는 것
    free(vargs);                            // 공간 할당 해제
    doit(connfd);                           // 스레드별로 트랜잭션 수행
    Close(connfd);                          // 트랜잭션 수행 후 식별자 닫기
    return NULL;
}

/* 클라이언트의 요청을 입력받고 요청에 따른 트랜잭션 (알맞은 서버로 요청 보내줌) 처리 */
void doit(int connfd) {
    int end_serverfd;

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; /* buf: request 헤더 정보 담을 공간*/
    char endserver_http_header[MAXLINE];                                /* 서버에 보낼 헤더 정보를 담을 공간 */
    char hostname[MAXLINE], path[MAXLINE];                              /* hostname: 클라이언트가 요청한 IP담을 공간, path: 경로 담을 공간 */
    rio_t rio, server_rio;                                              /* rio: client rio 구조체, server_rio: proxy의 rio 구조체 */
    int port;                                                           /* port 담을 변수 */
    char url[MAXLINE];                                                  /* cache를 read할 때 uri 값을 넣어둔 url 변수로 찾을 수 있음 */
    char content_buf[MAX_OBJECT_SIZE];                                  /* 서버에서 받아온 내용, 즉 cache에 쓸 내용을 담은 버퍼 */

    Rio_readinitb(&rio, connfd);                                        /* rio 구조체 초기화 */
    // telnet localhost 8888해줬을 때 여기서 클라이언트의 입력을 기다리고 있음
    // GET http://localhost:9020/index.html을 clientfd에 입력해주면 연결된 connfd에 전송되고 connfd와 연결된 rio에 전송되고 buf에 담김
    Rio_readlineb(&rio, buf, MAXLINE);                                  /* buf에 fd에서 읽을 값이 담김 */
    sscanf(buf, "%s %s %s", method, uri, version);                      /* sscanf는 첫 번째 매개 변수가 우리가 입력한 문자열, 두 번째 매개 변수 포맷, 나머지 매개 변수에 포맷에 맞게 데이터를 읽어서 인수들에 저장 */
    strcpy(url, uri);                                                   /* url에 uri 복사 */

    /* GET가 아닐 때 error 메시지 출력 */
    // 같으면 0이므로 같으면 false가 되어 안에 내용이 실행되지 않음 -> 달라야 실행됨
    if (strcasecmp(method, "GET")) {
        printf("501 Not implemented Tiny does not implement this method");
        return;
    }

    /* cache에서 url을 가진 것 찾았을 때 cache hit */
    if (reader(connfd, url)) {
        return; // doit() 종료
    }

    // 캐시에서 데이터 찾았으면 rio_written으로 캐시에 있는 값 보내줌 -> return  (서버에 연결할 필요 없으니까 close할 필요 없음)

    /* 클라이언트가 요청한 ip 정보와 경로와 port번호 세팅해줄 함수 */
    parse_uri(uri, hostname, path, &port);
    /* 서버로 보낼 http header 만드는 함수 */
    /* 프록시에서 HTTP request header로 파싱해주는 것 */
    // endserver_http_header에 build_http_header에서 만든 HTTP request header가 담김
    build_http_header(endserver_http_header, hostname, path, port, &rio);

    /* Tiny 서버와 프록시 서버가 connect하는 함수 */
    // endserver_http_header에 프록시에서 파싱해준 요청 헤더가 담겨있음
    // 서버와 프록시가 연결되어 새로운 연결 식별자가 생겨남
    end_serverfd = connect_endServer(hostname, port);

    /* 서버와 연결 실패 */
    if(end_serverfd < 0) {
        printf("connection failed\n");
        return;
    }

    /* 서버와 연결한 이후에는 proxy가 서버 입장이 되어 end 서버에서 수정한 값을 server_rio와 end_serverfd를 먼저 연결하고
    * 그리고 서버에서 수정한 end_serverfd와 연결된 server_rio에 있는 값을 buf에 한 줄씩 server_rio에서 전송해주고 
    * buf의 내용을 connfd로 전송해줌
    */
    /* server_rio 초기화, 즉, proxy와 서버에 연결되어 있는 식별자로 초기화 함 */
    // end_serverfd로 프록시와 서버 사이에서 연결되어 있는 통로가 만들어져 있는 상태임
    // 서버에서 connfd에 수정해준 게 end_serverfd에 실시간으로 전송될 수 있음
    Rio_readinitb(&server_rio, end_serverfd);

    /* proxy와 서버에 연결되어있는 식별자에 http header 작성 */
    // tiny 서버의 doit()에서 Rio_readlineb에서 클라이언트의 요청을 기다리고 있던 서버에서 
    // write을 해주면 그제서야 만들었던 request header가 end_serverfd에 담기면 fd에 전송되고 rio에 담겨서 buf에 담김
    // 그러면 tiny 서버는 입력을 받고 계속 실행함
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

    /* server_rio의 내부 버퍼(end_serverfd와 연결되어 있음)에 담긴 내용을 buf에 담고 클라이언트(리얼 사용자)와 proxy에 연결되어 있는 식별자 connfd에 buf에 담긴 내용을 씀 */
    size_t n;
    int total_size = 0;

    while((n = rio_readlineb(&server_rio, buf, MAXLINE)) != 0) { // 리턴 값인 n은 buf의 크기 (읽은 크기)
        printf("proxy received %ld bytes,then send\n",n);
        Rio_writen(connfd, buf, n); // 서버에서 보내준 값이 end_serverfd와 연결된 server_rio에 담기고 그것을 buf가 한 줄씩 읽는데, 그걸 connfd에 써서 클라이언트가 보게 함

        /* cache content의 최대 크기를 넘지 않으면 content_buf에 담음 */
        /* content_buf는 서버에서 받아온 내용, 즉 cache에 쓸 내용을 담은 버퍼 */
        if (total_size + n < MAX_OBJECT_SIZE) { // 현재 캐쉬에 들어있는 객체의 total_size와 서버에서 보내준 것을 한 줄 더했을 때 MAX보다 적으면
            strcpy(content_buf + total_size, buf); // 서버가 보내준 한 줄을 content_buf + total_size에 복사
        }
        total_size += n;
    }

    /* cache content의 최대 크기를 넘지 않았다면 cache에 쓰기 */
    // 현재 들어오는 buf의 크기를 total size와 더했을 때 전체 캐시의 크기보다 크면 못 넣고 사라짐
    if(total_size < MAX_OBJECT_SIZE) {
        writer(url, content_buf);
    }

    /* end_serverfd 닫기 */
    Close(end_serverfd);
}

/* 클라이언트가 요청한 ip 정보와 경로와 port번호 세팅해줄 함수 */
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    // uri : http://localhost:9020/index.html
    
    char* pos = strstr(uri,"//");                   /* "http://"가 있으면 //부터 return */
                                                    // 일치하는 문자열이 있으면 해당 위치의 포인터(char*) 반환
    /* pos는 ip가 시작되는 위치 */
    pos = pos != NULL? pos+2:uri;                   /* "//~~~~" 이렇게 나오니깐 + 2해서 ip가 시작하는 위치로 포인터 이동, 없으면 그냥 uri (전체) */
    /* pos2+1은 port 번호가 시작되는 위치 */
    char *pos2 = strstr(pos, ":");;                 /* ':'뒤에는 port랑 path가 있음 */
    // *port = end_server_port;                    
    *port = 80;                                     /* 기본 port: port 번호 안써줬을 때 기본 포트로 연결할 수 있게 하려고 *port에 80 넣어줌 */

    /* port(:)가 있으면 */
    if(pos2 != NULL) {
        *pos2 = '\0';                               /* ':'를 '\0'으로 변경 */
        sscanf(pos, "%s", hostname);                /* pos는 현재 ip 시작하는 위치이기 때문에 그 위치에서 문자열 포맷으로 ip를 hostname에 담음 */
        sscanf(pos2+1, "%d%s", port, path);         /* pos2+1은 포트가 시작하는 위치이기 때문에 정수형 포멧으로 port를 담고 문자열 포맷으로 경로를 path에 담음 */
    } 
    /* port(:)가 없으면 hostname과 path만 분리해주면 됨*/
    // uri : http://localhost/index.html
    else {
        pos2 = strstr(pos, "/");                    /* '/'를 찾아서 있으면 포인터 변경 */
        /* path가 있는지 확인 */
        if(pos2 != NULL) {
            *pos2 = '\0';                           /* '/'를 '\0'으로 바꿈 why? pos에 문자열 버퍼로 \n 전까지 끊어서 hostname과 path에 각각 넣어주려고 */
            sscanf(pos, "%s", hostname);            /* pos에서 문자열 포맷으로 ip를 hostname에 담음 */
            *pos2 = '/';                            /* 다시 pos2위치에 '/'를 붙혀서 path작성할 준비 (pos2 인덱스가 가리키는 값에 /를 넣어주는 것) */
            sscanf(pos2, "%s", path);               /* pos2에서 문자열 포맷으로 경로를 path에 담음 */
        } 
        /* path가 없으면 */
        else {
            // uri : http://localhost
            sscanf(pos, "%s", hostname);            /* pos에서 문자열 포맷으로 ip를 hostname에 담음 */
        }
    }

    // /* host명이 없는 경우(브라우저 테스트를 위함) */
    // if (strlen(hostname) == 0) {
    //     strcpy(hostname, end_server_host);
    // }
    return;
}

/* 서버에 요청할 헤더 만듬 */
/* 목표 형식 : GET /hub/index.html HTTP/1.0 */
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio) {
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    /* request_hdr에 요청 헤더 담음 HTTP/1.0로 변경 */
    // static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
    // 여기서 %s엔 path가 들어감
    sprintf(request_hdr, requestlint_hdr_format, path);

    /* 클라이언트가 보낸 요청 라인 읽기 */
    // client_rio에 있는 걸 한 줄씩 buf에 담는다
    while(Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        /* 마지막 종료 지점을 만나면 break */
        // client_rio는 connfd와 연결되어 있음
        // 클라이언트가 보낸 요청라인과 \r\n 이 같아지면 == 즉 헤더가 끝나면
        if(strcmp(buf, endof_hdr) == 0) {
            break; 
        }

        /* 대소문자 구문하지 않고 buf에서 host_key ("Host") 찾았으면 host_hdr값 세팅, 즉 "Host"찾는 거임 */
        /* 같은 거 찾으면 0 리턴 -> !0 이니까 True가 되어서 함수 안에가 실행되고 host_hdr에 host 값이 복사되어서 담김 */
        if(!strncasecmp(buf, host_key, strlen(host_key))) {
            printf("%s\n", buf);
            strcpy(host_hdr, buf);
            continue;
        }

        /* Connection, Proxy-Connection, User-Agent를 제외한 요청 라인을 other_hdr에 담음 */
        // 한 개라도 동일하면 0이되어서 if문 실행 안됨
        // other_hdr에 다른 요청들 계속 더해줌
        if(strncasecmp(buf, connection_key, strlen(connection_key)) && strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) && strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
            strcat(other_hdr, buf); // 버퍼에 있는 한 줄 한 줄을 other_hdr에 넣어줌 (요청헤더에 있는 나머지 값들을 그대로 넣어주는 것)
        }
    }

    /* 요청 라인을 다 읽었는데 Host가 없으면 현재 domain을 host_hdr에 담음 */
    if(strlen(host_hdr) == 0) {
        sprintf(host_hdr, host_hdr_format, hostname);
    }

    /* 담은 요청들을 http_header에 담음 */
    // endserver_http_header에 담는 것
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);

    return;
}

/* 서버와 proxy와 연결 */
// 연결하고 싶은 서버의 ip주소, 포트 번호, 프록시에서 파싱해준 요청 헤더
int connect_endServer(char *hostname, int port) {
    char portStr[100];
    // 포트 번호를 "%d"에 담고 그걸 postStr에 담음
    sprintf(portStr, "%d", port);
    // open_clientfd(연결하고 싶은 서버의 ip, 포트 넘버)
    // ip, port에 연결 요청을 듣는 서버와 연결을 설정함
    // server는 이미 ./tiny 9020 을 통해 accept에서 연결 요청을 대기하고 있는 상태이므로
    // 프록시와 서버가 연결됨 -> 결과적으로 connectfd 같은 새로운 연결 식별자가 생겨남
    // (중요) 프록시와 서버 사이에 Accept이 되고 서버는 Rio_readlineb에서 write가 일어날 때까지 기다리고 있음
    // 연결이 되었으니까 연결 식별자를 리턴하는 것 (듣기 식별자는 서버에 있는 것)
    return Open_clientfd(hostname, portStr);
}

/* cache 초기화 */
void init_cache() {
    Sem_init(&mutex, 0, 1);                                                 /* mutex를 1로 초기화 */
    Sem_init(&w, 0, 1);                                                     /* w를 1로 초기화 */
    readcnt = 0;                                                            /* 현재 크리티컬 섹션에 있는 reader의 수를 세는 변수 */
    cache = (Cache_info *)Malloc(sizeof(Cache_info) * 10);                  /* 캐시의 최대 크기는 1MB이고 캐시의 객체는 최대 크기가 100KB이라서 10개의 공간을 잡음 */
    for (int i = 0; i < 10; i++) {
        cache[i].url = (char *)Malloc(sizeof(char) * 256);                  /* url 공간을 256바이트 할당 */
        cache[i].flag = (int *)Malloc(sizeof(int));                         /* flag 공간을 4바이트 할당 */
        cache[i].cnt = (int *)Malloc(sizeof(int));                          /* cnt 공간을 4바이트 할당 */
        cache[i].content = (char *)Malloc(sizeof(char) * MAX_OBJECT_SIZE);  /* content의 공간을 100KB 할당 */
        *(cache[i].flag) = 0;                                               /* flag 0으로 설정, 즉, 비어있다는 뜻 */
        *(cache[i].cnt) = 1;                                                /* cnt 0으로 설정, 최근에 찾은 것일 수록 0이랑 가까움 */
    }
}

/* cache에서 요청한 url 있는지 찾기 */
/* 세마포어를 이용해서 reader가 먼저 되고 여러 thread가 읽고 있으면 writer는 할 수가 없게 한다. */
int reader(int connfd, char *url) {
    int return_flag = 0;    /* 캐시에서 찾았으면 1, 못 찾으면 0 */
    P(&mutex); // readcnt를 한번에 하나의 thread만 갱신하도록              
    readcnt++; // 현재 크리티컬 섹션에 있는 reader의 수를 세는 변수 
    if(readcnt == 1) {
        P(&w); // 여러 thread가 read하는 동안 write 방지
    }
    V(&mutex);

    /* cache를 다 돌면서 cache에 써있고 cache의 url과 현재 요청한 url이 같으면 client fd에 cache의 내용 쓰고 해당 cache의 cnt를 0으로 초기화 후 break */
    // 가장 최근에 방문한 cache가 0에 가깝고 오래 전에 방문한 cache는 수가 커짐 -> cache의 용량 초과할 때마다 cnt가 큰 수부터 삭제해줌
    for(int i = 0; i < 10; i++) {
        if(*(cache[i].flag) == 1 && !strcmp(cache[i].url, url)) {  // 캐시 객체에 값이 들어있을 때만 찾아야 하므로 1일 때 && 같은 거 찾았을 때 0이니까 not을 붙여서 찾아준 것
            Rio_writen(connfd, cache[i].content, MAX_OBJECT_SIZE); // connfd에 cache에 들어있는 내용을 써줌
            return_flag = 1;                                       // cache에서 찾았을 때 더이상 doit()이 실행되지 않도록 하기 위해 return값을 주려고 return_flag에 1을 줌
            *(cache[i].cnt) = 1;                                   // 최근에 방문한 곳이므로 1로 초기화 해줌
            break;
        }
    }    
    
    /* 모든 cache객체의 cnt를 하나씩 올려줌, 즉, 방문 안 한 객체들의 cnt를 하나씩 올려줌*/
    /* LRU 알고리즘 (least recently used : 가장 오랫동안 참조되지 않은 객체를 교체)
     * 프로세스가 주기억장치에 접근할 때마다 참조된 객체에 대한 시간을 기록해야함 -> 큰 오버헤드가 발생
     */
    for(int i = 0; i < 10; i++) {
        (*(cache[i].cnt))++;
    }

    P(&mutex);
    readcnt--;                                                      // 현재 크리티컬 섹션에 있는 reader의 수가 0일 때 -> 쓰기 가능해지도록 풀어줌
    if(readcnt == 0) {
        V(&w);
    }
    V(&mutex);
    return return_flag;
}

/* cache에서 요청한 url의 정보 쓰기 */
/* 세마포어를 이용해서 writer는 한번에 하나의 thread만 접근 가능 */
void writer(char *url, char *buf) {
    P(&w);

    int idx = 0;                        /* 작성할 곳을 가리키는 index */
    int max_cnt = 0;                    /* 가장 오래 방문 안한 일수 */

    /* 10개의 cache를 돌고 만약 비어있는 곳이 있으면 비어있는 곳에 index를 찾고, 없으면 가장 오래 방문 안한 곳의 index 찾기 */
    for(int i = 0; i < 10; i++) {
        if(*(cache[i].flag) == 0) {     /* 캐시 안에 값이 들어있지 않은 공간이 있으면 */
            idx = i;
            break;
        }
        if(*(cache[i].cnt) > max_cnt) { /* 캐시가 다 차 있을 때는 가장 오래 방문 안한 공간을 찾아야 함 (가장 큰 cnt 값을 찾음) */
            idx = i;
            max_cnt = *(cache[i].cnt);
        }
    }
    /* 해당 index에 cache 작성 */
    *(cache[idx].flag) = 1;             /* 써줘서 공간이 찼으므로 1 */
    strcpy(cache[idx].url, url);        /* cache의 url에 url 복사 */
    strcpy(cache[idx].content, buf);    /* cache의 content에 buf에 있는 내용 복사 */
    *(cache[idx].cnt) = 1;              /* 가장 최근에 방문했으므로 cnt 1로 설정 */

    V(&w);
}