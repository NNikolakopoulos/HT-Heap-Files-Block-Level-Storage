#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "BF.h"
#include "HT.h"

bool FLAG_OPEN = false;


int hashFun(int n, int i)
{
    return i%n;
}

int HT_CreateIndex(char* fname,char attrType,char* attrName,int attrLength,int buckets)
{
    int fd, count, len, i, empty=-1, now_block, numBuckets,next_block;
    //basic code for initialization of file
    if(BF_CreateFile(fname)<0)
    {
        BF_PrintError("Error creating BF");
        return -1;
    }

    fd = BF_OpenFile(fname);
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

    void* buffer;
    if(BF_ReadBlock(fd,0,&buffer))
    {
        BF_PrintError("Error reading BF");
        return -1;
    }

    count=0;
    memcpy(buffer,&attrType,sizeof(char));
    count+= sizeof(char);

    len = strlen(attrName);
    memcpy(buffer+count,&len,sizeof(int));
    count+= sizeof(int);
    memcpy(buffer+count,attrName,len);
    count+= len;

    memcpy(buffer+count,&attrLength,sizeof(int));
    count+= sizeof(int);

    memcpy(buffer+count,&buckets,sizeof(int));
    count+= sizeof(int);

    now_block=0;
    for(i=0; i<buckets; i++)
    {
        memcpy(buffer+count,&empty,sizeof(int));  // store -1 as empty(initialize)
        count+=sizeof(int);   

        
        if( (unsigned)count > BLOCK_SIZE - 3*sizeof(int))   //if new entries cannot fit in this block, then create a new one
        {

            if(BF_AllocateBlock(fd) < 0)
            {
                BF_PrintError("Error allocating BF");
                return -1;
            }

            next_block = BF_GetBlockCounter(fd)-1; // get the number of the new block(its the last created, -1 because we start counting from 0)
            memcpy(buffer+BLOCK_SIZE-2*sizeof(int), &next_block, sizeof(int)); //store the pointer to the new block , inside the previous block


            if(now_block==0)   // the 1st block has some extra info about the HT , so it has less available buckets
                numBuckets = i+1; 
            else               // else we can store up to count/sizeof(int) buckets to the block
                numBuckets = (count/sizeof(int)); 

            memcpy(buffer+BLOCK_SIZE-sizeof(int) , &numBuckets, sizeof(int));

            if(BF_WriteBlock(fd,now_block))   // store the previous block .
            {
                BF_PrintError("Error writing BF");
                return -1;
            }

            void *newbuf;
            if(BF_ReadBlock(fd,next_block,&newbuf))
            {
                BF_PrintError("Error reading BF");
                return -1;
            }

            now_block=next_block; 
            buffer=newbuf;
            count=0; //start counting from the beginning of the block
        }
    }

    if(now_block==0)   
        numBuckets = buckets; 
    else               
        numBuckets = (count/sizeof(int)); 
    
    memcpy(buffer+BLOCK_SIZE-sizeof(int),&numBuckets,sizeof(int));
    memcpy(buffer+BLOCK_SIZE-2*sizeof(int), &empty, sizeof(int));

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

HTinfo* HT_OpenIndex(char *fname)
{
    int fd,count,name_len=0,i,now_block,next_block;

    if(FLAG_OPEN) {
        printf("Error:File is already open\n");
        return NULL;
    }
    FLAG_OPEN=true;

    fd = BF_OpenFile(fname);
    if( fd < 0)
    {
        BF_PrintError("Error opening BF");
        return NULL;
    }

    void*buf;
    if(BF_ReadBlock(fd,0,&buf))
    {
        BF_PrintError("Error reading BF");
        return NULL;
    }

    HTinfo* hash = malloc(sizeof(HTinfo));
    count=0;

    memcpy(&hash->attrType,buf,sizeof(char));
    count+=sizeof(char);

    memcpy(&name_len,buf+count,sizeof(int));
    count+=sizeof(int);

    hash->attrName=malloc((name_len+1)*sizeof(char));   //TODO remember to free name allocated memory
    memcpy(hash->attrName,buf+count,name_len);
    count+=name_len;

    memcpy(&hash->attrLength,buf+count,sizeof(int));
    count+=sizeof(int);

    memcpy(&hash->numBuckets,buf+count,sizeof(int));
    count+=sizeof(int);

    hash->array=malloc(hash->numBuckets*sizeof(int));   // allocate memory for the hashtable
    hash->fileDesc=fd;   // store the file descriptor 
    hash->offset = count; //we need to store the offset of the first info's about the hashtable, to skip them in the 1st block.
    for(i=0; i<hash->numBuckets; i++)
    {
        memcpy(&(hash->array[i]),buf+count,sizeof(int));
        count+=sizeof(int);
        if(count > (BLOCK_SIZE - 3*sizeof(int)))  //if there is nothing to read in this block, then read from the next block
        {
            memcpy(&next_block, buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));  //get the next block

            void* newbuf;
            if(BF_ReadBlock(fd,next_block,&newbuf))  
            {
                BF_PrintError("Error reading BF");
                return NULL;
            }

            buf=newbuf;
            count=0;
        }
    }
    return hash;
}

int HT_CloseIndex(HTinfo* info)  
{

    int i,count,fd=info->fileDesc,now_block,next_block;
    void *buf;
    
    now_block=0;
    if(BF_ReadBlock(info->fileDesc,now_block,&buf)<0)
    {
         BF_PrintError("Error reading BF");
        return -1;
    }
    count=info->offset;
    for(i=0; i<info->numBuckets; i++)
    {
        
        if( count > BLOCK_SIZE - 3*sizeof(int))
        {
            if(BF_WriteBlock(fd,now_block))   // store the previous block .
            {
                BF_PrintError("Error writing BF");
                return -1;
            }

            memcpy(&next_block, buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));  //get the next block

            void* newbuf;
            if(BF_ReadBlock(fd,next_block,&newbuf))  
            {
                BF_PrintError("Error reading BF");
                return -1;
            }

            buf=newbuf;
            count=0;
            now_block=next_block;
        }    
        
        if(info->array[i]!=-1)
        {
            memcpy(buf+count,&info->array[i],sizeof(int));
        }
        count+=sizeof(int);

    }
        


    free(info->array);
    free(info->attrName);

    if(BF_CloseFile(info->fileDesc) < 0)
    {
        BF_PrintError("Error closing BF");
        return -1;
    }
    
    free(info);

    FLAG_OPEN=false; //initialize the flag , so we can open again a file
    return 0;
}

int HT_InsertEntry(HTinfo info,Record record)  
{
    int i,now_block,count,numRecords,empty=-1,next_block,tempid;
    i=hashFun(info.numBuckets,record.id);
    now_block=info.array[i];
    if(now_block == -1) //if there is no block in this bucket
    {
        if(BF_AllocateBlock(info.fileDesc) < 0)
        {
            BF_PrintError("Error allocating BF");
            return -1;
        }
        now_block = BF_GetBlockCounter(info.fileDesc)-1;
        info.array[i] = now_block;

        void *buf;
        if(BF_ReadBlock(info.fileDesc,now_block,&buf)<0)
        {
            BF_PrintError("Error reading BF");
            return -1;
        }
        count = 0;

        memcpy(buf,&record.id,sizeof(int));
        count+=sizeof(int);
        memcpy(buf+count,&record.name,15*sizeof(char));
        count+=15*sizeof(char);
        memcpy(buf+count,&record.surname,25*sizeof(char));
        count+=25*sizeof(char);
        memcpy(buf+count,&record.address,50*sizeof(char));
        count+=50*sizeof(char);

        numRecords = 1;   //now the new block it has 1 record only.
        memcpy(buf + BLOCK_SIZE - sizeof(int),&numRecords,sizeof(int)); 
        memcpy(buf + BLOCK_SIZE - 2*sizeof(int),&empty,sizeof(int));  // set pointer to next block = -1 , it means its NULL
        if(BF_WriteBlock(info.fileDesc,now_block))   // store the previous block .
        {
            BF_PrintError("Error writing BF");
            return -1;
        }
    }
    else
    {
        void *buf;
        if(BF_ReadBlock(info.fileDesc,now_block,&buf))
        {
            BF_PrintError("Error reading BF");
            return -1;
        }
        count=0;
        memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
        memcpy(&next_block,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));

        while(numRecords > 0 )
        {
            memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
            memcpy(&tempid,buf+count,sizeof(int));
            if(tempid == record.id)
            {
                printf("Error:This record is already inserted\n");
                return 0;
            }
            else
            {
                count+=94;
                if((count >  BLOCK_SIZE - 3*sizeof(int)) && next_block != -1)  //if we did not find the record in this block , go to next block
                {
                    now_block=next_block;
                    void* newbuf;
                    if(BF_ReadBlock(info.fileDesc,next_block,&newbuf))  
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    buf=newbuf;
                    memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
                    count=0;
                }
                else if((count >= (numRecords*sizeof(Record) -2))&& next_block == -1)   //if its the last block and we didnt find the record stop
                    break;
            }
        }
        memcpy(&numRecords,buf+BLOCK_SIZE-sizeof(int),sizeof(int));
        memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
        if(numRecords == 5 && next_block == -1) //if all the blocks are full in this bucket, then create a block
        {
            if(BF_AllocateBlock(info.fileDesc) < 0)
            {
                BF_PrintError("Error allocating BF");
                return -1;
            }

            next_block = BF_GetBlockCounter(info.fileDesc)-1; // get the number of the new block(its the last created, -1 because we start counting from 0)
            memcpy(buf+BLOCK_SIZE-2*sizeof(int), &next_block, sizeof(int)); //store the pointer to the new block , inside the previous block

            if(BF_WriteBlock(info.fileDesc,now_block))   // store the previous block .
            {
                BF_PrintError("Error writing BF");
                return -1;
            }

            void *newbuf;
            if(BF_ReadBlock(info.fileDesc,next_block,&newbuf))
            {
                BF_PrintError("Error reading BF");
                return -1;
            }
            numRecords=0;
            memcpy(newbuf+BLOCK_SIZE-sizeof(int), &numRecords, sizeof(int));    // the new block has 0 records
            memcpy(newbuf+BLOCK_SIZE-2*sizeof(int), &empty, sizeof(int));       // set pointer to NULL
            buf=newbuf;
            now_block=next_block;
        }

        count=94*numRecords;

        memcpy(buf+count,&record.id,sizeof(int));
        count+=sizeof(int);
        memcpy(buf+count,&record.name,15*sizeof(char));
        count+=15*sizeof(char);
        memcpy(buf+count,&record.surname,25*sizeof(char));
        count+=25*sizeof(char);
        memcpy(buf+count,&record.address,50*sizeof(char));
        count+=50*sizeof(char);

        numRecords++;
        memcpy(buf + BLOCK_SIZE - sizeof(int),&numRecords,sizeof(int)); 

        if(BF_WriteBlock(info.fileDesc,now_block))   // store the next block .
        {
            BF_PrintError("Error writing BF");
            return -1;
        }
    }
    return 0;
}


Record* read_rec_from_file(Record *record,FILE *fd,char *buffer,size_t bufsiz)
{
    char *token;
    //buffer = malloc(bufsiz*sizeof(char));

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


int HT_InsertAll(HTinfo info,char* ftest)
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
        if(HT_InsertEntry(info,*record) < 0)
        {
            printf("error inserting entry\n");
            return -1;
        }   
        free(record);
    }
    free(buffer);
    fclose(fp);
    return 0;;
}

int HT_DeleteEntry(HTinfo info,void* value)
{
    int now_block,i,val,next_block,numRecords,count,tempid=0,recid,last_block;
    Record* record = malloc(sizeof(Record));

    val = *(int*) value;
    i=hashFun(info.numBuckets,val);
    now_block=info.array[i];

    void* buf;
    if(BF_ReadBlock(info.fileDesc,now_block,&buf) <0)
    {   
        BF_PrintError("Error reading BF in delete entry");
        return -1;
    }

    memcpy(&numRecords,buf+BLOCK_SIZE-sizeof(int),sizeof(int));
    memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
    if(next_block==-1 && numRecords == 0) {
        printf("Error: this record does not exists\n");
        return -1;
    }
    
    if(next_block == -1 && numRecords == 1)  //if there is only 1 record in this bucket
    {
        memcpy(&recid,buf,sizeof(int)); 
        if (recid == val) {
            numRecords--;
            memcpy(buf + BLOCK_SIZE - sizeof(int),&numRecords,sizeof(int)); 
            if(BF_WriteBlock(info.fileDesc,now_block))    //store into the disk, because of the new number of records
            {
                BF_PrintError("Error writing BF");
                return -1;
            }
        }
        else
            printf("Error: this record does not exist\n");

        
    }
    else
    {
        // go to last block
        while(next_block!=-1) 
        {
            now_block=next_block;
            last_block=now_block;
            void* newbuf;
            if(BF_ReadBlock(info.fileDesc,next_block,&newbuf))  
            {
                BF_PrintError("Error reading BF");
                return -1;
            }
            buf=newbuf;
            memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
        }
        memcpy(&numRecords,buf+BLOCK_SIZE-sizeof(int),sizeof(int));
        count=94*(numRecords-1);

        // save the last record

        memcpy(&record->id,buf+count,sizeof(int));
        count+=sizeof(int);
        memcpy(&record->name,buf+count,15*sizeof(char));
        count+=15*sizeof(char);
        memcpy(&record->surname,buf+count,25*sizeof(char));
        count+=25*sizeof(char);
        memcpy(&record->address,buf+count,50*sizeof(char));
        count+=50*sizeof(char);

        numRecords--;  //dont forget to decrease the number of records by 1
        memcpy(buf + BLOCK_SIZE - sizeof(int),&numRecords,sizeof(int)); 
        if(BF_WriteBlock(info.fileDesc,now_block))    //store into the disk, because of the new number of records
        {
            BF_PrintError("Error writing BF");
            return -1;
        }


        //now re-load again the 1st block of this bucket
        //printf("%d %s\n",record->id,record->name);
        now_block=info.array[i];  
        if(BF_ReadBlock(info.fileDesc,now_block,&buf) <0)
        {   
            BF_PrintError("Error reading BF in delete entry");
            return -1;
        }
        count=0;
        memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
        memcpy(&next_block,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
        while(1)
        {
            memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
            memcpy(&tempid,buf+count,sizeof(int));
            //printf("%d   %d   %d\n",tempid,val,numRecords);
            if(tempid == val)
            {
                memcpy(buf+count,&record->id,sizeof(int));
                count+=sizeof(int);
                memcpy(buf+count,&record->name,15*sizeof(char));
                count+=15*sizeof(char);
                memcpy(buf+count,&record->surname,25*sizeof(char));
                count+=25*sizeof(char);
                memcpy(buf+count,&record->address,50*sizeof(char));
                count+=50*sizeof(char);

                if(BF_WriteBlock(info.fileDesc,now_block))   
                {
                    BF_PrintError("Error writing BF");
                    return -1;
                }
                return 0;
            }
            else
            {
                count+=94;
                if((count >  BLOCK_SIZE - 3*sizeof(int)) && next_block != -1)  //if we did not find the record in this block , go to next block
                {
                    now_block=next_block;
                    void* newbuf;
                    if(BF_ReadBlock(info.fileDesc,next_block,&newbuf))  
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    buf=newbuf;
                    memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
                    count=0;
                }
                else if((count >= numRecords*sizeof(Record))&& next_block == -1)   //if its the last block and we didnt find the record, then return -1
                {
                     //we increase by 1 the number of records of the last block, 
                     // because we decreased it before and we didn't find the 
                     // record we were looking for
                    if(BF_ReadBlock(info.fileDesc,last_block,&buf))    
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    memcpy(&numRecords,buf+BLOCK_SIZE-sizeof(int),sizeof(int));
                    //printf("records=%d\n",numRecords);
                    numRecords++;
                    //printf("records=%d\n",numRecords);
                    memcpy(buf+BLOCK_SIZE-sizeof(int),&numRecords,sizeof(int));
                    count=0;

                    printf("Error: this record does not exist\n");
                    return -1;
                }
            } 
        }
    }
    
    
}

int HT_Print(HTinfo info)
{
    int now_block,next_block,i,count,numRecords;
    void *buf;
    Record* record=malloc(sizeof(Record));
    for(int i=0; i<info.numBuckets; i++)
    {
        now_block = info.array[i];
        if(now_block!=-1)
        {
            if(BF_ReadBlock(info.fileDesc,now_block,&buf))  
            {
                BF_PrintError("Error reading BF");
                return -1;
            }
            count = 0;
            memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
            memcpy(&next_block,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
            while(numRecords!=0)
            {
                
                memcpy(&record->id,buf+count,sizeof(int));
                count+=sizeof(int);
                memcpy(&record->name,buf+count,15*sizeof(char));
                count+=15*sizeof(char);
                memcpy(&record->surname,buf+count,25*sizeof(char));
                count+=25*sizeof(char);
                memcpy(&record->address,buf+count,50*sizeof(char));
                count+=50*sizeof(char);

                printf("%d %s %s %s\n",record->id,record->name,record->surname,record->address);

                if(count >=  94*numRecords  && next_block != -1)  //go to next block
                {
                    now_block=next_block;
                    void* newbuf;
                    if(BF_ReadBlock(info.fileDesc,next_block,&newbuf))  
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    buf=newbuf;
                    memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
                    count=0;
                    memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
                }
                else if((count >=  94*numRecords)&& next_block == -1)   //if its the last block stop
                    break;
                
            }
            printf("\n");
        }
        
    }
    free(record);
}

int HT_GetAllEntries(HTinfo info,void* value)
{
    int now_block,next_block,i,count,numRecords,val,numBlocks=0;
    void *buf;
    Record* record=malloc(sizeof(Record));
    if( value != NULL)
    {
        val = *(int*) value;


        i = hashFun(info.numBuckets,val);
        now_block = info.array[i];
        if(now_block!=-1)
        {
            if(BF_ReadBlock(info.fileDesc,now_block,&buf))  
            {
                BF_PrintError("Error reading BF");
                return -1;
            }
            count = 0;
            memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
            memcpy(&next_block,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
            numBlocks++;
            while(numRecords!=0)
            {
                
                memcpy(&record->id,buf+count,sizeof(int));
                if(val == record->id)
                {
                    count+=sizeof(int);
                    memcpy(&record->name,buf+count,15*sizeof(char));
                    count+=15*sizeof(char);
                    memcpy(&record->surname,buf+count,25*sizeof(char));
                    count+=25*sizeof(char);
                    memcpy(&record->address,buf+count,50*sizeof(char));
                    count+=50*sizeof(char);

                    printf("%d %s %s %s\n",record->id,record->name,record->surname,record->address);
                    free(record);
                    return numBlocks;
                }
                else
                    count+=94;

                if(count >=  94*numRecords  && next_block != -1)  //go to next block
                {
                    numBlocks++;
                    now_block=next_block;
                    void* newbuf;
                    if(BF_ReadBlock(info.fileDesc,next_block,&newbuf))  
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    buf=newbuf;
                    memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
                    count=0;
                    memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
                }
                else if((count >=  94*numRecords)&& next_block == -1)   //if its the last block stop
                    break;
                
            }
        }
        printf("\n");
        return -1;
    }
    else
    {
        for(int i=0; i<info.numBuckets; i++)
        {
            now_block = info.array[i];
            if(now_block!=-1)
            {
                if(BF_ReadBlock(info.fileDesc,now_block,&buf))  
                {
                    BF_PrintError("Error reading BF");
                    return -1;
                }
                count = 0;
                memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
                memcpy(&next_block,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
                numBlocks++;
                while(numRecords!=0)
                {
                    
                    memcpy(&record->id,buf+count,sizeof(int));
                    count+=sizeof(int);
                    memcpy(&record->name,buf+count,15*sizeof(char));
                    count+=15*sizeof(char);
                    memcpy(&record->surname,buf+count,25*sizeof(char));
                    count+=25*sizeof(char);
                    memcpy(&record->address,buf+count,50*sizeof(char));
                    count+=50*sizeof(char);

                    printf("%d %s %s %s\n",record->id,record->name,record->surname,record->address);

                    if(count >=  94*numRecords  && next_block != -1)  //go to next block
                    {
                        numBlocks++;
                        now_block=next_block;
                        void* newbuf;
                        if(BF_ReadBlock(info.fileDesc,next_block,&newbuf))  
                        {
                            BF_PrintError("Error reading BF");
                            return -1;
                        }
                        buf=newbuf;
                        memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
                        count=0;
                        memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
                    }
                    else if((count >=  94*numRecords)&& next_block == -1)   //if its the last block stop
                        break;
                    
                }
                printf("\n");
            }
            
        }
        if(numBlocks == 0) // if there are no blocks
            return -1;
        free(record);
        return numBlocks;
    }
}

int HashStatistics(char *fname)
{

    int now_block,next_block,count,numRecords , minRec, maxRec, sum, blockCounter, blocksPerBucketCounter=0, nonEmptyBuckets=0, overflowBuckets=0, overflowBlocks=0;
    float avgRec;
    void *buf,*newbuf;
    HTinfo *info;

    info=HT_OpenIndex("fname");

    for(int i=0; i<info->numBuckets; i++)
    {
        now_block = info->array[i];
        if(now_block!=-1)
        {
            nonEmptyBuckets++;
            if(BF_ReadBlock(info->fileDesc,now_block,&buf))  
            {
                BF_PrintError("Error reading BF");
                return -1;
            }
            
            memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
            memcpy(&next_block,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));

            overflowBlocks=0;
            count = 0;
            minRec = 1000000;
            maxRec = -1000000;
            avgRec = 0.0;
            sum = 0;
            blockCounter = 1;
            blocksPerBucketCounter++;

            while(1)
            {
                if(numRecords > maxRec)
                    maxRec=numRecords;
                if(numRecords < minRec)
                    minRec=numRecords;
                sum+=numRecords;
                count+=94*numRecords;

                if(count >=  94*numRecords  && next_block != -1)  //go to next block
                {
                    overflowBlocks++;
                    overflowBuckets++;
                    blockCounter++;
                    blocksPerBucketCounter++;
                    now_block=next_block;
                    void* newbuf;
                    if(BF_ReadBlock(info->fileDesc,next_block,&newbuf))  
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    buf=newbuf;
                    memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
                    count=0;
                    memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
                }
                else if((count >=  94*numRecords)&& next_block == -1)   //if its the last block stop
                    break;
                
            }
            avgRec = (float)sum / (float)blockCounter;
            printf("In bucket %d  |  minimun records are: %d    |  maximun records are: %d    |  average records are: %.3f      |    overflow blocks are: %d\n",i,minRec,maxRec,avgRec,overflowBlocks);

        }
        
    }

    

    printf("The number of blocks are: %d     |      number of overflow buckets are: %d\n",BF_GetBlockCounter(info->fileDesc),overflowBuckets);
    printf("Average blocks per bucket: %.3f\n",(float)blocksPerBucketCounter/(float)nonEmptyBuckets);


    HT_CloseIndex(info);
    return 0;
}


