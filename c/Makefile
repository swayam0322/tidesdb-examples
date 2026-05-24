CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
LDFLAGS ?= -ltidesdb -lzstd -llz4 -lsnappy -lpthread

COMMON_SRC = common/utils.h

EXPERIMENTS = \
	01-basic-crud \
	02-column-families \
	03-transactions \
	04-benchmarks \
	05-ttl \
	06-crash-recovery \
	07-compression

BINS = $(foreach exp,$(EXPERIMENTS),$(exp)/$(exp))

.PHONY: all clean $(EXPERIMENTS)

all: $(EXPERIMENTS)

$(EXPERIMENTS): %: %/main.c $(COMMON_SRC)
	$(CC) $(CFLAGS) -I. -o $@/$@ $@/main.c $(LDFLAGS)

clean:
	rm -f $(BINS)
	rm -rf */data/