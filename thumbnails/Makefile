EXEC = thumbnails
DEPS = -lepeg -larchive

# For pthreads
#DEPS += -lpthread
# For upthreads
DEPS += -lupthread -lparlib
INCS = -I/usr/include/upthread/compatibility

all:
	gcc -std=gnu99 $(INCS) $(EXEC).c -o $(EXEC) $(DEPS)

clean:
	rm -rf $(EXEC)
