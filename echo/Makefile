CC = gcc
make: hostinfo
test: hostinfo
	./hostinfo naver.com twitter.com 
csapp.o: csapp.c csapp.h
	$(CC) -c csapp.c
hostinfo.o: hostinfo.c csapp.h
	$(CC) -c hostinfo.c
hostinfo: hostinfo.o csapp.o
	$(CC) hostinfo.o csapp.o -o hostinfo
clean:
	rm -rf csapp.o hostinfo.o hostinfo

# make: echoclient
# test: echoclient.o
#     ./echoclient 127.0.0.1 9190 # 이 도메인의 포트 번호로 들어가라는 것
# csapp.o: csapp.c csapp.h
#     $(CC) -c csapp.c
# echoclient.o: echoclient.c csapp.h
#     $(CC) -c echoclient.c
# echoclient: echoclient.o csapp.o
#     $(CC) echoclient.o csapp.o -o echoclient
# clean:
#     rm -rf *.o hostinfo echoclient echoserveri

# 왜 실행하려면 ./hostinfo naver.com 이렇게 써줘야 하는 것? test로 그냥 쓰면 왜 안 되나?