/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL){ // 쿼리 스트링 값(1&2) 있으면
    p = strchr(buf, '&');
    *p = '\0'; // 구분자 부분을 NULL로 만들어서 공백을 기준으로 arg 가져올 수 있음
    strcpy(arg1, buf); // buf의 처음 포인터 값 1
    strcpy(arg2, p+1); // buf의 다음 포인터 값 2
    n1 = atoi(arg1); // 숫자로 변환
    n2 = atoi(arg2);
  }

  /* Make the response body */
  /* 응답 바디 */
  sprintf(content, "QUERY_STRING=%s", buf); // content에 HTML 코드 담음
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answeris: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  /* 응답 라인과 헤더 작성 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout); 
  // 버퍼 안에 존재하는 데이터를 비우는 즉시 출력함
  // 즉, 버퍼에 있는 데이터를 꺼내 출력장치로 보냄

  exit(0);
}
/* $end adder */
