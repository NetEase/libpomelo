CFLAGS = -g -I./include -I./deps/uv/include -I./deps/jansson/src
LDFLAGS = -L./deps/uv -L./deps/jansson/lib -L. -lm -luv -ljansson -pthread -lpomelo -framework CoreServices
OBJDIR := out

OBJS += src/client.o
OBJS += src/network.o
OBJS += src/package.o
OBJS += src/pkg-handshake.o
OBJS += src/pkg-heartbeat.o
OBJS += src/map.o
OBJS += src/listener.o

OBJS := $(addprefix $(OBJDIR)/,$(OBJS))

all: libpomelo.a

$(OBJDIR)/src/%.o: src/%.c include/pomelo-client.h \
									 include/pomelo-protocol/package.h \
									 include/pomelo-private/map.h \
									 include/pomelo-private/listener.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

libpomelo.a: $(OBJS)
	$(AR) rcs $@ $^

test: test/test.c libpomelo.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o test/test test/test.c

connect: test/connect.c libpomelo.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o test/connect test/connect.c

hashtest: test/hashtest.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o test/hashtest test/hashtest.c

run:
	./test/connect

clean:
	rm out -rf
	rm libpomelo.a

.PONY: clean all run touch