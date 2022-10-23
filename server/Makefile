BINARY=aesdsocket
LDFLAGS=-pthread

.PHONY:
all: ${BINARY}
	${CC} -O2 -Wall -Werror ${LDFLAGS} ${BINARY}.c -o ${BINARY}

.PHONY:
default: ${BINARY}
	${CC} -O2 -Wall -Werror ${LDFLAGS} ${BINARY}.c -o ${BINARY}

.PHONY:
clean:
	-rm ${BINARY}
