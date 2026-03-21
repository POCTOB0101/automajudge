export CC=gcc
export FLAGS=-Wall
export DEBUG=-g
FASTFLAGS=-O2 -DNDEBUG
SHIMFLAGS=-shared -fPIC
OUTPUT=-o libmyalloc.so
IMPORTANTFILES=allocator.c
# user can change as desired if they have headers

default:
	$(CC) $(FLAGS) $(DEBUG) $(SHIMFLAGS) $(IMPORTANTFILES) $(OUTPUT)
	chmod +x make_help.sh
	./make_help.sh

