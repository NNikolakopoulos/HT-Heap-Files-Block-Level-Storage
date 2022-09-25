typedef struct {
    int fileDesc; /* αναγνωριστικός αριθµός ανοίγµατος αρχείου από το επίπεδο block */ 
    char attrType; /* ο τύπος του πεδίου που είναι κλειδί για το συγκεκριµένο αρχείο, 'c' ή'i' */ 
    char*  attrName; /* το όνοµα του πεδίου που είναι κλειδί για το συγκεκριµένο αρχείο */
    int attrLength;/* το µέγεθος του πεδίου που είναι κλειδί για το συγκεκριµένο αρχείο */
} HP_info;


typedef struct{
    int id;
    char name[15];
    char surname[25];
    char address[50];
}Record;

int HP_CreateFile(char *fileName,char attrType,char* attrName,int attrLength);
HP_info* HP_OpenFile(char *fname);
int HP_InsertEntry( HP_info header_info,Record record);
int HP_DeleteEntry( HP_info header_info,void *value);
int HP_GetAllEntries( HP_info header_info,void *value);
int HP_InsertAll(HP_info info,char* ftest);
void printRecord(Record *record);
int HP_InsertAll(HP_info info,char* ftest);
//Record* read_rec_from_file(Record *record,FILE *fd,char *buffer,size_t bufsiz)
int HP_Print(HP_info info);
int HP_GetAllEntries(HP_info header_info,void *value);
int HP_CloseFile( HP_info* header_info ) ;