#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BF.h"
#include "hp.h"
int main(void)
{
    Record* rec = (Record*) malloc(sizeof(Record));

    int i;
    i=9997;
    HP_info *info,*test;
    BF_Init();

    rec->id=9997;
    strcpy(rec->name,"nam");
    strcpy(rec->surname,"surname");
    strcpy(rec->address,"addre"); 

    if (HP_CreateFile("DB",'i',"id",sizeof(int))<0)
    {
        BF_PrintError("Error in HP Create.");
        return -1;
    }
    test = HP_OpenFile("DB");
    info = HP_OpenFile("DB");
    HP_InsertAll(*test,"records10K.txt");
    HP_DeleteEntry(*test,&i);
    //HP_DeleteEntry(*test,&i);
    HP_InsertEntry(*test,*rec);
    //HP_InsertEntry(*test,*rec);
    HP_GetAllEntries(*test,NULL);
    HP_GetAllEntries(*test,&i);
    HP_CloseFile(test);
}