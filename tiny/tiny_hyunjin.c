/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/*
* argc: 메인 함수에 전달 되는 데이터의 수
* argv: 메인 함수에 전달 되는 실질적인 정보
*/
int main(int argc, char **argv) {   // 명령줄에서 넘겨받은 포트로의 연결 요청을 듣는다
    /*
    * addrinfo의 구조
    * int flags             - 추가적인 옵션을 정의 할 때 사용함. 여러 flag를 OR연산해서 생성
    * int family            - address family를 나타냄. AF_INET(IPv4), AF_INET6(IPv6), AF_UNSPEC(정해지지 않았을 때)
    * int socktype          - socktype을 나타냄. SOCK_STREAM(스트림 소켓, 즉, TCP), SOCK_DGRAM(데이터그램 소켓, 즉, UDP)
    * int protocol          - IPv4와 IPv6에 대한 IPPROTO_xxx와 같은 값을 가짐. 0을 넣을 시 모든 프로토콜 관련 정보를 얻을 수 있고, 특정 프로토콜을 지정해 줄 수 있음
    * socklen_t addrlen     - Length of socket address. socket 주소인 addr의 길이를 가짐
    * sokaddr *addr         - Socket address for socket. sockaddr 구조체 포인터
    * char *canonname       - 호스트의 canonical name을 나타냄, hostname을 가리키는 포인터
    * addrinfo *next        - 주소정보 구조체 adrinfo는 linked list이다. 즉, 다음 노드를 가리키는 포인터
    */
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; // 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);  // 듣기 소켓 오픈
    /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 호출*/
    while (1) { 
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept 반복적으로 연결 요청 접수
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);   // line:netp:tiny:doit  트랜잭션 수행
        Close(connfd);  // line:netp:tiny:close 자신쪽의 연결 끝 닫기
    }
}

void doit(int fd)   // 한 개의 HTTP 트랜잭션을 처리한다
{
    int is_static;      // 정적 컨텐츠인지 -> Static = 1, Dynamic = 0
    struct stat sbuf;   // 파일의 정보를 담을 곳
    char buf[MAXLINE];  // request의 헤더가 담길 곳
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];
    char cgiargs[MAXLINE];  // 쿼리스트링을 담는 공간(?이후의 스트링) CGI 인자 스트링
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);            // open한 식별자마다 한 번 호출된다. 식별자 fd를 주소 rp에 위치한 rio_t 타입의 읽기 버퍼와 연결한다.
                                        // 읽기 버퍼의 포맷을 초기화한다. 한 개의 빈 버퍼를 설정하고, 이 버퍼와 한 개의 오픈한 파일 식별자를 연결한다.
    Rio_readlineb(&rio, buf, MAXLINE);  // 다음 텍스트 줄을 파일 rp(종료 새 줄 문자를 포함해서)에서 읽고, 이것을 메모리 위치 usrbuf로 복사하고, 텍스트 라인을 널(0) 문자로 종료시킨다. 
                                        // 최대 maxlen-1개의 바이트를 읽으며, 종료용 널 문자를 위한 공간을 남겨둔다. 
        // rio_read : 내부 버퍼에서 사용자 버퍼로 min(n, rio_cnt) 바이트를 전송한다. 내부 버퍼가 비어있으면 read 호출로 다시 채운다.
        // n : 사용자가 요청한 바이트 수, rio_cnt : 내부 버퍼에서 읽지 않은 바이트 수
    printf("Request headers:\n");
    printf("%s, buf");
    sscanf(buf, "%s %s %s", method, uri, version);  // 헤더
    if (strcasecmp(method, "GET")) {    // method가 GET이면 0반환 -> 메인 루틴으로 돌아가고 연결닫고 다음 연결 기다림, GET이 아니면 0이 아닌 수 반환
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio); // 함수를 호출해서 이들을 읽고 무시한다.

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);  // Static = 1, Dynamic = 0
    if (stat(filename, &sbuf) < 0) {    // 파일이 디스크 상에 있지 않으면 에러
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    if (is_static) { /* Serve static content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {    // 파일인지, 읽기권한이 있는지(실행파일, 사용권한 등-> html은 읽어오기만 하면 됨) 확인
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else {          /* Serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {    // 실행파일인지 확인 -> 실행해서 결과값을 받고 싶으니까, 읽기권한 필요없다
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    // HTML 응답은 본체에서 컨텐츠의 크기와 타입을 나타내야 한다. 여기에선 컨텐츠를 한 개의 스트링으로 만듦
    sprintf(body, "<html><title>Tiny Error</title>");   
    sprintf(body, "%s(body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP Response */
    sprintf(body, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(buf, buf, strlen(buf));
    sprintf(body, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(body, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);    
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
URI에 cgi-bin이 있으면 동적 컨텐츠, 아니면 정적 컨텐츠
정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이고, 실행파일의 홈 디렉토리는 /cgi-bin이라고 가정
기본 파일 이름은 ./home.html
 */
int parse_uri (char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) { /* Static content */ // strstr -> 문자열 포인터 반환
        strcpy(cgiargs, "");    // 인자스트링을 지우고
        strcpy(filename, ".");  // uri를 ./index.html같은 상대 리눅스 경로이름으로 변환
        strcpy(filename, uri);
        if (uri[strlen(uri)-1] == '/')      // uri가 /로 끝나면 
            strcat(filename, "home.html");  // 기본 파일 이름을 추가한다.
        return 1;
    }
    else {  /* Dynamic content 0*/
        ptr = index(uri, '?');      // 모든 CGI 인자들을 추출하고
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");
        strcpy(filename, ".");      // 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환
        strcat(filename, uri);
        return 0;
    }
}

void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    // 클라이언트에 응답줄과 응답헤더를 보낸다. 빈줄 한개가 헤더를 종료하고 있음
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContet-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    // 요청한 파일의 내용을 연결식별자fd로 복사해서 응답 본체를 보낸다.
    srcfd = Open(filename, O_RDONLY, 0);
        // open(pathname, flags, mode)   
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); 
        /* 요청한 파일을 가상메모리 영역으로 매핑한다. 파일 srcfd의 첫번째 filesize바이트를 주소 srcp에서 시작하는 사적 읽기-허용 가상메모리 영역으로 매핑한다.
        // mmap(addr, len, prot, flags, fd, offset)
        */
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

// get_filetype - Derive file type from filename
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".git"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1. 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) /* Child */  // 이 아래 내용은 각각 실행, 자식만 조건 문 안 실행
    /* Real server would set all CGI vars here */
    {
        // REPLACE가 0이 아니면 덮어쓰기 /* 요청 메서드를 환경변수에 추가한다. */
        setenv("QUERY_STRING", cgiargs, 1); // a=1&b=1
        // 파일 복사하기
        // 표준 출력이 fd에 저장되게 만드는 듯
        // 원래는 STDOUT_FILENO -> 1 임. 표준 파일 식별자.
        // clientfd 출력을 CGI 프로그램의 표준 출력과 연결한다.
        // CGI 프로그램에서 printf하면 클라이언트에서 출력됨!!
        Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
            // dup(fd) : fd를 복제, 같은 파일에 새로운 fd 반환
            // dup2(fd,fd2): fd 내용을 fd2로 복제, fd2를 닫고 동일한 파일에서 연다.
        // 파일 네임의 실행 코드를 가지고 와서 실행,
        // 즉 자식 프로세스에는 기존 기능이 전부 없어지고 파일이 실행되는 것임.
        Execve(filename, emptylist, environ); /* Run CGI program */
            // 현재 프로세스를 ARGV(emptylist)와 ENVP(environ) 실행하는 걸(filename)로 바꿈. ARGV ENVP는 NULL포인터로 종료된다.
    }
    Wait(NULL); // 자식 끝날때까지 부모 프로세스가 sleep모드로 기다림
}