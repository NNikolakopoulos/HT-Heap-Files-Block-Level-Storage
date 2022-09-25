#include "HTtypes.h"

int HT_CreateIndex(char* fname,char attrType,char* attrName,int attrLength,int buckets);
HTinfo* HT_OpenIndex(char *fname);
int HT_CloseIndex(HTinfo* info);
int HT_InsertEntry(HTinfo info,Record record);
int hashFun(int n,int i);
int HT_InsertAll(HTinfo info,char* ftest);
int HT_DeleteEntry(HTinfo info,void* value);
int HT_Print(HTinfo info);
int HashStatistics(char *fname);
int HT_GetAllEntries(HTinfo info,void* value);