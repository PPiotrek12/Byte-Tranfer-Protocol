CC=g++
CFLAGS=-c -Wall -Wextra -O3 -std=c++17

all: ppcbs ppcbc

ppcbs: ppcbs.o common.o err.o 
	$(CC) $(LFLAGS) -o $@ $^

ppcbc: ppcbc.o common.o err.o
	$(CC) $(LFLAGS) -o $@ $^

ppcbs.o: ppcbs.cpp common.h err.h
	$(CC) $(CFLAGS) -c -o $@ $<

ppcbc.o: ppcbc.cpp common.h err.h
	$(CC) $(CFLAGS) -c -o $@ $<

common.o: common.cpp common.h err.h
	$(CC) $(CFLAGS) -c -o $@ $<

err.o: err.cpp err.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o ppcbc ppcbs
