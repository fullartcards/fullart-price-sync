CC ?= cc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O2
PTHREAD_FLAGS ?= -pthread
CFLAGS += $(PTHREAD_FLAGS)
CPPFLAGS += $(shell curl-config --cflags)
CPPFLAGS += -MMD -MP
CPPFLAGS += -Isrc
LDLIBS += $(shell curl-config --libs)
LDLIBS += $(PTHREAD_FLAGS)

BIN := bin/fullart-price-sync
SRCS := src/main.c src/config.c src/http.c src/ebay/ebay.c src/justtcg/justtcg.c
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)
