#include "HTtypes.h"

int SHT_CreateSecondaryIndex(char *sfname,char* attrName,int attrLength,int buckets, char *fname);
SHTinfo* SHT_OpenIndex(char *sfname);
int SHT_CloseIndex(SHTinfo* info);
int SHT_InsertEntry(SHTinfo info,SRecord record);
int hashFunString(int size,char *K);
int SHT_InsertAll(SHTinfo sinfo,HTinfo info);
int SHT_HashStatistics(char *fname);
int SHT_SecondaryGetAllEntries(SHTinfo sinfo, HTinfo info, void *value);
int SHT_Print(SHTinfo info);