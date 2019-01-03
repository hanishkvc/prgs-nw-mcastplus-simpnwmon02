
CMN_OPTS=-Wall -g

all: simpnwmon02 testll

simpnwmon02: simpnwmon02.c
	gcc $(CMN_OPTS) -o simpnwmon02 simpnwmon02.c

testll: LinkedListRange.c
	gcc $(CMN_OPTS) -D MODE_PROGRAM_LL -o testll LinkedListRange.c

clean:
	rm -i simpnwmon02 testll || /bin/true

