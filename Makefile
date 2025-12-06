CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra -std=c11
CXXFLAGS=-Wall -Wextra -std=c++17

C_OBJS=lexer.o lang_functions.o

all: lexer_test.exe dfa_visualizer.exe

lexer_test.exe: main.o $(C_OBJS)
	$(CC) $(CFLAGS) -o $@ main.o $(C_OBJS) -lm

dfa_visualizer.exe: dfa_visualizer.o $(C_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ dfa_visualizer.o $(C_OBJS) -lgdiplus -lm

main.o: main.c lexer.h lang.h
	$(CC) $(CFLAGS) -c main.c

lexer.o: lexer.c lexer.h lang.h
	$(CC) $(CFLAGS) -c lexer.c

lang_functions.o: lang_functions.c lang.h
	$(CC) $(CFLAGS) -c lang_functions.c

dfa_visualizer.o: dfa_visualizer.cpp lexer.h lang.h
	$(CXX) $(CXXFLAGS) -c dfa_visualizer.cpp

clean:
	del lexer_test.exe dfa_visualizer.exe main.o lexer.o lang_functions.o dfa_visualizer.o