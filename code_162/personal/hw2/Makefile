CC=gcc
CFLAGS=-ggdb3 -c -Wall -std=gnu99
LDFLAGS=-pthread
SOURCES=httpserver.c libhttp.c wq.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=httpserver

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)
