/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 * HTTP 프로토콜: 클라이언트-서버 간 데이터를 주고 받는 응용 계층의 프로토콜
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

/*
* main: 명령줄에서 넘겨받는 포트로의 연결 요청을 받음
* doit: 한 개의 HTTP 트랜잭션을 처리 (원하는 요청과 응답을 모두 doit에 넣음)
* read_requesthdrs: 요청 헤더를 읽고 무시
* parse_uri: HTTP URI를 분석
* serve_static: 정적 컨텐츠를 클라이언트에게 서비스
* serve_dynamic: 동적 컨텐츠를 클라이언트에게 서비스
* clienterror: 에러 메시지를 클라이언트에게 보냄
*/

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* port 번호를 인자로 받아 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 함수를 호출한다. */
int main(int argc, char **argv) {
  /*
  * argc: 메인 함수에 전달 되는 데이터의 수
  * argv: 메인 함수에 전달 되는 실질적인 정보
  */
  int listenfd, connfd; 
  // 듣기식별자: 클라이언트로부터의 연결 요청 승낙 가능 상태 시 생성됨
  // 연결식별자: 서버가 연결 요청을 수락할 때마다 생성되고, 서버가 클라이언트에 서비스 하는 동안에만 존재
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소

  /* Check command line args */
  if (argc != 2) { // 한 개의 인자만을 명령어로 받음
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어줌 */
  listenfd = Open_listenfd(argv[1]); 

  /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 호출*/
  while (1) {
    /* 클라이언트에게서 받은 연결 요청을 accept한다. connfd = 서버 연결 식별자 */
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept

    /* 연결이 성공했다는 메세지를 위해 Getnameinfo를 호출하면서 hostname과 port가 채워짐 */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); 
    // 소켓 주소 구조체를 대응되는 호스트와 서비스 이름 스트링으로 변환
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* doit 함수를 실행 */
    doit(connfd);   // line:netp:tiny:doit

    /* 서버 연결 식별자를 닫아줌 */
    Close(connfd);  // line:netp:tiny:close

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

  }
}

/* 클라이언트의 HTTP 요청 라인을 확인해 정적, 동적 컨텐츠인지를 구분하고 각각의 서버에 보낸다. */
void doit(int fd) {
  int is_static;      // 정적 vs. 동적 컨텐츠
  struct stat sbuf;   // 파일의 정보를 담아줄 변수
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 클라이언트에게서 받은 요청(rio)으로 채워진다.
  char filename[MAXLINE], cgiargs[MAXLINE]; 
  rio_t rio;

  /* Read request line and headers */
  /* 클라이언트가 rio로 보낸 request 라인과 헤더를 읽고 분석한다. */
  Rio_readinitb(&rio, fd); // fd의 내용을 rio 구조체에 초기화. rio 안의 버퍼와 fd(서버의 connfd) 연결
  Rio_readlineb(&rio, buf, MAXLINE); //  rio(==connfd)에 있는 string 한 줄(응답 라인)을 모두 buf로 옮김

  /* 사용자 버퍼인 buf에 connfd의 값들이 들어간 것 */

  printf("Request headers:\n");
  printf("%s", buf); // 요청 라인 buf = "GET /godzilla.gif HTTP/1.1\0"을 표준 출력만 해줌!
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 포멧을 지정하여 읽어옴

  /* 요청 method가 GET이 아니면 종료. main으로 가서 연결 닫고 다음 요청 기다림 */
  if (strcasecmp(method, "GET")) { // 대소문자를 구분하지 않고 스트링 비교
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* 요청 라인을 뺀 나머지 요청 헤더들을 프린트 */
  read_requesthdrs(&rio);

  /* 컨텐츠의 유형(정적, 동적)을 파악한 후 각각의 서버에 보냄 */
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);  // 클라이언트 요청 라인에서 받아온 uri를 이용해 정적/동적 컨텐츠를 구분 (정적이면 1)
  if (stat(filename, &sbuf) < 0) { // 파일의 상태 및 정보를 읽는 함수 -> sbuf에 파일 정보 값 들어있음
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) { /* Serve static content */
    if (! (S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 정규 파일인지 확인
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 동적 서버에 인자를 같이 보냄
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));

}

/* 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 그냥 프린트한다 */
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) { // 더 읽을 거 있을 수 있으니까 \r\n 을 기준으로 끊기
    Rio_readlineb(rp, buf, MAXLINE); // rp(==connfd)에 있는 string 한 줄씩 모두 buf로 옮김
    printf("%s", buf);
  }
  return;
}

/* uri를 받아 요청받은 파일의 이름(filename)과 요청 인자(cgiarg)를 채워준다. */
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { /* Static Content */
    // strstr(str1, str2);
    // str1에서 str2와 일치하는 문자열이 있는지 확인 -> 일치하는 문자열이 있으면 해당 위치의 포인터(char*) 반환
    strcpy(cgiargs, ""); // cgiargs를 ""로 만듦
    strcpy(filename, ".");
    strcat(filename, uri); // uri를 filename에 연결
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html"); // home.html을 filename에 연결
    }

    // 정적 컨텐츠면 1 리턴
    return 1;
  }
  else { /* Dynamic Content */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 출력할 결과 값을 변수(buf)에 저장하는 함수
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // buf에 있는 값이 fd로 써짐 (파일이 수정됨)
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}

/* get_filetype - Derive file type from filename */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else if (strstr(filename, ".mpeg"))
        strcpy(filetype, "video/mpeg");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) { // fd: 클라이언트 식별자, filename: uri의 파일경로 (별칭), cgiargs: 쿼리스트링
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 버퍼 뒤에 붙임
    Rio_writen(fd, buf, strlen(buf)); // 버퍼에서 꺼내서 클라이언트 식별자 file에 써줌
    sprintf(buf, "Server: Tiny Web Server\r\n"); 
    Rio_writen(fd, buf, strlen(buf)); 

    // 서버가 GET /cgi-bin/adder?15000&213 HTTP/1.1 과 같은 요청을 받은 후에

    // 자식 프로세스 생성, 이 아래 내용은 각각 실행 자식만 조건문 안 실행
    if (Fork() == 0) 
    /* Real server would set all CGI vars here */
    {
        // 자식 프로세스는 CGI 환경변수 QUERY_STRING을 "15000&213"으로 설정해줌
        // adder 프로그램은 런 타임에 리눅스 getenv()로 이 값을 참조 가능
        setenv("QUERY_STRING", cgiargs, 1); // a=1&b=1

        // 1이면 원래 있던거 지우고 다시 넣기
        // 파일 복사하기
        // 표준 출력이 fd에 저장되게 만드는 듯
        // 원래는 STDOUT_FILENO -> 1 임. 표준 파일 식별자.
        Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
        // 자식은 자식의 표준 출력을 연결 파일 식별자로 재지정
        // 파일 네임의 실행 코드를 가지고 와서 실행,
        // 즉 자식 프로세스에는 기존 기능이 전부 없어지고 파일이 실행되는 것임.

        // /cgi-bin/adder 프로그램을 자식의 컨텍스트에서 실행
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); // 자식 끝날때까지 기다림
}