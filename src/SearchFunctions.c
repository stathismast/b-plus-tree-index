#include "SearchFunctions.h"

extern file_info **open_files;
extern scan_info **open_scans;
extern entry entryToReturn;

#define CHECK_BF_ERROR2(call)        	\
	{                               	\
		BF_ErrorCode code = call;   	\
		if (code != BF_OK) {        	\
			return -2;       	\
		}                           	\
	}

#define CHECK_BF_ERROR(call)        	\
	{                               	\
		BF_ErrorCode code = call;   	\
		if (code != BF_OK) {        	\
			return AME_ERROR;       	\
		}                           	\
	}

/* Store value and key of entry with entryNum in entryToReturn if entry exists or
	nextDatablock exists and update lastEntry,else return -1 */
int GetEntry(int scanDesc){

	int fd = open_files[ open_scans[scanDesc]->openFilesIndex ]-> fd;
	int blockNum = open_scans[scanDesc]->lastEntry.blockNum;
	int entryNum = open_scans[scanDesc]->lastEntry.entryNum;
	int attrLength1 = open_files[ open_scans[scanDesc]->openFilesIndex ]-> attrLength1;
	int attrLength2 = open_files[ open_scans[scanDesc]->openFilesIndex ]-> attrLength2;
	int offset = 0;
	char *data;
	dataBlockHeader header;
	BF_Block *block;
	BF_Block_Init(&block);
	CHECK_BF_ERROR2(BF_GetBlock(fd,blockNum,block));

	data = BF_Block_GetData(block);

	/* retrieve header */
	memcpy(&(header.type),data+offset,sizeof(header.type));
	offset += sizeof(header.type);
	memcpy(&(header.parent),data+offset,sizeof(header.parent));
	offset += sizeof(header.parent);
	memcpy(&(header.blockFullness),data+offset,sizeof(header.blockFullness));
	offset += sizeof(header.blockFullness);
	memcpy(&(header.nextDataBlock),data+offset,sizeof(header.nextDataBlock));
	offset += sizeof(header.nextDataBlock);

	/* Check if entryNum exists in current block , else go to nextDatablock if exists. We do header.blockFullness -1 as entryNum begins from 0 */
	if( entryNum <= header.blockFullness-1 ){	/* This entryNum exists in current block */

		/* store (key,value) which is at position entryNum --- entryNum starts from 0 */
		offset += entryNum*(attrLength1+attrLength2);
		memcpy(entryToReturn.key,data+offset,attrLength1);
		offset += attrLength1;
		memcpy(entryToReturn.value,data+offset,attrLength2);
		offset += attrLength2;
		CHECK_BF_ERROR2(BF_UnpinBlock(block));
		BF_Block_Destroy(&block);

	}
	else{	/* We must check if nextDatablock exists */

		if( header.nextDataBlock != 0 ){	/* There is nextDatablock */
			/* There will be always at least one entry in each DataBlock */

			/* Update lastEntry */
			open_scans[scanDesc]->lastEntry.blockNum = header.nextDataBlock;
			open_scans[scanDesc]->lastEntry.entryNum = 0;
			CHECK_BF_ERROR2(BF_UnpinBlock(block));

			/* Get next Block */
			CHECK_BF_ERROR2(BF_GetBlock(fd,header.nextDataBlock,block));
			data = BF_Block_GetData(block);

			/* store (key,value) which is at position entryNum --- entryNum starts from 0 */
			offset += (open_scans[scanDesc]->lastEntry.entryNum)*(attrLength1+attrLength2);
			memcpy(entryToReturn.key,data+offset,attrLength1);
			offset += attrLength1;
			memcpy(entryToReturn.value,data+offset,attrLength2);
			offset += attrLength2;
			CHECK_BF_ERROR2(BF_UnpinBlock(block));
			BF_Block_Destroy(&block);
		}
		else{ /* There is no nextDatablock */
			/* update lastEntry with error ( -2 ) */
			open_scans[scanDesc]->lastEntry.blockNum = -2;
			open_scans[scanDesc]->lastEntry.entryNum = -2;
			CHECK_BF_ERROR2(BF_UnpinBlock(block));
			BF_Block_Destroy(&block);
			return -1;
		}
	}
	return 1;
}

/* Initialization of first Entry to check taking in consideration the value of the operator */
int InitFirstEntry(int scanDesc){
	int dataBlockNum,offset;

	switch ( open_scans[scanDesc]->operator ) {
		case GREATER_THAN_OR_EQUAL:
		case GREATER_THAN:
		case EQUAL:					/* We need to see where the value is or where it should be it */
									dataBlockNum = getDataBlock( open_scans[scanDesc]->openFilesIndex,\
									open_files[ open_scans[scanDesc]->openFilesIndex ]->root,open_scans[scanDesc]->value );
									offset = findFirstEntry( open_scans[scanDesc]->openFilesIndex, dataBlockNum, open_scans[scanDesc]->value );

									if ( offset == -1 ){ /* There is no such value */
										open_scans[scanDesc]->lastEntry.blockNum = -2;
										open_scans[scanDesc]->lastEntry.entryNum = -2;
										return -1;
									}

									open_scans[scanDesc]->lastEntry.blockNum = dataBlockNum;
									/* entryNum begins from 0 */
									open_scans[scanDesc]->lastEntry.entryNum = (offset - DATA_HEADER_SIZE) / \
									(open_files[ open_scans[scanDesc]->openFilesIndex ]->attrLength1 \
									+ open_files[ open_scans[scanDesc]->openFilesIndex ]->attrLength2);
									break;
		case LESS_THAN_OR_EQUAL:
		case LESS_THAN:
		case NOT_EQUAL:				/* We begin from lowest key which is always at blockNum 2 and entryNum 0 */
									open_scans[scanDesc]->lastEntry.blockNum = 2;
									/* entryNum begins from 0 */
									open_scans[scanDesc]->lastEntry.entryNum = 0;
									break;
	}

}

/* Comparison of entryToReturn and open_scans->value taking in consideration the value of the operator */
int EntryCompare(int scanDesc){

	switch ( open_scans[scanDesc]->operator ) {
		case LESS_THAN_OR_EQUAL:
		case LESS_THAN:
		case EQUAL:					if( compare(entryToReturn.key,open_scans[scanDesc]->operator,\
										open_scans[scanDesc]->value,open_files[ open_scans[scanDesc]->openFilesIndex ]->attrType1) == 1 ){
										/* Next entry is ok  */
										return 1;
									}
									else{
										/* Next entry is not ok */
										return -1;
									}
									break;
		case GREATER_THAN_OR_EQUAL:
		case GREATER_THAN:
		case NOT_EQUAL:				/* EOF only if there is no next block, else we just skip current entry */
									if( compare(entryToReturn.key,open_scans[scanDesc]->operator,\
										open_scans[scanDesc]->value,open_files[ open_scans[scanDesc]->openFilesIndex ]->attrType1) == 1 ){
										/* Next entry is ok so return entryToReturn.value */
										return 1;
									}
									else{ /* Skip  entries that do not match the comparison */
										while(1){
											open_scans[scanDesc]->lastEntry.entryNum ++;
											int error = GetEntry(scanDesc);
											if( error == -1 ){ /* There is no next entry because there is no next block to access */
												return -1;
											}
											else if( error == -2 ){ /* A BF_ERROR occured */
												return -2;
											}
											if( compare(entryToReturn.key,open_scans[scanDesc]->operator,\
												open_scans[scanDesc]->value,open_files[ open_scans[scanDesc]->openFilesIndex ]->attrType1) == 1 ){
												/* Next entry is ok so return entryToReturn.value */
												return 1;
											}
										}
									}
									break;
	}
}

int compare(void * left, int op, void *right, char type){
	switch(type){
		case 'i':
			switch(op){
				case EQUAL:
					return (*((int *)left) == *((int *)right));
				case NOT_EQUAL:
					return (*((int *)left) != *((int *)right));
				case LESS_THAN:
					return (*((int *)left) <  *((int *)right));
				case GREATER_THAN:
					return (*((int *)left) >  *((int *)right));
				case LESS_THAN_OR_EQUAL:
					return (*((int *)left) <= *((int *)right));
				case GREATER_THAN_OR_EQUAL:
					return (*((int *)left) >= *((int *)right));
				default:
					return -1;
			}
			break;
		case 'f':
			switch(op){
				case EQUAL:
					return (*((float *)left) == *((float *)right));
				case NOT_EQUAL:
					return (*((float *)left) != *((float *)right));
				case LESS_THAN:
					return (*((float *)left) <  *((float *)right));
				case GREATER_THAN:
					return (*((float *)left) >  *((float *)right));
				case LESS_THAN_OR_EQUAL:
					return (*((float *)left) <= *((float *)right));
				case GREATER_THAN_OR_EQUAL:
					return (*((float *)left) >= *((float *)right));
				default:
					return -1;
			}
			break;
		case 'c':;
			int result = strcmp(((char *)left), ((char *)right));
			switch(op){
				case EQUAL:
					return (result == 0);
				case NOT_EQUAL:
					return (result != 0);
				case LESS_THAN:
					return (result <  0);
				case GREATER_THAN:
					return (result >  0);
				case LESS_THAN_OR_EQUAL:
					return (result <= 0);
				case GREATER_THAN_OR_EQUAL:
					return (result >= 0);
				default:
					return -1;
			}
			break;
		default:
			return -1;
	}
}

int getDataBlock(int openFilesIndex, int index, void * key){

	int nextBlockIndex,offset,blockFullness,i;

	void *nextKey = malloc(open_files[openFilesIndex]->attrLength1);
	int fd = open_files[openFilesIndex]->fd;
	char type = open_files[openFilesIndex]->attrType1;
	int attrLength1 = open_files[openFilesIndex]->attrLength1;
	BF_Block * block;
	BF_Block_Init(&block);


	char * data;
	while(1){ /* Do loop till we reach a data block */

		CHECK_BF_ERROR(BF_GetBlock(fd, index, block)); //load root index block
		data = BF_Block_GetData(block);

		offset = sizeof(char) + sizeof(int);
		memcpy(&blockFullness,data+offset,sizeof(int));

		if(isDataBlock(data)){ /* If we reach a data block we stop */

			BF_UnpinBlock(block);
			BF_Block_Destroy(&block);
			free(nextKey);
			return index;

		}
		else{ /* Check all keys till we find a key which is GREATER_THAN the key we search */

			if( blockFullness == 1 ){ /* There is only one pointer in this indexBlock,the first one and no key to compare, so we take this first one pointer */

				offset = INDEX_HEADER_SIZE;
				memcpy(&nextBlockIndex,data+offset,sizeof(int));

			}
			else{

				offset = INDEX_HEADER_SIZE + sizeof(int); /* We start from first key, which is right to the first pointer */

				/* Number of keys in each indexBlock is blockFullness-1 */
				for( i=0; i<blockFullness-1; i++ ){ /* Check all keys till find a key which is greater than the key we want to reach  */

					memcpy(nextKey,data+offset,attrLength1); /* Take the key to compare with the key we search */

					if( compare(nextKey,GREATER_THAN,key,type) == 1 ){ /* We found a key greater than the key we want to reach */

						/* So we take the pointer which is left the nextKey */
						offset -= sizeof(int);
						memcpy(&nextBlockIndex,data+offset,sizeof(int));
						break;

					}

					offset += attrLength1 + sizeof(int); /* Check next key */

				}

				if( i == blockFullness-1 ){ /* There is no key GREATER_THAN the key we want to search so we should get the pointer which is right the last key */

					offset = INDEX_HEADER_SIZE + ( (blockFullness-1) * (sizeof(int)+attrLength1) );
					memcpy(&nextBlockIndex,data+offset,sizeof(int));

				}

			}

			index = nextBlockIndex;
			BF_UnpinBlock(block);

		}
	}
}

/* Find the offset ( from the beggining of the DataBlock ) of entry with key or if there is no such an entry
	find where it should there be */
int findFirstEntry(int openFilesIndex, int blockNum, void * key){

	int offset,i,blockFullness;
	void *nextKey = malloc(open_files[openFilesIndex]->attrLength1);
	int fd    = open_files[openFilesIndex]->fd;
	char type  = open_files[openFilesIndex]->attrType1;
	int attrLength1 = open_files[openFilesIndex]->attrLength1;
	int attrLength2 = open_files[openFilesIndex]->attrLength2;

	BF_Block * block;
	BF_Block_Init(&block);
	CHECK_BF_ERROR(BF_GetBlock(fd, blockNum, block));
	char * data;
	data = BF_Block_GetData(block);

	offset = sizeof(char) + sizeof(int);
	memcpy(&blockFullness,data+offset,sizeof(int));

	offset = DATA_HEADER_SIZE;
	for( i=0; i<blockFullness; i++ ){

		memcpy(nextKey,data+offset,attrLength1); /* Take the key to compare with the key we search */
		if( compare(key,LESS_THAN_OR_EQUAL,nextKey,type) == 1 ){ /* key <= nextKey  */
			/* We found where the key is, or if there is no such a key, it should be inserted at this offset */
			BF_UnpinBlock(block);
			BF_Block_Destroy(&block);
			free(nextKey);
			return offset;

		}

		offset += attrLength1 + attrLength2;

	}

	/* If we reach here it means that there is no such a key in this block, and if we want to insert one
		it should be inserted at the end of the block */
	BF_UnpinBlock(block);
	BF_Block_Destroy(&block);
	free(nextKey);
	return -1;
}

/* Find the offset ( from the beggining of the IndexBlock ) key, or if there is no such a key
	find where it should there be */
int findFirstKey(int openFilesIndex, int blockNum, void * key){

	int offset,i,blockFullness;
	void *nextKey = malloc(open_files[openFilesIndex]->attrLength1);
	int fd    = open_files[openFilesIndex]->fd;
	char type  = open_files[openFilesIndex]->attrType1;
	int attrLength1 = open_files[openFilesIndex]->attrLength1;

	BF_Block * block;
	BF_Block_Init(&block);
	CHECK_BF_ERROR(BF_GetBlock(fd, blockNum, block));
	char * data;
	data = BF_Block_GetData(block);

	offset = sizeof(char) + sizeof(int);
	memcpy(&blockFullness,data+offset,sizeof(int));

	offset = INDEX_HEADER_SIZE + sizeof(int); /*First key is right to the first pointer */
	/* Number of keys in each indexBlock is blockFullness-1 */
	for( i=0; i<blockFullness-1; i++ ){

		memcpy(nextKey,data+offset,attrLength1); /* Take the key to compare with the key we search */
		if( compare(key,LESS_THAN_OR_EQUAL,nextKey,type) == 1 ){ /* key <= nextKey  */
			/* We found where the key is, or if there is no such a key, it should be inserted at this offset */
			BF_UnpinBlock(block);
			BF_Block_Destroy(&block);
			free(nextKey);
			return offset;
		}
		offset += attrLength1 + sizeof(int);

	}

	/* If we reach here it means that there is no such a key in this block, and if we want to insert one
		it should be inserted at the end of the block */
	BF_UnpinBlock(block);
	BF_Block_Destroy(&block);
	free(nextKey);
	return -1;
}

int isDataBlock(char * data){
	char identifier;
	memcpy(&identifier, data,sizeof(char));
	if(identifier == 'd'){
		return 1;
	}
	return 0;
}
