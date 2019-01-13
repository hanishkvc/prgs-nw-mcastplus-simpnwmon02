
CMN_OPTS=-Wall -g
LL_FILES=LinkedListRange.c fileutils.c

all: simpnwmon02 testll

simpnwmon02: simpnwmon02.c $(LL_FILES)
	gcc $(CMN_OPTS) -I . -o simpnwmon02 simpnwmon02.c $(LL_FILES)

testll: $(LL_FILES)
	gcc $(CMN_OPTS) -D MODE_PROGRAM_LL -I . -o testll $(LL_FILES)

clean:
	rm -i simpnwmon02 testll || /bin/true

