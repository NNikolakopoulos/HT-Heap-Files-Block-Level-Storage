#include <stdio.h>
#include "hp.h"
#include "BF.h"
#include <string.h>
#include <stdlib.h>


int HP_CreateFile(char *fileName,char attrType,char* attrName,int attrLength)
{
    int fd, count, len, i, empty=-1, now_block,next_block;
    //basic code for initialization of file
    if(BF_CreateFile(fileName)<0)
    {
        BF_PrintError("Error creating BF");
        return -1;
    }

    fd = BF_OpenFile(fileName);
    if( fd < 0)
    {
        BF_PrintError("Error opening BF");
        return -1;
    }

    if(BF_AllocateBlock(fd) < 0)
    {
        BF_PrintError("Error allocating BF");
        return -1;
    }

    void* buf;
    if(BF_ReadBlock(fd,0,&buf))
    {
        BF_PrintError("Error reading BF");
        return -1;
    }

    count=0;
    memcpy(buf,&attrType,sizeof(char));
    count+= sizeof(char);

    len = strlen(attrName);
    memcpy(buf+count,&len,sizeof(int));
    count+= sizeof(int);
    memcpy(buf+count,attrName,len);
    count+= len;

    memcpy(buf+count,&attrLength,sizeof(int));
    count+= sizeof(int);

    now_block=0;
    empty=-1;
    memcpy(buf + BLOCK_SIZE - sizeof(int),&now_block,sizeof(int));//num of records
    memcpy(buf + BLOCK_SIZE - 2*sizeof(int),&empty,sizeof(int));
    if(BF_WriteBlock(fd,now_block))   // store the previous block .
    {
        BF_PrintError("Error writing BF");
        return -1;
    }

    if(BF_CloseFile(fd))   // store the previous block .
    {
        BF_PrintError("Error closing BF");
        return -1;
    }
    return 0;
    
}
int flag=0;

HP_info* HP_OpenFile(char *fname)
{
    printf("hello\n");
    if (flag==1)
    {
        printf("file is already open\n");
        return NULL;
    }
    flag=1;
    printf("flag:%d\n",flag);
    int fd,count=0,name_len=0,i,now_block,next_block;

    fd = BF_OpenFile(fname);
    if( fd < 0)
    {
        BF_PrintError("Error opening BF");
        return NULL;
    }

    void* buf;
    if(BF_ReadBlock(fd,0,&buf))
    {
        BF_PrintError("Error reading BF");
        return NULL;
    }

    HP_info* heap = malloc(sizeof(HP_info));
    heap->attrName=NULL;
    memcpy(&heap->attrType,buf,sizeof(char));
    count+=sizeof(char);

    memcpy(&name_len,buf+count,sizeof(int));
    count+=sizeof(int);

    heap->attrName=malloc((name_len+1)*sizeof(char));   //TODO remember to free name allocated memory
    memcpy(heap->attrName,buf+count,name_len);
    count+=name_len;
    memcpy(&heap->attrLength,buf+count,sizeof(int));
    count+=sizeof(int);

    heap->fileDesc=fd;   // store the file descriptor 
    //heap->offset = count; //we need to store the offset of the first info's about the heaptable, to skip them in the 1st block.
    return heap;
}

int HP_InsertEntry( HP_info header_info,Record record)
{
    int i,now_block,nowblock=0,count=0,numRecords,empty=-1,next_bloc,temp,id,blocknum,val,pass;
    void *buf,*t;
    Record *r;
    val=record.id;
    now_block=BF_GetBlockCounter(header_info.fileDesc) -1;
    if(BF_ReadBlock(header_info.fileDesc,now_block,&buf)<0)
    {
        BF_PrintError("Error reading BF");
        return -1;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    blocknum=BF_GetBlockCounter(header_info.fileDesc)-1;
    if(BF_ReadBlock(header_info.fileDesc,blocknum,&t) <0)
    {   
        BF_PrintError("Error reading BF in delete entry");
        return -1;
    }
    memcpy(&numRecords,t + BLOCK_SIZE - sizeof(int),sizeof(int));
    while(1)
    {
        memcpy(&numRecords, buf + BLOCK_SIZE - sizeof(int),sizeof(int));
        for(int count=0; count<numRecords;count++)
        {
            memcpy(&temp,buf + 94*count,sizeof(int));
            if (temp==val)
            {
                printf("same id found\n");
                return -1;//same id found
            }
        }
        memcpy(&pass,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
        if(pass==-1) break;
        if(BF_ReadBlock(header_info.fileDesc,++nowblock,&buf) <0)
        {   
            BF_PrintError("Error reading BF in delete entry");
            return -1;
        }
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    now_block=BF_GetBlockCounter(header_info.fileDesc) -1;
    if(BF_ReadBlock(header_info.fileDesc,now_block,&buf)<0)
    {
        BF_PrintError("Error reading BF");
        return -1;
    }
    memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int), sizeof(int));
    if(numRecords < 5) //an xoraei record sto block
    {
        count=94*numRecords;
        memcpy(buf+count,&record.id,sizeof(int));
        count+=sizeof(int);
        memcpy(buf+count,&record.name,15*sizeof(char));
        count+=15*sizeof(char);
        memcpy(buf+count,&record.surname,25*sizeof(char));
        count+=25*sizeof(char);
        memcpy(buf+count,&record.address,50*sizeof(char));
        count+=50*sizeof(char);
        temp=numRecords+1;
        memcpy(buf + BLOCK_SIZE - sizeof(int),&temp,sizeof(int));
        memcpy(&id,buf+94*numRecords,sizeof(int));
        if(BF_WriteBlock(header_info.fileDesc,now_block))   // store the previous block .
        {
            BF_PrintError("Error writing BF");
            return -1;
        }
        return BF_GetBlockCounter(header_info.fileDesc)-1;
    }
    else // an den xoraei record sto block
    {
        void *newbuf;
        temp=BF_GetBlockCounter(header_info.fileDesc)-1;
        memcpy(buf+ BLOCK_SIZE - 2*sizeof(int),&temp,sizeof(int));
        if(BF_AllocateBlock(header_info.fileDesc) < 0)
        {
            BF_PrintError("Error allocating BF");
            return -1;
        }
        now_block = BF_GetBlockCounter(header_info.fileDesc)-1;
        if(BF_ReadBlock(header_info.fileDesc,now_block,&newbuf)<0)
        {
            BF_PrintError("Error reading BF");
            return -1;
        }
        count=0;
        memcpy(newbuf,&record.id,sizeof(int));
        count+=sizeof(int);
        memcpy(newbuf+count,&record.name,15*sizeof(char));
        count+=15*sizeof(char);
        memcpy(newbuf+count,&record.surname,25*sizeof(char));
        count+=25*sizeof(char);
        memcpy(newbuf+count,&record.address,50*sizeof(char));
        count+=50*sizeof(char);
        temp=1;
        memcpy(newbuf + BLOCK_SIZE - sizeof(int),&temp,sizeof(int));
        temp=-1;
        memcpy(newbuf + BLOCK_SIZE - 2*sizeof(int),&temp, sizeof(int));
        memcpy(&id,newbuf,sizeof(int));
        if(BF_WriteBlock(header_info.fileDesc,now_block))   // store the previous block .
        {
            BF_PrintError("Error writing BF");
            return -1;
        }
        return BF_GetBlockCounter(header_info.fileDesc)-1;
    }
}

int HP_DeleteEntry( HP_info header_info,void *value)
{
    int nowblock=0,temp,blocknum,numRecords,val,pass;
    val = *(int*) value;
    void * buf,*t;
    Record *r;
    if(BF_ReadBlock(header_info.fileDesc,nowblock,&buf) <0)
    {   
        BF_PrintError("Error reading BF in delete entry");
        return -1;
    }
    blocknum=BF_GetBlockCounter(header_info.fileDesc)-1;
    if(BF_ReadBlock(header_info.fileDesc,blocknum,&t) <0)
    {   
        BF_PrintError("Error reading BF in delete entry");
        return -1;
    }
    memcpy(&numRecords,t + BLOCK_SIZE - sizeof(int),sizeof(int));
    memcpy(r,t + 94*(numRecords-1),94);
    if (r->id==val)
    {
        pass=numRecords-1;
        memcpy(t + BLOCK_SIZE - sizeof(int),&pass,sizeof(int));
        if(BF_WriteBlock(header_info.fileDesc,blocknum))    //store into the disk, because of the new number of records
        {
            BF_PrintError("Error writing BF");
            return -1;
        }
        return 0;
    }
    while(1)
    {
        memcpy(&numRecords, buf + BLOCK_SIZE - sizeof(int),sizeof(int));
        for(int count=0; count<numRecords;count++)
        {
            memcpy(&temp,buf + 94*count,sizeof(int));
            if (temp==val)
            {
                blocknum=BF_GetBlockCounter(header_info.fileDesc)-1;
                if(BF_ReadBlock(header_info.fileDesc,blocknum,&t) <0)
                {   
                    BF_PrintError("Error reading BF in delete entry");
                    return -1;
                }
                memcpy(&numRecords,t + BLOCK_SIZE - sizeof(int),sizeof(int));
                memcpy(r,t + 94*(numRecords-1),94);
                pass=numRecords-1;
                memcpy(t + BLOCK_SIZE - sizeof(int),&pass,sizeof(int));
                if(BF_WriteBlock(header_info.fileDesc,blocknum))    //store into the disk, because of the new number of records
                {
                    BF_PrintError("Error writing BF");
                    return -1;
                }
                if(BF_ReadBlock(header_info.fileDesc,nowblock,&t) <0)
                {   
                    BF_PrintError("Error reading BF in delete entry");
                    return -1;
                }
                memcpy(t + count*94,r,94);
                if(BF_WriteBlock(header_info.fileDesc,nowblock))    //store into the disk, because of the new number of records
                {
                    BF_PrintError("Error writing BF");
                    return -1;
                }
                return 0;
            }
        }
        memcpy(&pass,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
        if(pass==-1) break;
        if(BF_ReadBlock(header_info.fileDesc,++nowblock,&buf) <0)
        {   
            BF_PrintError("Error reading BF in delete entry");
            return -1;
        }
    }
    return -1;
}

Record* read_rec_from_file(Record *record,FILE *fd,char *buffer,size_t bufsiz)
{
    char *token;

    token = strtok(buffer+1,",");
    record->id=atoi(token);
    token = strtok(NULL, ",");
    strncpy(record->name,token+1,strlen(token)-2);
    token = strtok(NULL, ",");
    strncpy(record->surname,token+1,strlen(token)-2);
    token = strtok(NULL, "}");
    strncpy(record->address,token+1,strlen(token)-2);


    return record;
}

int HP_InsertAll(HP_info info,char* ftest)
{
    FILE *fp = fopen(ftest,"r"); //open the file we want
    char*buffer=NULL;
    size_t bufsiz=110;
    int retval=0;
    int i=0;
    Record* record=NULL;
    
    buffer = malloc(bufsiz*sizeof(char));

    if(fp == NULL)
        return -1;
    while(getline(&buffer,&bufsiz,fp)>=0)
    {

        record = (Record*) malloc(sizeof(Record));
        record=read_rec_from_file(record,fp,buffer,bufsiz);
        //printRecord(record);
        if(HP_InsertEntry(info,*record) < 0)
        {
            printf("error inserting entry\n");
            return -1;
        }
        free(record);   
    }
    free(buffer);
    fclose(fp);
    return 0;
}

void printRecord(Record *record)
{
    printf(" %d %s %s %s\n", record->id,record->name,record->surname,record->address);
}

int HP_Print(HP_info info)
{
    int now_block,next_block,count,numRecords,temp,pass,i=0;
    void *buf;
    Record* record=malloc(sizeof(Record));
    now_block=0;
    if(BF_ReadBlock(info.fileDesc,now_block,&buf))  
    {
        BF_PrintError("Error reading BF");
        return -1;
    }
    
    while (1)
    {
        memcpy(&numRecords, buf + BLOCK_SIZE - sizeof(int),sizeof(int));
        for(int temp=0; temp<numRecords;temp++)
        {             
            count=94*temp;
            
            memcpy(&record->id,buf+count,sizeof(int));
            count+=sizeof(int);
            memcpy(&record->name,buf+count,15*sizeof(char));
            count+=15*sizeof(char);
            memcpy(&record->surname,buf+count,25*sizeof(char));
            count+=25*sizeof(char);
            memcpy(&record->address,buf+count,50*sizeof(char));
            count+=50*sizeof(char);
            printf("%d %s %s %s\n",record->id,record->name,record->surname,record->address);
        }
        memcpy(&pass,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
        if(pass==-1) break;
        now_block++;
        if(BF_ReadBlock(info.fileDesc,now_block,&buf))  
        {
            BF_PrintError("Error reading BF");
            return -1;
        }
    }
    free(record);
}

int HP_GetAllEntries(HP_info header_info,void *value)
{
    int nowblock=0,temp,blocknum,numRecords,val,pass,i=1;
    if (value==NULL)
    {
        HP_Print(header_info);
        return BF_GetBlockCounter(header_info.fileDesc) -1;
    }
    val = *(int*) value;
    //printf("hello %d\n",val);
    void * buf,*t;
    Record *record=malloc(sizeof(Record));
    if(BF_ReadBlock(header_info.fileDesc,nowblock,&buf) <0)
    {   
        BF_PrintError("Error reading BF in delete entry");
        return -1;
    }
    while(1)
    {
        memcpy(&numRecords, buf + BLOCK_SIZE - sizeof(int),sizeof(int));
        for(int count=0; count<numRecords;count++)
        {
            memcpy(&temp,buf + 94*count,sizeof(int));
            if (temp==val)
            {
                count=94*count;
                memcpy(&record->id,buf+count,sizeof(int)); printf("record id : %d\n",record->id);
                count+=sizeof(int);
                memcpy(&record->name,buf+count,15*sizeof(char));
                count+=15*sizeof(char);
                memcpy(&record->surname,buf+count,25*sizeof(char));
                count+=25*sizeof(char);
                memcpy(&record->address,buf+count,50*sizeof(char));
                count+=50*sizeof(char);
                printf("%d %s %s %s\n",record->id,record->name,record->surname,record->address);
                free(record);
                return i;
            }
        }
        i++;
        memcpy(&pass,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
        if(pass==-1) break;
        if(BF_ReadBlock(header_info.fileDesc,++nowblock,&buf) <0)
        {   
            BF_PrintError("Error reading BF in delete entry");
            return -1;
        }
    }
    free(record);
    printf("no such record found\n");
    return -1;
}

int HP_CloseFile( HP_info* header_info )
{
    free(header_info->attrName);
    flag=0;
    if(BF_CloseFile(header_info->fileDesc) < 0)
    {
        BF_PrintError("Error closing BF");
        return -1;
    }
    free(header_info);
    return 0;
}
