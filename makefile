all: mainHP mainHT

mainHT:mainHT.o HT.o SHT.o
	gcc -g3 -o  mainHT mainHT.o HT.o SHT.o BF_64.a -no-pie

mainHP:mainHP.o hp.o 
	gcc -o mainHP mainHP.o hp.o BF_64.a -no-pie


hp.o:hp.c
	gcc -c hp.c

SHT.o:SHT.c
	gcc -g3 -c SHT.c

HT.o:HT.c
	gcc -g3 -c HT.c


clean:
	rm -f mainHT.o mainHP.o hp.o HT.o mainHT mainHP SHT.o sfname fname