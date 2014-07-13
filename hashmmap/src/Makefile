all: libhashmmap.so

libhashmmap.so: hashmmap.c ../include/hashmmap.h include/hashmmap_priv.h
	gcc -shared -fPIC -g -I../include -Iinclude hashmmap.c -o libhashmmap.so
