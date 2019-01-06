
CMN_OPTS=-Wall -g

all: simpnwmon02 testll

simpnwmon02: simpnwmon02.c
	gcc $(CMN_OPTS) -I . -o simpnwmon02 simpnwmon02.c LinkedListRange.c

testll: LinkedListRange.c
	gcc $(CMN_OPTS) -D MODE_PROGRAM_LL -I . -o testll LinkedListRange.c

clean:
	rm -i simpnwmon02 testll || /bin/true

