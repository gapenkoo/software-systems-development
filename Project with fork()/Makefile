# Exec file name
TARGET = lab1

# cc invoking
CC = gcc

# object file names
OBJECTS = src/main.o

SRCS = src/

# project dependence
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS)

clean:
	rm -rf ${TARGET} ${SRCS}*.o
