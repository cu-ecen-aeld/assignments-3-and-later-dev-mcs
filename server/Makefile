BINARY=aesdsocket
LDFLAGS=-pthread

.PHONY:
all: ${BINARY}
	${CC} -Wall -Werror ${LDFLAGS} ${BINARY}.c -o ${BINARY}

.PHONY:
default: ${BINARY}
	${CC} -Wall -Werror ${LDFLAGS} ${BINARY}.c -o ${BINARY}

.PHONY:
clean:
	-rm ${BINARY}
