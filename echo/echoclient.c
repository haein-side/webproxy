#include "csapp.h"
int main(int argc, char **argv) {
    int clientfd;
    char *host, *port, buf[MAXLINE];
    /*
    * rio_t 구조체: 파일 디스크립터 정보는 물론 내부적인 임시 버퍼와 관련된 정보들도 포함
    * rio_fd: 기존 파일 디스크립터 정보
    * rio_cnt: 내부 버퍼의 읽은 바이트 수
    * rio_bufptr: 내부 버퍼에서 읽고 쓰는 위치를 지정하기 위한 포인터
    * rio_buf[RIO_BUFSIZE]: 내부 버퍼
    */
    rio_t rio;
    /*
    * IP주소와 port번호 입력을 안했을 때
    */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1]; /* IP주소 */
    port = argv[2]; /* port 번호 */
    clientfd = Open_clientfd(host, port);   /* clientfd 생성 */
    Rio_readinitb(&rio, clientfd);          /* clientfd로 rio_t 구조체를 초기화 */
    /*
    * 현재 stream 위치에서 끝까지 읽음
    * return: 성공-포인터, 오류 또는 파일의 끝-NULL
    */
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf)); /* buf에서 clientfd로 n바이트를 전송 */
        Rio_readlineb(&rio, buf, MAXLINE);      /* 텍스트 줄을 파일 rio에서 읽고, 이것을 메모리 위치 buf로 복사하고, 텍스트라인을 NULL로 종료 */
        Fputs(buf, stdout);
    }
    Close(clientfd);    /* 클라이언트가 프로세스를 종료할 때, 커널이 알아서 fd를 모두 닫아주지만, 열어둔 것은 다시 닫아주는게 좋음 */
    exit(0);            /* 정상적인 종료 */
}