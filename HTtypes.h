
#ifndef _HASH_H_
#define _HASH_H_


typedef struct 
{
    int id;
    char name[15];
    char surname[25];
    char address[50];

}Record;

typedef struct 
{
    char surname[25];
    int blockid;

}SRecord;



typedef struct 
{
    int fileDesc;
    char attrType;
    char* attrName;
    int attrLength;
    int numBuckets;
    int *array;
    int offset;

}HTinfo;


typedef struct 
{
    int fileDesc;
    char attrType;
    char* attrName;
    int attrLength;
    int numBuckets;
    int *array;
    int offset;
    char *filename;

}SHTinfo;


#endif