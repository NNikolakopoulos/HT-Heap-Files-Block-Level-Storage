#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "BF.h"
#include "HT.h"
#include "SHT.h"

bool S_FLAG_OPEN = false;

int hashFunStrings(int size,char *K)
{
    int h=0, a=33;
    for (; *K!='\0'; K++)
        h=(a*h + *K) % size;
    return h;
}


int SHT_CreateSecondaryIndex(char* sfname,char* attrName,int attrLength,int buckets,char *fname)
{
    int fd, count, len, i, empty=-1, now_block, numBuckets,next_block;
    //basic code for initialization of file
    if(BF_CreateFile(sfname)<0)
    {
        BF_PrintError("Error creating BF");
        return -1;
    }

    fd = BF_OpenFile(sfname);
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

    len = strlen(attrName);
    memcpy(buffer+count,&len,sizeof(int));  // save the string's length of attrName 
    count+= sizeof(int);
    memcpy(buffer+count,attrName,len);
    count+= len;

    memcpy(buffer+count,&attrLength,sizeof(int));   //save the actual length of attrName
    count+= sizeof(int);

    memcpy(buffer+count,&buckets,sizeof(int));
    count+= sizeof(int);

    len = strlen(fname);
    memcpy(buffer+count,&len,sizeof(int));  // save the string's length of fname 
    count+= sizeof(int);
    memcpy(buffer+count,fname,len);  //save the primary hashtable file name
    count+= strlen(fname);
    

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


            if(now_block==0)   // the 1st block has some extra info about the SHT , so it has less available buckets
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

    if(BF_CloseFile(fd))  
    {
        BF_PrintError("Error closing BF");
        return -1;
    }
    return 0;
    
}

SHTinfo* SHT_OpenIndex(char *sfname)
{
    int fd,count,name_len=0,i,now_block,next_block;

    if(S_FLAG_OPEN) {
        printf("Error:File is already open\n");
        return NULL;
    }
    S_FLAG_OPEN=true;

    fd = BF_OpenFile(sfname);
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

    SHTinfo* hash = malloc(sizeof(SHTinfo));
    count=0;


    memcpy(&name_len,buf+count,sizeof(int));
    count+=sizeof(int);
    hash->attrName=malloc((name_len+1)*sizeof(char));   
    memcpy(hash->attrName,buf+count,name_len);
    count+=name_len;

    memcpy(&hash->attrLength,buf+count,sizeof(int));
    count+=sizeof(int);

    memcpy(&hash->numBuckets,buf+count,sizeof(int));
    count+=sizeof(int);

    memcpy(&name_len,buf+count,sizeof(int));
    count+=sizeof(int);
    hash->filename=malloc((name_len+1)*sizeof(char));  
    memcpy(hash->filename,buf+count,name_len);
    count+=name_len;


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

int SHT_CloseIndex(SHTinfo* info)  
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
    free(info->filename);

    if(BF_CloseFile(info->fileDesc) < 0)
    {
        BF_PrintError("Error closing BF");
        return -1;
    }
    
    free(info);

    S_FLAG_OPEN=false; //initialize the flag , so we can open again a file
    return 0;
}

int SHT_InsertEntry(SHTinfo info,SRecord srecord)  
{
    int i,now_block,count,numRecords,empty=-1,next_block,tempid;
    char *tempsurname=malloc(25*sizeof(char));
    i=hashFunStrings(info.numBuckets,srecord.surname);
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

        memcpy(buf+count,&srecord.blockid,sizeof(int));
        count+=sizeof(int);

        memcpy(buf+count,srecord.surname,25*sizeof(char));
        count+=25*sizeof(char);


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
       
        memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
        memcpy(&next_block,buf + BLOCK_SIZE - 2*sizeof(int),sizeof(int));
        count=0;

        while(numRecords > 0 )
        {
            memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
            memcpy(&tempid,buf+count,sizeof(int));
            count+=sizeof(int);
            memcpy(tempsurname,buf+count,25*sizeof(char));
            count+=25*sizeof(char);
            if(tempid == srecord.blockid && strcmp(tempsurname,srecord.surname) == 0 )
            {
                printf("Error:This record is already inserted\n");
                free(tempsurname);
                return 0;
            }
            else
            {
                if((count >=  BLOCK_SIZE - sizeof(SRecord)) && (next_block != -1))  //if we did not find the record in this block , go to next block
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
                else if(count >=  BLOCK_SIZE - sizeof(SRecord) && (next_block == -1))
                    break;
                //else if((count >= (numRecords*sizeof(SRecord) -2))&& (next_block == -1))   //if its the last block and we didnt find the record stop
                  //  break;
            }
        }
        memcpy(&numRecords,buf+BLOCK_SIZE-sizeof(int),sizeof(int));
        memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
        if(numRecords ==  BLOCK_SIZE/sizeof(SRecord) && next_block == -1) //if all the blocks are full in this bucket, then create a block
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

        count=29*numRecords;
        memcpy(buf+count,&(srecord.blockid),sizeof(int));
        count+=sizeof(int);
        memcpy(buf+count,srecord.surname,25*sizeof(char));
        count+=25*sizeof(char);
    
        numRecords++;
        memcpy(buf + BLOCK_SIZE - sizeof(int),&numRecords,sizeof(int)); 

        if(BF_WriteBlock(info.fileDesc,now_block))   // store the next block .
        {
            BF_PrintError("Error writing BF");
            return -1;
        }
    }
    free(tempsurname);
    return 0;
}


int SHT_InsertAll(SHTinfo sinfo,HTinfo info)
{
    char* buffer=NULL;
    void *buf,*newbuf;
    size_t bufsiz=110;
    int retval=0,i=0,now_block,next_block,count,numRecords;

    SRecord* srecord=NULL;
    
    buffer = malloc(bufsiz*sizeof(char));


    srecord = (SRecord*) malloc(sizeof(SRecord));

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
                srecord->blockid = now_block;
                count+=sizeof(int); // thats the offset for id of record in primary hashtable
                count+=15*sizeof(char); // thats the offset for name of record in primary hashtable

                memcpy(srecord->surname,buf+count,25*sizeof(char));  //get surname from primary hashtable
                count+=25*sizeof(char);
                count+=50*sizeof(char);  // thats the offset for address of record in primary hashtable
                
                // ~~~~~~~~~~~ here we insert the secondary record into Secondary hash table ~~~~~~~~~~~~
                if(SHT_InsertEntry(sinfo,*srecord) < 0)
                {
                    printf("error inserting entry\n");
                    return -1;
                }   


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
                    memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
                    count=0;
                }
                else if((count >=  94*numRecords)&& next_block == -1)   //if its the last block stop
                    break;
                
            }
        }
        
    }
        
    free(srecord);
    free(buffer);

    return 0;;
}

int SHT_SecondaryGetAllEntries(SHTinfo sinfo, HTinfo info, void *value)
{
    void * buf, *newbuf;
    int now_block,next_block,i,count,numRecords,val,numBlocks=0,scount=0,snumRecords=0;
    bool flag = true;
    
    if (value!=NULL)
    {   
        value=(char*)value;
        SRecord *srecord=malloc(sizeof(SRecord));
        Record *record=malloc(sizeof(Record));

        i = hashFunStrings(sinfo.numBuckets,value);
        now_block = sinfo.array[i];
        if(now_block!=-1)
        {
            if(BF_ReadBlock(sinfo.fileDesc,now_block,&buf))  
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
                
                memcpy(&srecord->blockid,buf+count,sizeof(int));
                count+=sizeof(int);
                memcpy(&srecord->surname,buf+count,25*sizeof(char));
                count+=25*sizeof(char);

                if(strcmp(srecord->surname,value)==0)
                {

                    if(BF_ReadBlock(info.fileDesc,srecord->blockid,&newbuf))  
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    scount = 0;
                    memcpy(&snumRecords,newbuf + BLOCK_SIZE - sizeof(int),sizeof(int));
                    while(snumRecords!=0)
                    {
                        memcpy(&record->id,newbuf+scount,sizeof(int));
                        scount+=sizeof(int);
                        memcpy(&record->name,newbuf+scount,15*sizeof(char));
                        scount+=15*sizeof(char);
                        memcpy(&record->surname,newbuf+scount,25*sizeof(char));
                        scount+=25*sizeof(char);
                        memcpy(&record->address,newbuf+scount,50*sizeof(char));
                        scount+=50*sizeof(char);

                        if(strcmp(srecord->surname,record->surname)==0)
                        {
                            printf("%d %s %s %s\n",record->id,record->name,record->surname,record->address);
                            flag = false;
                            break;
                        }
                        if ( scount >= 94*snumRecords )
                        {
                            printf("error surname exists in secondary ht but not in primary.\n");
                            break;
                        }
                    }
                }
                if(count >=  29*numRecords  && next_block != -1)  //go to next block
                {
                    now_block=next_block;
                    void* newbuf;
                    if(BF_ReadBlock(sinfo.fileDesc,next_block,&newbuf))  
                    {
                        BF_PrintError("Error reading BF");
                        return -1;
                    }
                    buf=newbuf;
                    memcpy(&next_block,buf+BLOCK_SIZE-2*sizeof(int),sizeof(int));
                    count=0;
                    memcpy(&numRecords,buf + BLOCK_SIZE - sizeof(int),sizeof(int));
                }
                else if((count >=  29*numRecords)&& next_block == -1)   //if its the last block stop
                {
                    if( flag )
                        printf("The surname does not exist.\n");
                    break;
                }
            }
        }
        free(srecord);
        free(record);
    }
    else
    {
        HT_GetAllEntries(info,NULL);
    }
}

int SHT_HashStatistics(char *fname)
{

    int now_block,next_block,count,numRecords , minRec, maxRec, sum, blockCounter, blocksPerBucketCounter=0, nonEmptyBuckets=0, overflowBuckets=0, overflowBlocks=0;
    float avgRec;
    void *buf,*newbuf;
    SHTinfo *info;

    info=SHT_OpenIndex(fname);

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
                count+=29*numRecords;

                if(count >=  29*numRecords  && next_block != -1)  //go to next block
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
                else if((count >=  29*numRecords)&& next_block == -1)   //if its the last block stop
                    break;
                
            }
            avgRec = (float)sum / (float)blockCounter;
            printf("In bucket %d  |  minimun records are: %d    |  maximun records are: %d    |  average records are: %.3f      |    overflow blocks are: %d\n",i,minRec,maxRec,avgRec,overflowBlocks);

        }
        
    }

    

    printf("The number of blocks are: %d     |      number of overflow buckets are: %d\n",BF_GetBlockCounter(info->fileDesc),overflowBuckets);
    printf("Average blocks per bucket: %.3f\n",(float)blocksPerBucketCounter/(float)nonEmptyBuckets);


    SHT_CloseIndex(info);
    return 0;
}



int SHT_Print(SHTinfo info)
{
    int now_block,next_block,i,count,numRecords;
    void *buf;
    SRecord* srecord=malloc(sizeof(SRecord));
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
                
                memcpy(&srecord->blockid,buf+count,sizeof(int));
                count+=sizeof(int);
                memcpy(&srecord->surname,buf+count,25*sizeof(char));
                count+=25*sizeof(char);


                printf("|| %d %s\n",srecord->blockid,srecord->surname);

                if(count >=  29*numRecords  && next_block != -1)  //go to next block
                {
                    printf("ALLO BLOCK   ????????????????????????????????????????? \n");
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
                else if((count >=  29*numRecords)&& next_block == -1)   //if its the last block stop
                    break;
                
            }
            printf("allo bucket~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        }
        
    }
    free(srecord);
}

