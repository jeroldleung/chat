SRCS = $(wildcard *.c)
DEBUGFLAGS = -g -O0 -Wall -std=c99
RELEASEFLAGS = -O2 -Wall -std=c99

debug: $(SRCS)
	$(CC) $(SRCS) -o server $(DEBUGFLAGS)

release: $(SRCS)
	$(CC) $(SRCS) -o server $(RELEASEFLAGS)

format:
	clang-format -i $(SRCS)

clean:
	rm server
