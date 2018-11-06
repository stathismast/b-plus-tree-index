#include "AM.h"

int AM_errno = AME_OK;

#define CHECK_BF_ERROR(call)       		\
	{                               	\
		BF_ErrorCode code = call;   	\
		if (code != BF_OK) {   			\
			AM_errno =AME_ERROR;		\
			return AME_ERROR;       	\
		}                      			\
	}

#define CHECK_CREATE_ERROR(call)       				\
	{                               				\
		BF_ErrorCode code = call;   				\
		if (code != BF_OK) {        				\
			AM_errno = AME_CREATION_FILE_ERROR;     \
			return AME_CREATION_FILE_ERROR;       	\
		}                           				\
	}

#define CHECK_OPEN_ERROR(call)       			\
	{                               			\
		BF_ErrorCode code = call;   			\
		if (code != BF_OK) {        			\
			AM_errno = AME_OPENING_FILE_ERROR;	\
			return AME_OPENING_FILE_ERROR;		\
		}                           			\
	}

#define CHECK_CLOSE_ERROR(call)       			\
	{                               			\
		BF_ErrorCode code = call;   			\
		if (code != BF_OK) {        			\
			AM_errno = AME_CLOSING_FILE_ERROR;	\
			return AME_CLOSING_FILE_ERROR;		\
		}                           			\
	}

#define CHECK_INSERT_ERROR(call)       			\
	{                               			\
		BF_ErrorCode code = call;   			\
		if (code != 1) {        				\
			AM_errno = AME_INSERTION_ERROR;		\
			return AME_INSERTION_ERROR	;		\
		}                           			\
	}

#define RETURN_OPEN_ERROR()					\
	{										\
		AM_errno = AME_OPENING_FILE_ERROR;	\
		return AME_OPENING_FILE_ERROR;		\
	}

#define CHECK_LIMIT_ERROR(call) 				\
	{                           				\
		int code = call;        				\
		if (code < 0) {         				\
			AM_errno = AME_VALUE_OUT_LIMITS;	\
			return AME_VALUE_OUT_LIMITS;   		\
		}                       				\
	}

#define CHECK_TYPE_ERROR(call)       			\
	{                   	        			\
		int code = call;    	    			\
		if (code < 0) {         				\
			AM_errno = AME_WRONG_TYPE_LENGTH;	\
			return AME_WRONG_TYPE_LENGTH;   	\
		}                       				\
	}

#define FREE_WHAT_IT_NEEDS()                        \
	{                                               \
		CHECK_BF_ERROR(BF_UnpinBlock(block));       \
		BF_Block_Destroy(&block);                   \
		free(to_store);                             \
		free(s);                                    \
	}


file_info **open_files; 		/* array in which we store information about
								each open file */

scan_info **open_scans; 		/* array in which we store information about
									each scan */

/*Global struct which we use to return value from findNextEntry */
entry entryToReturn;

/* Initialization of global arrays for open_files and open_scans */
void AM_Init() {

	BF_Init(LRU);

	Open_Files_Init();

	Open_Scans_Init();

	entryToReturn.key = malloc(255);
	entryToReturn.value = malloc(255);

}

/* Creation of file and initialization of its first block so as to be an AM file */
int AM_CreateIndex(	char *fileName,
					char attrType1, /* type of first field : 'c' for string,
								'i' for integer,'f' for float */
					int attrLength1, /* length of first field : 4 for 'i' or
								'f',1-255 for 'c' */
					char attrType2, /* type of second field : 'c' for string,
								'i' for integer,'f' for float */
					int attrLength2) { 	/* length of second field : 4 for 'i' or 'f'
								,1-255 for 'c' */

	/* Do not create file if type or length is wrong */
	CHECK_TYPE_ERROR(checkTypeLength(attrType1,attrLength1));
	CHECK_TYPE_ERROR(checkTypeLength(attrType2,attrLength2));

	if ( (DATA_HEADER_SIZE + attrLength1 + attrLength2 > BF_BLOCK_SIZE) || (INDEX_HEADER_SIZE + attrLength1 + 2*sizeof(int) > BF_BLOCK_SIZE) ) {
		AM_errno = AME_CREATION_FILE_ERROR;
		return AME_CREATION_FILE_ERROR;
	}

	CHECK_CREATE_ERROR(BF_CreateFile(fileName));
	BF_Block *block;
	int fd,i,x,offs=0;
	BF_Block_Init(&block);
	char *data,*s;
	CHECK_CREATE_ERROR(BF_OpenFile(fileName,&fd));
	CHECK_CREATE_ERROR(BF_AllocateBlock(fd,block));

	s = malloc( strlen("AM_FILE") + 1 );
	assert( s!= NULL );
	strcpy(s,"AM_FILE");		/* identifying string at the beggining of the file
							so as we know it is an AM_FILE 	*/
	data = BF_Block_GetData(block);

	memcpy(data+offs,s,strlen(s)+1); 	/* store identifying string */
	offs += strlen(s)+1;


	memcpy(data+offs,&attrType1,sizeof(char));	/* store attrType1 */
	offs += sizeof(char);
	memcpy(data+offs,&attrLength1,sizeof(int)); /* store attrLength1 */
	offs += sizeof(int);


	memcpy(data+offs,&attrType2,sizeof(char));	/* store attrType2 */
	offs += sizeof(char);
	memcpy(data+offs,&attrLength2,sizeof(int)); /* store attrLength2 */
	offs += sizeof(int);

	x = 0;  	/* There is no root at the beggining,next times we need to
			update this info with the current block which represents the root */
	memcpy(data+offs,&x,sizeof(int)); /* store root_block_num */
	offs += sizeof(int);

	x = ( BF_BLOCK_SIZE - INDEX_HEADER_SIZE - sizeof(int) ) / ( sizeof(int) + attrLength1 ) + 1 ;	    /* max number of pointers in each index block */
	memcpy(data+offs,&x,sizeof(int));  /* store pointersPerBlock */
	offs += sizeof(int);
	x = ( BF_BLOCK_SIZE - DATA_HEADER_SIZE  ) / ( attrLength1 + attrLength2 ) ;	/* max number of entries in each data block */
	memcpy(data+offs,&x,sizeof(int)); /* store tuplesPerBlock */
	offs += sizeof(int);

	memset(data+offs,1,BF_BLOCK_SIZE-offs); //fill the remaining block with 1
	BF_Block_SetDirty(block);
	CHECK_CREATE_ERROR(BF_UnpinBlock(block));
	BF_CloseFile(fd);
	BF_Block_Destroy(&block);
	free(s);
	return AME_OK;
}

/* Remove file with fileName only if there is no open_scan on it. */
int AM_DestroyIndex(char *fileName) {
	int i;
	for ( i=0 ; i<MAX_OPEN_FILES ; i++ ){
		if( open_files[i] != NULL ){
			if( strcmp(open_files[i]->fileName,fileName) == 0 ){
				/* There is at least one instance of file with fileName open, so dont destroy it */
				AM_errno = AME_FILE_IS_USED;
				return AME_FILE_IS_USED;
			}
		}
	}
	/* There is no instance of file with fileName so we can remove it. */
	if( remove(fileName) != 0 ){
		AM_errno = AME_NO_SUCH_FILE;
		return AME_NO_SUCH_FILE;
	}
	return AME_OK;
}

/* Open file with name fileName only if it exists. Check its first block to see
	if it is an AM file and store its information to the global array.*/
int AM_OpenIndex (char *fileName) {
	int fd,pos,x,offs=0,i;
	char *data,*s,y;
	file_info *to_store;

	/*Check if this file exists and then open it */
	if( access( fileName, F_OK ) != 0 ) {     /* file doesn't exist */
		AM_errno = AME_NO_SUCH_FILE;
		return AME_NO_SUCH_FILE;
	}

	CHECK_OPEN_ERROR(BF_OpenFile(fileName,&fd));
	BF_Block *block;
	BF_Block_Init(&block);
	CHECK_OPEN_ERROR(BF_GetBlock(fd,0,block));

	data = BF_Block_GetData(block);

	to_store = malloc ( sizeof(file_info) );
	assert( to_store != NULL );
	/* Check if it is an AM FILE */
	s = malloc( strlen("AM_FILE") + 1 );
	memcpy(s,data,strlen("AM_FILE") + 1 );

	if( strcmp(s,"AM_FILE") != 0 ){  /* It is not an AM file */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	offs += strlen("AM_FILE") + 1;

	memcpy(&y,data+offs,sizeof(char));
	if( y < 0 ){				/* attrType1 must be a non negative value */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	to_store->attrType1 = y;
	offs += sizeof(char);

	memcpy(&x,data+offs,sizeof(int));
	if( x < 0 ){				/* attrLength1 must be a non negative value */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	to_store->attrLength1 = x;
	offs += sizeof(int);

	CHECK_TYPE_ERROR(checkTypeLength(to_store->attrType1,to_store->attrLength1));

	memcpy(&y,data+offs,sizeof(char));
	if( y < 0 ){				/* attrType2 must be a non negative value */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	to_store->attrType2 = y;
	offs += sizeof(char);

	memcpy(&x,data+offs,sizeof(int));
	if( x < 0 ){				/* attrLength2 must be a non negative value */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	to_store->attrLength2 = x;
	offs += sizeof(int);

	CHECK_TYPE_ERROR(checkTypeLength(to_store->attrType2,to_store->attrLength2));

	memcpy(&x,data+offs,sizeof(int));
	if( x < 0 ){				/* root_block_num must be a non negative value */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	to_store->root = x;
	offs += sizeof(int);

	memcpy(&x,data+offs,sizeof(int));
	if( x < 0 ){				/* pointersPerBlock must be a non negative value */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	to_store->pointersPerBlock = x;
	offs += sizeof(int);

	memcpy(&x,data+offs,sizeof(int));
	if( x < 0 ){				/* tuplesPerBlock must be a non negative value */
		FREE_WHAT_IT_NEEDS();
		RETURN_OPEN_ERROR();
	}
	to_store->tuplesPerBlock = x;
	offs += sizeof(int);

	/* The rest of block must be filled in with 1 */
	for(i=0;i<=BF_BLOCK_SIZE-offs;i++){

		memcpy(&y,data+offs,sizeof(char));
		if(y!=1){
			FREE_WHAT_IT_NEEDS();
			RETURN_OPEN_ERROR();
		}
		offs+=sizeof(char);
	}


	/* We have passed all tests and now we are sure that it is an AM file!  */
	to_store->fd = fd;
	strcpy(to_store->fileName,fileName);


	/* Find a free position in the global array of file_info */
	for( i = 0 ; i < MAX_OPEN_FILES ; i++ ){
		if( open_files[i] == NULL){
			break;
		}
	}

	if( i >= MAX_OPEN_FILES ){ /* There is no free position in the global array. */
		FREE_WHAT_IT_NEEDS();
		AM_errno = AME_OPEN_FILES_LIMIT;
		return AME_OPEN_FILES_LIMIT;
	}

	open_files[i] = to_store;

	CHECK_OPEN_ERROR(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
	free(s);
	return i;
}

/* Close instance of file that is at position fileDesc in the global array,
		only if the file with fileName has not open_scan on it. */
int AM_CloseIndex (int fileDesc) {
	int i;

	CHECK_LIMIT_ERROR(checkNumberLimits (fileDesc,0,MAX_OPEN_FILES));

	if( open_files[fileDesc] == NULL ){ /* There is nothing at this index */
		AM_errno = AME_CLOSING_FILE_ERROR;
		return AME_CLOSING_FILE_ERROR;
	}

	/* If there are open_scans on file with fileName dont close it. */
	for ( i=0 ; i<MAX_SCANS ; i++ ){
		if( open_scans[i] != NULL ){
			if( open_scans[i]->openFilesIndex == fileDesc ){
				/* There is at least one scan on this file with fileDesc, so cannot close it */
				AM_errno = AME_FILE_IS_USED;
				return AME_FILE_IS_USED;
			}
		}
	}


	/* There is no open_scan on this file, so we can close this instance of it and free the pos of the global array. */
	CHECK_CLOSE_ERROR(BF_CloseFile(open_files[fileDesc]->fd));
	free(open_files[fileDesc]);
	open_files[fileDesc] = NULL;
	return AME_OK;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {


	CHECK_LIMIT_ERROR(checkNumberLimits(fileDesc,0,MAX_OPEN_FILES));

	if (open_files[fileDesc] == NULL) {
		AM_errno = AME_INSERTION_ERROR;
		return AME_INSERTION_ERROR;
	}

	//case when the tree is completely empty so we need to make the root
	if(open_files[fileDesc]->root == 0) {
		//insert the data
		CHECK_INSERT_ERROR(Base_Insert(fileDesc, value1, value2));
		return AME_OK;
	}
	else {
		int dataBlockNum = getDataBlock(fileDesc, open_files[fileDesc]->root, value1);
		CHECK_INSERT_ERROR(Insert_Data(dataBlockNum,fileDesc, value1, value2));
		return AME_OK;
	}
}

/* Open a scan ( and store it to open_scans global array ) for file which is
	opened at index fileDesc of the global array and initialization of lastEntry */
int AM_OpenIndexScan(int fileDesc, int op, void *value) {
	int i;
	scan_info *to_store;

	/* Check fileDesc if it's between 0 and MAX_OPEN_FILES */
	CHECK_LIMIT_ERROR(checkNumberLimits(fileDesc,0,MAX_OPEN_FILES));
	/* Check op */
	CHECK_LIMIT_ERROR(checkNumberLimits(op,1,6));

	/* Find a free position in the global array of open_scans */
	for( i = 0 ; i < MAX_SCANS ; i++ ){
		if( open_scans[i] == NULL){
			break;
		}
	}

	if( i >= MAX_SCANS ){ /* There is no free position in the global array. */
		AM_errno = AME_OPEN_SCNANS_LIMIT;
		return AME_OPEN_SCNANS_LIMIT;
	}

	to_store = malloc ( sizeof(scan_info) );
	assert( to_store!= NULL );
	/* As we haven't found any entry with findNextEntry as it is our first time
		we initialize lastEntry with -1 */
	to_store->lastEntry.blockNum = -1;
	to_store->lastEntry.entryNum = -1;

	to_store->openFilesIndex = fileDesc;
	to_store->operator = op;

	/* void* will point to a value that is sizeof(attrLength1) */
	to_store->value = malloc ( open_files[fileDesc]->attrLength1 );
	assert( to_store->value != NULL );
	memcpy(to_store->value,value,open_files[fileDesc]->attrLength1);

	open_scans[i] = to_store;

	return i;	/* return scanDesc */
}


void *AM_FindNextEntry(int scanDesc) {

	/* Check scanDesc if it's between 0 and MAX_OPEN_FILES */
	if( checkNumberLimits(scanDesc,0,MAX_SCANS) < 0 ) {
		AM_errno = AME_SCAN_ERROR;
		return NULL;
	}

	int dataBlockNum,offset;

	if( open_scans[scanDesc] != NULL ){
		if( open_scans[scanDesc]->value != NULL ){
			if( open_scans[scanDesc]->lastEntry.blockNum == -1 ){ /* It's first time we call findNextEntry so we should call findFirstEntry */

				if( InitFirstEntry(scanDesc) == -1 ){ /* There is no such value so  EOF */
					AM_errno = AME_EOF;
					return NULL;
				}

			}
			else if( open_scans[scanDesc]->lastEntry.blockNum == -2 ){ /* There was an error last time we called findNextEntry */

				AM_errno = AME_SCAN_ERROR;
				return NULL;

			}
			else{	/* We have already information about lastEntry so we need to check the next one */

					open_scans[scanDesc]->lastEntry.entryNum ++;

			}

			/* Take the value of lastEntry and store it in entryToReturn*/
			int error = GetEntry(scanDesc);

			if( error == -1 ){ /* There is no next entry because there is no next block to access */

				AM_errno = AME_EOF;
				return NULL;

			}
			else if( error == -2 ){ /* A BF_ERROR occured */

				AM_errno = AME_SCAN_ERROR;
				return NULL;

			}

			/* Compare entryToReturn.key and open_scans value */
			error = EntryCompare(scanDesc);
			if( error == -1 ){ /* Next entry is not ok so return EOF or There is no next entry because there is no next block to access */

				AM_errno = AME_EOF;
				return NULL;

			}
			else if( error == -2){ /* A BF_ERROR occured */
				AM_errno = AME_SCAN_ERROR;
				return NULL;
			}
			else{
				return entryToReturn.value;
			}

		}
		else{

			AM_errno = AME_SCAN_ERROR;
			return NULL;

		}
	}
	else{

		AM_errno = AME_SCAN_ERROR;
		return NULL;

	}

}

/* Close scan with scanDesc and free this index from the global array */
int AM_CloseIndexScan(int scanDesc) {

	/* Check scanDesc if it's between 0 and MAX_OPEN_FILES */
	CHECK_LIMIT_ERROR(checkNumberLimits(scanDesc,0,MAX_SCANS));

	if( open_scans[scanDesc] != NULL ){
		if( open_scans[scanDesc]->value != NULL ){
			free( open_scans[scanDesc]->value );
		}
		else{
			AM_errno = AME_CLOSE_SCAN;
			return AME_CLOSE_SCAN;
		}
		free(open_scans[scanDesc]);
		open_scans[scanDesc] = NULL;
	}
	else{
		AM_errno = AME_CLOSE_SCAN;
		return AME_CLOSE_SCAN;
	}

	return AME_OK;
}


void AM_PrintError(char *errString) {

	printf("%s", errString);
	switch (AM_errno) {
		case AME_CREATION_FILE_ERROR:
			printf("An error occured while trying to create a file.\n\n");
			break;
		case AME_OPENING_FILE_ERROR:
			printf("An error occured while trying to open a file. Might not be an AM File or it's a corrupted one.\n\n");
			break;
		case AME_CLOSING_FILE_ERROR:
			printf("An error occured while trying to close a file.\n\n");
			break;
		case AME_NO_SUCH_FILE:
			printf("The file you are trying to open/access/delete does not exist.\n\n");
			break;
		case AME_EOF:
			printf("There are no other entries which meet the requirements of the open scan.\n\n");
			break;
		case AME_OPEN_FILES_LIMIT:
			printf("The max number of open files has been reached. Cannot open more files unless you close some.\n\n");
			break;
		case AME_OPEN_SCNANS_LIMIT:
			printf("The max number of open scans has been reached. Cannot open more scans unless you close some.\n\n");
			break;
		case AME_FILE_IS_USED:
			printf("The file you are trying to close/delete is being used.\n\n");
			break;
		case AME_INSERTION_ERROR:
			printf("An error occured while trying to insert an entry.\n\n");
			break;
		case AME_SCAN_ERROR:
			printf("AN error occured while scanning the file.\n\n");
			break;
		case AME_VALUE_OUT_LIMITS:
			printf("A value is out of its respective limits. Try with a different value.\n\n");
			break;
		case AME_WRONG_TYPE_LENGTH:
			printf("The length/type of the value is invalid. Could not create an AM File with that length/type.\n\n");
			break;
		case AME_CLOSE_SCAN:
			printf("An error occured while trying to close a scan.\n\n");
			break;
		case AME_ERROR:
			printf("An error occured but there are no more details. Try using BF_PrintError for more info.\n\n");
			break;
		default:
			printf("No recent error.\n\n");
			break;
	}

}

/* Destruction of global arrays for open_files and open_scans */
void AM_Close() {

	BF_Close();
	Open_Files_Destroy();
	Open_Scans_Destroy();
	free(entryToReturn.key);
	free(entryToReturn.value);

}
