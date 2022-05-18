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

/* 반복실행 "서버"로 명령줄에서 넘겨받은 포트로의 연결 요청을 들음 */
/* port 번호를 인자로 받아 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 함수를 호출한다. */
/* 연결요청을 받을 준비가 된 듣기 식별자를 생성하고 클라이언트와 서버를 */
int main(int argc, char **argv) {
  /*
  * argc: 메인 함수에 전달 되는 데이터의 수
  * argv: 메인 함수에 전달 되는 실질적인 정보
  */
  int listenfd, connfd; 
  // 듣기식별자: 클라이언트로부터의 연결 요청 승낙 가능 상태 시 생성됨
  // 연결식별자: 서버가 연결 요청을 수락할 때마다 생성되고, 서버가 클라이언트에 서비스 하는 동안에만 존재
  char hostname[MAXLINE], port[MAXLINE];
  // hostname: 접속한 클라이언트 ip, port: 접속한 클라이언트 port 
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
    // 내 컴퓨터 하나에서 요청해서 ip 주소는 같지만 포트 번호가 제각기 다름
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
  Rio_readinitb(&rio, fd); // fd의 내용을 rio 구조체에 초기화. rio 안의 내부 버퍼와 fd(서버의 connfd) 연결
  Rio_readlineb(&rio, buf, MAXLINE); //  rio(==connfd)에 있는 string 한 줄(응답 라인)을 읽고 buf로 옮김

  // 사용자가 GET / HTTP/1.0을 입력해줬을 때의 상태.. port번호는 어디서 입력해줬나?
  // fd에 담겨있는 건 사용자가 입력한 GET / HTTP/1.0이 유일?

  /* 사용자 버퍼인 buf에 응답 라인이 들어간 것 */

  printf("Request headers:\n");
  printf("%s", buf); // 요청 라인 buf = "GET /godzilla.gif HTTP/1.1\0"을 표준 출력만 해줌!
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 포멧을 지정하여 읽어옴

  /* 요청 method가 GET이 아니면 종료. main으로 가서 연결 닫고 다음 요청 기다림 */
  // 같으면 0이므로 안에 실행하지 않음 -> 다를 때만 실행됨
  if (strcasecmp(method, "GET")) { // 대소문자를 구분하지 않고 스트링 비교 -> 같으면 0 리턴 // 0 보다 작으면 string1 < string2
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* 요청 라인을 뺀 나머지 요청 헤더들을 프린트 */
  // 위에서 요청 라인을 한 줄 읽어서 포인터가 그 다음 요청 헤더들을 가리킴
  // 포인터가 컨텐츠를 가리키도록 요청 헤더들을 모두 읽어줌
  read_requesthdrs(&rio);

  /* 컨텐츠의 유형(정적, 동적)을 파악한 후 각각의 서버에 보냄 */
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);  // 클라이언트 요청 라인에서 받아온 uri를 이용해 정적/동적 컨텐츠를 구분 (정적이면 1)
  
  /* stat(file, *buffer) : file의 상태를 buffer에 넘긴다. */
  /* 여기서 filename : 클라이언트가 요청한 서버의 컨텐츠 디렉토리 및 파일 이름 */
  if (stat(filename, &sbuf) < 0) { // 요청받은 filename을 찾아서 그 파일의 속성을 sbuf에 넘김 
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) { /* Serve static content */
    if (! (S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // !(정규 파일이다) || !(읽기 권한 있다) (둘 중 하나라도 만족 못하면)
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 읽기 가능한 정적 파일의 filename 확인해보기
    // printf("정적 filename 입니다 %s \n", filename);

    // 정적 서버에 연결식별자, 파일명, 해당 파일 사이즈 보냄
    // fd를 제외하고 모두 NULL인 상태
    serve_static(fd, filename, sbuf.st_size);
  }
  else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){ // !(정규 파일이다) || !(실행 권한 있다) (둘 중 하나라도 만족 못하면)
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 실행 가능한 정적 파일의 filename 확인해보기
    // printf("동적 filename 입니다 %s \n", filename);

    // 동적 서버에 연결식별자, 파일명, 해당 파일 사이즈 보냄
    // fd를 제외하고 모두 NULL인 상태
    serve_dynamic(fd, filename, cgiargs); 
  }
}

/* HTTP 응답을 응답라인과 응답 본체(HTML)를 서버 소켓을 통해 클라이언트에 보낸다. */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  
  char buf[MAXLINE], body[MAXBUF];
  
  /* 응답 본체: 요청한 컨텐츠들이 포함되고 화면에서 볼 수 있음 */
  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>"); // 출력할 결과값을 변수(body)에 저장하는 함수
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* 응답 헤더: 클라이언트는 개발자 도구로 들어가면 볼 수 있음 */
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // buf에 있는 값이 fd로 써짐 (파일이 수정됨)
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf)); 

  /* HTML 컨텐츠로 담은 body가 fd로 써짐 -> HTML 수정됨 */
  Rio_writen(fd, body, strlen(body));

}

/* 클라이언트가 버퍼 rp에 보낸 요청 라인을 제외한 나머지 요청 헤더들을 그냥 프린트한다 */
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

  /* uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 리턴한다.*/
  if (!strstr(uri, "cgi-bin")) { /* Static Content */
    // strstr(str1, str2);
    // str1에서 str2와 일치하는 문자열이 있는지 확인 -> 일치하는 문자열이 있으면 해당 위치의 포인터(char*) 반환
    strcpy(cgiargs, ""); // 정적이니까 cgiargs는 아무것도 없다.
    strcpy(filename, "."); // 현재경로에서부터 시작 ./path ~~
    strcat(filename, uri); // filename에 uri를 연결

    /* 예시
      uri : /godzilla.jpg
      ->
      cgiargs : 
      filename : ./godzilla.jpg
    */

    // 만약 uri뒤에 '/'이 있다면 그 뒤에 home.html을 붙인다.
    // 내가 브라우저에 http://localhost:8000만 입력하면 바로 뒤에 '/'이 생기는데,
    // '/' 뒤에 home.html을 붙여 해당 위치 해당 이름의 정적 컨텐츠가 출력
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html"); // home.html을 filename에 연결
    }

    // 정적 컨텐츠면 1 리턴
    return 1;
  }
  else { /* Dynamic Content */
    ptr = index(uri, '?'); // uri 중 ?가 있는 인덱스를 ptr에 넣어줌

    // '?'가 있으면 cgiargs를 '?' 뒤 인자들과 값으로 채워주고 ?를 NULL로 만든다.
    if (ptr) {
      strcpy(cgiargs, ptr+1); // 뒤에 있는 문자열 전체를 앞에 있는 변수로 복사하는 함수
      *ptr = '\0';
    }
    else { // '?'가 없으면 그냥 아무것도 안 넣어준다.
      strcpy(cgiargs, "");
    }
    strcpy(filename, "."); // 현재 디렉토리에서 시작
    strcat(filename, uri); // uri 넣어준다.

    /* 예시
      uri : /cgi-bin/adder?123&123
      ->
      cgiargs : 123&123
      filename : ./cgi-bin/adder
    */

    return 0;
  }
}

/*
* 클라이언트가 원하는 정적 컨텐츠 디렉토리를 받아온다. 
* 응답 라인과 헤더를 작성하고 서버에게 보낸다. 
* 그 후 정적 컨텐츠 파일을 읽어 그 응답 본체를 클라이언트에 보낸다.
*/
void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client 클라이언트에게 응답 헤더 보내기*/
  /* 응답 라인과 헤더 작성 */
  get_filetype(filename, filetype); // 파일 타입 찾아오는 함수 호출
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 출력할 결과값을 변수(buf)에 저장하는 함수 // 응답 라인 작성
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // 응답 헤더 작성
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  /* 응답 라인과 헤더를 클라이언트에게 보냄 */
  Rio_writen(fd, buf, strlen(buf)); // buf에 있는 응답 라인과 헤더가 fd로 써짐 (파일이 수정되어 클라이언트가 받음)
  printf("Response headers:\n");
  printf("%s", buf); // 커멘드 창에 buf의 내용 프린트

  /* 응답 바디를 클라이언트에게 보냄 */
  srcfd = Open(filename, O_RDONLY, 0); // filename의 이름을 갖는 파일을 읽기 권한으로 불러온다.
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 메모리에 파일 내용을 동적할당한다.
  Close(srcfd); // 열어줬던 파일을 닫음
  Rio_writen(fd, srcp, filesize); // 메모리 동적할당 된 파일 내용을 fd에 써줌 (클라이언트가 응답 바디 볼 수 있음)
  Munmap(srcp, filesize); // mmap 함수로 매핑해준 메모리 영역을 해제해줌
}

/* get_filetype - Derive file type from filename */
// filename을 조사해 웹 서버에서 읽을 수 있는 각각의 식별자에 맞는 MIME 타입을 filetype에 입력해준다.
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html")) // 브라우저 클라이언트한테 요청 받은 것
        strcpy(filetype, "text/html"); // 웹 서버에서 읽을 수 있는 MIME 타입으로 넣어줌
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

/*
* 클라이언트가 원하는 동적 컨텐츠 디렉토리를 받아온다. 
* 응답 라인과 헤더를 작성하고 서버에게 보낸다. 
* CGI 자식 프로세스를 fork하고 그 프로세스의 표준 출력을 클라이언트 출력과 연결한다.
*/
void serve_dynamic(int fd, char *filename, char *cgiargs) {
    // fd: 연결 식별자, filename: uri의 파일경로 (별칭), cgiargs: 쿼리스트링
    char buf[MAXLINE], *emptylist[] = {NULL};

    // 서버가 GET /cgi-bin/adder?15000&213 HTTP/1.1 과 같은 요청을 받음

    /* Return first part of HTTP response */
    /* Send response headers to client 클라이언트에게 응답 헤더 보내기*/
    /* 응답 라인과 헤더 작성 */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 버퍼에 넣음
    Rio_writen(fd, buf, strlen(buf)); // 버퍼에서 꺼내서 클라이언트 fd에 수정해줌
    sprintf(buf, "Server: Tiny Web Server\r\n"); 
    Rio_writen(fd, buf, strlen(buf)); 

    // 자식 프로세스 생성, 실행 자식만 조건문 실행 (반환값이 0이면 자식 프로세스)
    // Fork() 실행 시 실행하면 부모 프로세스와 자식 프로세스가 동시에 실행됨
    if (Fork() == 0) 
    {
        // 자식 프로세스의 CGI 환경변수 QUERY_STRING을 "15000&213"으로 설정해줌
        // 1이면 원래 있던거 지우고 다시 넣기
        // adder 프로그램은 런 타임에 리눅스 getenv()로 이 값을 참조 가능
        setenv("QUERY_STRING", cgiargs, 1); // 10&20

        // fd에 stdout 넣기 (표준 출력이 fd에 저장되게 만드는 것)
        // adder에서 fflush로 stdout만 출력하고 끝날 게 아니라, 
        // fd에 있는 데이터를 수정해줘야 하므로 stdout을 fd에 저장되게 만듦
        Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
        // 자식은 자식의 표준 출력을 연결 파일 식별자로 재지정

        // filename을 찾아서 /cgi-bin/adder 프로그램을 자식의 컨텍스트에서 실행
        // environ에 환경변수들 담겨 있음
        // emptylist는 변수 개수 맞춰주려고 넣어준 임의의 빈 배열
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); // 부모 프로세스가 자식 프로세스 끝날때까지 기다림
}
