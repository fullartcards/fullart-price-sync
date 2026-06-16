CC ?= cc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O2
PTHREAD_FLAGS ?= -pthread
PG_CONFIG ?= pg_config
LIBPQ_PREFIX ?= $(shell brew --prefix libpq 2>/dev/null || true)
ifeq ($(LIBPQ_PREFIX),)
ifneq ($(wildcard /opt/homebrew/opt/libpq),)
LIBPQ_PREFIX := /opt/homebrew/opt/libpq
else ifneq ($(wildcard /usr/local/opt/libpq),)
LIBPQ_PREFIX := /usr/local/opt/libpq
endif
endif
PG_INCLUDEDIR := $(shell $(PG_CONFIG) --includedir 2>/dev/null)
PG_LIBDIR := $(shell $(PG_CONFIG) --libdir 2>/dev/null)
ifeq ($(PG_INCLUDEDIR),)
ifneq ($(wildcard $(LIBPQ_PREFIX)/include/libpq-fe.h),)
PG_INCLUDEDIR := $(LIBPQ_PREFIX)/include
PG_LIBDIR := $(LIBPQ_PREFIX)/lib
endif
endif
DB_SRC := src/db/db_stub.c
CFLAGS += $(PTHREAD_FLAGS)
CPPFLAGS += $(shell curl-config --cflags)
CPPFLAGS += -MMD -MP
CPPFLAGS += -Isrc
ifneq ($(PG_INCLUDEDIR),)
CPPFLAGS += -I$(PG_INCLUDEDIR)
DB_SRC := src/db/db.c
LDLIBS += -lpq
endif
LDLIBS += $(shell curl-config --libs)
LDLIBS += $(PTHREAD_FLAGS)
ifneq ($(PG_LIBDIR),)
LDLIBS += -L$(PG_LIBDIR)
endif

BIN := bin/fullart-price-sync
SRCS := src/main.c src/config.c src/http.c $(DB_SRC) src/ebay/ebay.c src/justtcg/justtcg.c
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
	find src \( -name '*.o' -o -name '*.d' \) -delete

-include $(DEPS)
