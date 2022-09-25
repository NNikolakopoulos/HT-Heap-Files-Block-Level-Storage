#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HT.h"
#include "BF.h"
#include "SHT.h"

int main(void)
{

    HTinfo* info,*test;
    SHTinfo* Sinfo;
    
    BF_Init();

    HT_CreateIndex("fname",'i',"key",5,50);
    SHT_CreateSecondaryIndex("sfname","key",5,50,"fname");

    info=HT_OpenIndex("fname");
    Sinfo=SHT_OpenIndex("sfname");
    
    HT_InsertAll(*info,"records1K.txt");
    SHT_InsertAll(*Sinfo,*info);

    //SHT_Print(*Sinfo);
    int k=597;
    HT_GetAllEntries(*info, &k);
    SHT_SecondaryGetAllEntries(*Sinfo, *info, "surname_597");
    
    HT_CloseIndex(info);
    SHT_CloseIndex(Sinfo);

    //SHT_HashStatistics("sfname");

    return 0;
}