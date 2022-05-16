/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;
  char *method = getenv("REQUEST_METHOD");

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL){ // 쿼리 스트링 값(1&2) 있으면
    p = strchr(buf, '&');
    *p = '\0'; // 구분자 부분을 NULL로 만들어서 공백을 기준으로 arg 가져올 수 있음

    // strcpy(arg1, buf); // buf의 처음 포인터 값 1
    // strcpy(arg2, p+1); // buf의 다음 포인터 값 2
    // n1 = atoi(arg1); // 숫자로 변환
    // n2 = atoi(arg2);

    /* 참고: sscanf(buf, "%s %s %s", method, uri, version); // buf에서 포멧을 지정하여 읽어옴 */
    sscanf(buf, "first=%d", &n1);   // first=(뒤의 값)을 &n1에 넣어줌
    sscanf(p+1, "second=%d", &n2);  // second=(뒤의 값)을 &n2에 넣어줌
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

  /* 메서드가 HEAD가 아닐 경우만 응답 바디를 출력 */
  if (strcasecmp(method, "HEAD") != 0){
      printf("%s", content);  // 응답 바디를 클라이언트 표준 출력!
  }

  fflush(stdout); 
  // 버퍼 안에 존재하는 데이터를 비우는 즉시 출력함
  // 즉, 버퍼에 있는 데이터를 꺼내 출력장치로 보냄

  exit(0);
}
/* $end adder */