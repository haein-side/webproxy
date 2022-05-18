#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400 // 캐시 하나하나 사이즈

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
int connect_endServer(char *hostname, int port, char *http_header);
void *thread(void *vargsp);
void init_cache(void);
int reader(int connfd, char *url);
void writer(char *url, char *buf);

/* end server info(브라우저 테스트를 위한) */
// static const char *end_server_host = "localhost";   /* end server의 hostname은 현재 localhost */
// static const int end_server_port = 9190;            /* proxy 서버의 소켓 번호 +1 */

/*
* argc: 메인 함수에 전달 되는 데이터의 수
* argv: 메인 함수에 전달 되는 실질적인 정보 (여기선 proxy에서 연결 대기 상태로 만들어주고 싶은 포트 번호가 들어감)
* ./proxy 8888
*/
int main(int argc, char **argv) {
    /* 프로그램 실행 시 port를 안썼으면 */
    // telnet localhost ~여기에 port번호 안 썼으면~
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    int listenfd, connfd;                  
    // 듣기식별자: 클라이언트로부터의 연결 요청 승낙 가능 상태 시 생성됨
    // 연결식별자: 서버가 연결 요청을 수락할 때마다 생성되고, 서버가 클라이언트에 서비스 하는 동안에만 존재
    char hostname[MAXLINE], port[MAXLINE]; /* hostname: 접속한 클라이언트 ip, port: 접속한 클라이언트 port */
    socklen_t clientlen;                   /* socklen_t 는 소켓 관련 매개 변수에 사용되는 것으로 길이 및 크기 값에 대한 정의를 내려준다 */
    struct sockaddr_storage clientaddr;    
    // 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소
    // 어떤 타입의 소켓 구조체가 들어올지 모르기 때문에 충분히 큰 소켓 구조체로 선언 
    // pthread_t tid;

    /* listenfd: 이 포트에 대한 프록시의 듣기 소켓 오픈 */
    // telnel localhost 8888
    listenfd = Open_listenfd(argv[1]);
    pthread_t tid; // 스레드가 생성되면 스레드 id인 tid가 생성됨

    while (1) {
        clientlen = sizeof(clientaddr);                                                 /* 소켓 구조체 크기 */
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                       /* 연결 요청 큐에 아무것도 없을 경우 기본적으로 연결이 생길때까지 호출자를 막아둠, 즉 대기 상태 (./proxy 8888 했을 때의 경우) */
                                                                                        /* telnet localhost 8888로 연결요청이 생기면 연결 요청 큐에 요청이 담김
                                                                                           addr 내에 클라이언트의 소켓주소가 담기고
                                                                                           connfd가 생겨나는 것 */
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); /* clientaddr: SA 구조체로 형변환, 클라이언트 소켓 정보를 가져옴 */

        // telnet localhost 8888을 실행했을 때 출력됨 (편하게 생각하면 telnet localhost 8888은 클라이언트가 연결요청을 서버에 보낸 것임 -> Accept 함수 실행되고 connfd가 생성됨)
        printf("Accepted connection from (%s, %s)\n", hostname, port);                  /* 어떤 주소와 포트 번호를 가진 client가 들어왔는지 print */

        Ptrhread_create(&tid, NULL, thread, (void *)connfd);

        doit(connfd);                                                                   /* 트랜잭션 수행 */
        
        /* 연결이 끝났다고 print 하고 식별자(파일 디스크립트)를 닫아줌 */
        // printf("endoffile\n");
        // Close(connfd);
    }
}

void *thread(void *vargs) {
    int connfd = *((int *)vargs);
    Pthread_detach(pthread_self()); 
    // 연결가능한 상태로 스레드가 유지되면 스레드 하나를 종료시켜도 연결된 스레드가 죽을 때까지 기다림
    // 죽이면 자기 자신만 죽이고 싶어서 자신을 분리해주는 것
    doit(connfd);
    Close(connfd);
}

/* 클라이언트의 요청을 입력받고 요청에 따른 트랜잭션 (알맞은 서버로 요청 보내줌) 처리 */
void doit(int connfd) {
    int end_serverfd;

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; /* buf: request 헤더 정보 담을 공간*/
    char endserver_http_header[MAXLINE];                                /* 서버에 보낼 헤더 정보를 담을 공간 */
    char hostname[MAXLINE], path[MAXLINE];                              /* hostname: 클라이언트가 요청한 IP담을 공간, path: 경로 담을 공간 */
    rio_t rio, server_rio;                                              /* rio: client rio 구조체, server_rio: proxy의 rio 구조체 */
    int port;                                                           /* port 담을 변수 */

    Rio_readinitb(&rio, connfd);                                        /* rio 구조체 초기화 */
    // telnet localhost 8888해줬을 때 여기서 클라이언트의 입력을 기다리고 있음
    // GET http://localhost:9020/index.html을 clientfd에 입력해주면 연결된 connfd에 전송되고 connfd와 연결된 rio에 전송되고 buf에 담김
    Rio_readlineb(&rio, buf, MAXLINE);                                  /* buf에 fd에서 읽을 값이 담김 */
    sscanf(buf, "%s %s %s", method, uri, version);                      /* sscanf는 첫 번째 매개 변수가 우리가 입력한 문자열, 두 번째 매개 변수 포맷, 나머지 매개 변수에 포맷에 맞게 데이터를 읽어서 인수들에 저장 */

    // 캐시에 데이터 써주는 건 여기에서 해주면 될 듯

    /* GET이나 HEAD가 아닐 때 error 메시지 출력 */
    // 같으면 0이므로 둘 다 0이면 false가 되어 안에 내용이 실행되지 않음
    // 하나만 같고 다른 건 달라서 0, 1 이면 && 연산에 의해 둘 다 1은 아니므로 False가 됨 -> 즉 하나만 GET이거나 HEAD여도 안에 내용 실행 안 됨
    // 둘 다 달라서 0이 아니면 True가 됨 -> 즉 둘 다 method가 GET, HEAD가 아닐 때 안에 내용이 실행되는 것
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
        printf("501 Not implemented Tiny does not implement this method");
        return;
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
    end_serverfd = connect_endServer(hostname, port, endserver_http_header);

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
    while((n = rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        printf("proxy received %ld bytes,then send\n",n);
        Rio_writen(connfd, buf, n);
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
int connect_endServer(char *hostname, int port, char *http_header) {
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