#include "csapp.h"
/*
* argc: 메인 함수에 전달 되는 데이터의 수
* argv: 메인 함수에 전달 되는 실질적인 정보
*/
int main(int argc, char **argv) {
    /*
    * addrinfo의 구조
    * int flags             - 추가적인 옵션을 정의 할 때 사용함. 여러 flag를 OR연산해서 생성
    * int family            - address family를 나타냄. AF_INET(IPv4), AF_INET6(IPv6), AF_UNSPEC(정해지지 않았을 때)
    * int socktype          - socktype을 나타냄. SOCK_STREAM(스트림 소켓, 즉, TCP), SOCK_DGRAM(데이터그램 소켓, 즉, UDP)
    * int protocol          - IPv4와 IPv6에 대한 IPPROTO_xxx와 같은 값을 가짐. 0을 넣을 시 모든 프로토콜 관련 정보를 얻을 수 있고, 특정 프로토콜을 지정해 줄 수 있음
    * socklen_t addrlen     - socket 주소인 addr의 길이를 가짐
    * sokaddr *addr         - sockaddr 구조체 포인터
    * char *canonname       - 호스트의 canonical name을 나타냄, hostname을 가리키는 포인터
    * addrinfo *next        - 주소정보 구조체 addrinfo는 linked list이다. 즉, 다음 노드를 가리키는 포인터
    */
    struct addrinfo *p, *listp, hints;
    char buf[MAXLINE];                  /* 8192 */
    char buf2[MAXLINE]; 
    int rc, flags;                      /* flags: addrinfo의 flags를 설정해줄 변수 */
    /* 메인 함수에 전달되는 데이터의 수가 2개가 아닐 때 */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <domain name>\n, argv[0]", argv[0]);  /* stderr: 에러를 표시할 때 사용, 버퍼 없이 바로 출력 */
        exit(0);
    }
    /*
     * ./hostinfo만 입력 했을 때
     * usage: ./hostinfo <domain name>
     * 이렇게 뜸
     */

    /*
    * void *memset(void *ptr, int value, size_t num);
    * memset 함수는 메모리의 내용을 원하는 크기만큼 특정 값으로 세팅할 수 있는 함수
    * 첫 번째 인자 ptr: 세팅하고자 하는 메모리 시작 주소
    * 두 번째 인자 value: 메모리에 세팅하고자 하는 값(int 타입으로 받지만 내부에서는 unsigned char로 변환되어서 저장)
    * 세 번째 인자 num: 길이, 바이트 단위로써 메모리의 크기 한조각 단위의 길이를 말함
    */
    memset(&hints, 0, sizeof(struct addrinfo));         /* hints의 주소에 0으로 addrinfo의 크기만큼 세팅 */
    hints.ai_family = AF_INET;                          /* IPv4 */
    hints.ai_socktype = SOCK_STREAM;                    /* TCP */
    /*
    * getaddrinfo(const char *hostname, const char *service, const struct addrinfo *hints, struct addrinfo **result);
    * ip 주소를 이용해 도메인 정보를 얻어오는 것
    * ip 주소와 port 번호 받아서 그 소켓에 대응되어 있는 모든 소켓 주소 구조체를 가리키는 addrinfo 구조체의 연결리스트를 가리키는 포인터를 리턴
    * domain address를 받아서 네트워크 주소 정보(IP address에 들어있는 정보)를 가져오는 함수
    * 첫 번째 인자 hostname: 호스트 이름 혹은 주소 문자열(주소 문자열은 IPv4는 .으로 구분하는 10진 주소 문자열이거나 IPv6의 16진수 문자열)
    * 두 번째 인자 service: 서비스 이름 혹은 10진수로 표현한 포트 번호 문자열
    * 세 번째 인자 hints: 희망하는 유형을 알려주는 힌트를 제공함, 예를 들어, IPv4주소만 받고 싶으면 hints.ai_family에 값을 지정해주면 됨, 별도의 hint를 제공하지 않을 경우, NULL을 넣음
    * 네 번째 인자 result: DNS서버로부터 받은 네트워크 주소 정보를 돌려주는 output 매개변수, result의 내용중 필요한 것들은 적절히 copy하여 사용자의 변수로 옮겨두어야 하며, result는 사용이 끝나는 즉시 freeaddrinfo 함수로 메모리 해제를 해주어야함.
    * return: 성공하면 0, 실패하면 0이 아닌 값(에러 값들중 하나)을 반환
    */
    rc = getaddrinfo(argv[1], NULL, &hints, &listp); /* argv[1] 도메인에 대응되는 소켓주소구조체 리턴 받음 (port번호는 상관 없음) */
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }
    flags = NI_NUMERICHOST; /* 10진수 주소 스트링으로 리턴하라는 flag */
    p = listp;
    for (p; p; p = p->ai_next) {
        Getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, buf2, MAXLINE, flags);
        printf("%s\n", buf);
        // printf("%s\n", buf2); /* 소켓 구조체에 대응되는 port 번호가 service 버퍼로 복사되어 http가 출력됨 */
    }
    /* 할당한 메모리 해제 */
    Freeaddrinfo(listp);
    exit(0);
}