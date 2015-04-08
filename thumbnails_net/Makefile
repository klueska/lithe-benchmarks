EXEC = thumbnails
DEPS = -lm -lcurl -pthread native-timing.c

all:
	gcc -std=gnu99 -g $(INCS) $(EXEC).c -o $(EXEC) $(DEPS)

clean:
	rm -rf $(EXEC)
