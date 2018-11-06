#include "Insert_Functions.h"


#define CHECK_BF_ERROR(call)        \
	{                               \
		BF_ErrorCode code = call;   \
		if (code != BF_OK) {        \
			return -1;       \
		}                           \
	}

#define CHECK_ERROR(call)   \
	{                       \
		int code = call;    \
		if (code < 0) {     \
			return -1;		\
		}                   \
	}


#define WRITE_GARBAGE(data, offs)						\
	{													\
		memset(data+offs, -1, BF_BLOCK_SIZE-offs);		\
	}

#define CREATE_NEW_BLOCK(fileDesc, block)							\
	{																\
		BF_Block_Init(&block);										\
		CHECK_BF_ERROR(BF_AllocateBlock(fileDesc, block));			\
	}

#define DESTROY_BLOCK(block)		\
	{								\
		BF_Block_SetDirty(block);	\
		BF_UnpinBlock(block);		\
		BF_Block_Destroy(&block);	\
	}

extern file_info **open_files;

//writes the data  in the block, in a dataBlock
void Write_Data(int* offs, char* data, void* value1, void* value2, int openFilesIndex) {

	int attrLength1 = open_files[openFilesIndex]->attrLength1;
	int attrLength2 = open_files[openFilesIndex]->attrLength2;
	char type1 = open_files[openFilesIndex]->attrType1;
	char type2 = open_files[openFilesIndex]->attrType2;
	int stop;

	if( type1 == 'c' ){
		/* if type is 'c' we write the strlen(((char *)value1))+1 bytes
		and then we fill with 0 the rest bytes till attrLength1 so as not to have unitialized bytes */
		memcpy(data+(*offs),value1,strlen(((char *)value1))+1);
		stop = attrLength1 - ( strlen(((char *)value1)) + 1 )  ;
		memset(data+(*offs)+strlen(((char *)value1))+1,0,stop);
		(*offs) += attrLength1;
	}
	else{
		memcpy(data+(*offs), value1, attrLength1);
		(*offs) += attrLength1;
	}

	if( type2 == 'c' ){
		memcpy(data+(*offs),value2,strlen(((char *)value2))+1);
		stop = attrLength2 - ( strlen(((char *)value2)) + 1 )  ;
		memset(data+(*offs)+strlen(((char *)value2))+1,0,stop);
		(*offs) += attrLength2;
	}
	else{
		memcpy(data+(*offs), value2, attrLength2);
		(*offs) += attrLength2;
	}

}

//Write_Data2 same as Write_Data but here the second value is not attrType2 so we cannot use this info from the global table
void Write_Data2(int* offs, char* data, void* value1, void* value2,int attrLength1,int attrLength2,int openFilesIndex) {

	char type1 = open_files[openFilesIndex]->attrType1;
	int stop;

	if( type1 == 'c' ){
		memcpy(data+(*offs),value1,strlen(((char *)value1))+1);
		stop = attrLength1 - ( strlen(((char *)value1)) + 1 )  ;
		memset(data+(*offs)+strlen(((char *)value1))+1,0,stop);
		(*offs) += attrLength1;
	}
	else{
		memcpy(data+(*offs), value1, attrLength1);
		(*offs) += attrLength1;
	}

	memcpy(data+(*offs), value2, attrLength2);
	(*offs) += attrLength2;

}


//Inserts data when there are no blocks. It creates the root and the first data block
int Base_Insert(int openFilesIndex, void* value1, void* value2) {

	int offs,pointer;
	BF_Block* block;
	char* data;

	int attrLength1 = open_files[openFilesIndex]->attrLength1;
	int attrLength2 = open_files[openFilesIndex]->attrLength2;
	int fd = open_files[openFilesIndex]->fd;
	indexBlockHeader root;
	dataBlockHeader data_block;

//////////////////////CREATE ROOT/////////////////////////////////////////
	CREATE_NEW_BLOCK(fd, block);
	data = BF_Block_GetData(block);

	//write the metadata
	offs=0;
	//determine if it's data or index block
	root.type = 'i';
	memcpy(data+offs,&(root.type),sizeof(root.type));
	offs += sizeof(root.type);

	//the father of the respective block. Since the root has no father it's set to 0
	root.parent = 0;
	memcpy(data+offs,&(root.parent),sizeof(root.parent));
	offs += sizeof(root.parent);

	//write how many entries there are in it
	root.blockFullness = 1;
	memcpy(data+offs,&(root.blockFullness),sizeof(root.blockFullness));
	offs += sizeof(root.blockFullness);

	//since it's the first insertion the first pointer of the root will
	//point to the first data block which is gonna be block #2 in the file
	pointer = 2;
	memcpy(data+offs,&(pointer),sizeof(int));
	offs += sizeof(int);

	//write garbage
	WRITE_GARBAGE(data, offs);

	DESTROY_BLOCK(block);

//////////////////////CREATE DATA BLOCK//////////////////////////////////////////
	CREATE_NEW_BLOCK(fd, block);
	data = BF_Block_GetData(block);

	offs=0;

	//determine that it is a data block
	data_block.type = 'd';
	memcpy(data+offs,&(data_block.type),sizeof(data_block.type));
	offs += sizeof(data_block.type);

	//write the father of this block which is gonna be the root
	data_block.parent = 1;
	memcpy(data+offs,&(data_block.parent),sizeof(data_block.parent));
	offs += sizeof(data_block.parent);

	//write how many entries there are in it
	data_block.blockFullness = 1;
	memcpy(data+offs,&(data_block.blockFullness),sizeof(data_block.blockFullness));
	offs += sizeof(data_block.blockFullness);

	//write the pointer to next block
	data_block.nextDataBlock = 0;
	memcpy(data+offs,&(data_block.nextDataBlock),sizeof(data_block.nextDataBlock));
	offs += sizeof(data_block.nextDataBlock);

	//write the data
	Write_Data(&offs, data, value1, value2, openFilesIndex);
	//write garbage
	WRITE_GARBAGE(data, offs);

	DESTROY_BLOCK(block);

	//update the oot in the open files array and the file
	Update_Root(1, openFilesIndex);

	return 1;
}

//Insertion of data in any other case expect root.
int Insert_Data(int block_num, int openFilesIndex, void* value1, void* value2) {

	int offs, offs2;
	BF_Block *block, *new_block;
	char *data, *new_data;
	void *key;

	dataBlockHeader old_block,new_one_block;

	int attrLength1 = open_files[openFilesIndex]->attrLength1;
	int attrType1 = open_files[openFilesIndex]->attrType1;
	int attrLength2 = open_files[openFilesIndex]->attrLength2;
	int MaxTuplesCount = open_files[openFilesIndex]->tuplesPerBlock;
	int maxPointerCount = open_files[openFilesIndex]->pointersPerBlock;
	int fileDesc = open_files[openFilesIndex]->fd;

	BF_Block_Init(&block);
	CHECK_BF_ERROR(BF_GetBlock(fileDesc, block_num, block));
	data = BF_Block_GetData(block);

	offs = sizeof(char);
	memcpy(&(old_block.parent), data+offs, sizeof(old_block.parent));

	offs += sizeof(old_block.parent);
	memcpy(&(old_block.blockFullness), data+offs, sizeof(old_block.blockFullness));


	if (old_block.blockFullness < MaxTuplesCount) { 	/* There is space in old_block */

		int firstEntryOffset = findFirstEntry(openFilesIndex, block_num, value1);

		if( firstEntryOffset == -1 ){ /* Insert entry at the end of the block */

			offs = DATA_HEADER_SIZE + old_block.blockFullness*(attrLength1 + attrLength2);
			Write_Data(&offs, data, value1, value2, openFilesIndex);

		}
		else{

			int firstEntryNum = (firstEntryOffset-DATA_HEADER_SIZE)/(attrLength1+attrLength2);
			memmove(data+firstEntryOffset+attrLength1+attrLength2, data+firstEntryOffset, (old_block.blockFullness - firstEntryNum)*(attrLength1+attrLength2));
			offs = firstEntryOffset;
			Write_Data(&offs, data, value1, value2, openFilesIndex);

		}

		/* Update old_block.blockFullness */
		old_block.blockFullness++;
		offs = sizeof(char) + sizeof(int);
		memcpy(data+offs, &(old_block.blockFullness), sizeof(old_block.blockFullness));
		DESTROY_BLOCK(block);
		return 1;

	}
	else {	/* There is no space in old_block, so we must create a new block */

		CREATE_NEW_BLOCK(fileDesc, new_block);
		new_data = BF_Block_GetData(new_block);

		//write the metadata in the new block
		offs2=0;

		//determine that it is a data block
		new_one_block.type = 'd';
		memcpy(new_data+offs2, &(new_one_block.type), sizeof(new_one_block.type));
		offs2 += sizeof(new_one_block.type);

		offs2 += sizeof(new_one_block.parent);
		//write how many entries there are in it

		if (MaxTuplesCount%2 == 0)
			old_block.blockFullness = old_block.blockFullness/2;
		else
			old_block.blockFullness = old_block.blockFullness/2 + 1;

		//Make sure we are not splitting duplicate keys
		void * leftOfBreakPoint = malloc(attrLength1);
		void * rightOfBreakPoint = malloc(attrLength1);

		offs = DATA_HEADER_SIZE+(old_block.blockFullness-1)*(attrLength1+attrLength2);
		memcpy(leftOfBreakPoint, data+offs, attrLength1);

		offs = DATA_HEADER_SIZE+(old_block.blockFullness)*(attrLength1+attrLength2);
		memcpy(rightOfBreakPoint, data+offs, attrLength1);

		if (compare(leftOfBreakPoint, EQUAL, rightOfBreakPoint, attrType1)){
			int walkLeft = old_block.blockFullness;
			int walkRight = old_block.blockFullness;

			while (compare(leftOfBreakPoint, EQUAL, rightOfBreakPoint, attrType1)){
				walkLeft--;
				if (walkLeft > 0){
					memcpy(leftOfBreakPoint, data+DATA_HEADER_SIZE+(walkLeft-1)*(attrLength1+attrLength2), attrLength1);
					memcpy(rightOfBreakPoint, data+DATA_HEADER_SIZE+(walkLeft)*(attrLength1+attrLength2), attrLength1);
				}
				else if (walkLeft == 0){
					break;
				}
			}

			memcpy(leftOfBreakPoint, data+DATA_HEADER_SIZE+(old_block.blockFullness-1)*(attrLength1+attrLength2), attrLength1);
			memcpy(rightOfBreakPoint, data+DATA_HEADER_SIZE+(old_block.blockFullness)*(attrLength1+attrLength2), attrLength1);
			while (compare(leftOfBreakPoint, EQUAL, rightOfBreakPoint, attrType1)){
				walkRight++;
				if (walkRight < MaxTuplesCount){
					memcpy(leftOfBreakPoint, data+DATA_HEADER_SIZE+(walkRight-1)*(attrLength1+attrLength2), attrLength1);
					memcpy(rightOfBreakPoint, data+DATA_HEADER_SIZE+(walkRight)*(attrLength1+attrLength2), attrLength1);
				}
				else if(walkRight == MaxTuplesCount){
					break;
				}
			}

			if(old_block.blockFullness - walkLeft < walkRight - old_block.blockFullness){
				old_block.blockFullness = walkLeft;
			}
			else {
				old_block.blockFullness = walkRight;
			}

		}
		free(leftOfBreakPoint);
		free(rightOfBreakPoint);

		/* The "extra" entry will go to old_block */
		new_one_block.blockFullness = MaxTuplesCount - old_block.blockFullness;

		if(new_one_block.blockFullness == 0){
			void *duplicateKey = malloc(attrLength1);
			//load value of duplicate keys
			memcpy(duplicateKey, data+DATA_HEADER_SIZE, attrLength1);
			if(compare(value1, GREATER_THAN, duplicateKey, attrType1)){
				new_one_block.blockFullness++;
				memcpy(new_data+offs2, &(new_one_block.blockFullness), sizeof(new_one_block.blockFullness));
				offs2 += sizeof(new_one_block.blockFullness);
				new_one_block.blockFullness--;
			}
			else{
				new_one_block.blockFullness = MaxTuplesCount;
				memcpy(new_data+offs2, &(new_one_block.blockFullness), sizeof(new_one_block.blockFullness));
				offs2 += sizeof(new_one_block.blockFullness);
				old_block.blockFullness = 0;
			}
			free(duplicateKey);
		}
		else{
			memcpy(new_data+offs2, &(new_one_block.blockFullness), sizeof(new_one_block.blockFullness));
			offs2 += sizeof(new_one_block.blockFullness);
		}

		//write the pointer to next block.
		offs = sizeof(char) + 2*sizeof(int);
		memcpy(&(old_block.nextDataBlock), data+offs, sizeof(old_block.nextDataBlock));
		new_one_block.nextDataBlock = old_block.nextDataBlock;

		memcpy(new_data+offs2, &(new_one_block.nextDataBlock), sizeof(new_one_block.nextDataBlock));
		offs2 += sizeof(new_one_block.nextDataBlock);

		//update the next pointer of the old block which will be the num of last block we allocated
		offs = sizeof(char) + 2*sizeof(int);
		BF_GetBlockCounter(fileDesc, &old_block.nextDataBlock);

		old_block.nextDataBlock-=1;
		memcpy(data + offs, &(old_block.nextDataBlock), sizeof(old_block.nextDataBlock));

		//updates the old_block.blockFullness of the original block with how many entries will remain now
		offs -= sizeof(int);
		memcpy(data+offs, &(old_block.blockFullness), sizeof(old_block.blockFullness));

		//copy the data from the first block
		offs = DATA_HEADER_SIZE + old_block.blockFullness*(attrLength1 + attrLength2);
		memcpy(new_data + offs2, data + offs, new_one_block.blockFullness*(attrLength1+attrLength2));

		WRITE_GARBAGE(data, offs);

		offs = DATA_HEADER_SIZE + new_one_block.blockFullness*(attrLength1 + attrLength2);
		WRITE_GARBAGE(new_data, offs);

		//get the first key of the new block to insert it to the index
		offs = DATA_HEADER_SIZE;
		key = malloc(attrLength1);
		memcpy(key, new_data+offs, attrLength1);


		if(new_one_block.blockFullness == 0){
			offs = DATA_HEADER_SIZE;
			Write_Data(&offs, new_data, value1, value2, openFilesIndex);
			Insert_Index(old_block.nextDataBlock, old_block.parent, openFilesIndex, value1);
		}
		else if(old_block.blockFullness == 0){
		    old_block.blockFullness = 1;
		    memcpy(data+sizeof(char)+sizeof(int), &(old_block.blockFullness), sizeof(old_block.blockFullness));
			offs = DATA_HEADER_SIZE;
			Write_Data(&offs, data, value1, value2, openFilesIndex);
			Insert_Index(old_block.nextDataBlock, old_block.parent, openFilesIndex, key);
		}
		else{
			Insert_Index(old_block.nextDataBlock, old_block.parent, openFilesIndex, key);

			/* We did the right changes and now there is space for the next entry, so we invoke again Insert_Data  */
			int dataBlockNum = getDataBlock(openFilesIndex, open_files[openFilesIndex]->root, value1);
			Insert_Data(dataBlockNum, openFilesIndex, value1, value2);
		}

		DESTROY_BLOCK(block);
		DESTROY_BLOCK(new_block);
		free(key);
		return 1;
	}
}

//Inserts the pair key,pointer in our index block
int Insert_Index(int child_block, int parent_block, int openFilesIndex, void* key) {

	BF_Block* block, *new_block, *root_block, *child;
	char *data, *new_data, block_type, *root_data, *child_data;
	int offs, offs2,new_block_ptr, root_offs;

	void* middle_key;

	indexBlockHeader old_block,new_one_block,root_block_hd;

	int attrLength1 = open_files[openFilesIndex]->attrLength1;
	int attrType1 = open_files[openFilesIndex]->attrType1;
	int maxPointerCount = open_files[openFilesIndex]->pointersPerBlock;
	int fd = open_files[openFilesIndex]->fd;
	int root = open_files[openFilesIndex]->root;
	BF_Block_Init(&block);

	//gets the block we have to do insert the insertion to check
	CHECK_BF_ERROR(BF_GetBlock(fd, parent_block, block));
	data = BF_Block_GetData(block);

	offs = sizeof(char) + sizeof(int);
	memcpy(&(old_block.blockFullness), data+offs, sizeof(old_block.blockFullness));

	if (old_block.blockFullness < maxPointerCount) { /* There is space in the old block */

		int keyOffset = findFirstKey(openFilesIndex, parent_block, key);

		if( keyOffset == -1 ){ /* Insert it at the end of the block */

			offs = INDEX_HEADER_SIZE + old_block.blockFullness*sizeof(int) + (old_block.blockFullness-1)*attrLength1;
			Write_Data2(&offs, data, key, &child_block, attrLength1, sizeof(int),openFilesIndex);

		}
		else{

			int keyNum = (keyOffset-INDEX_HEADER_SIZE - sizeof(int))/(attrLength1+sizeof(int));
			memmove(data+keyOffset+attrLength1+sizeof(int), data+keyOffset, (old_block.blockFullness - keyNum -1)*(attrLength1+sizeof(int)));
			offs = keyOffset;
			Write_Data2(&offs, data, key, &child_block, attrLength1, sizeof(int),openFilesIndex);

		}

		//write the pointer and the key to the parent

		old_block.blockFullness++;
		offs = sizeof(char) + sizeof(int);
		memcpy(data+offs, &(old_block.blockFullness), sizeof(int));
		DESTROY_BLOCK(block);

		//update the parent of the child
		BF_Block_Init(&block);
		CHECK_BF_ERROR(BF_GetBlock(fd, child_block, block));
		data = BF_Block_GetData(block);
		offs = sizeof(char);
		memcpy(data+offs, &parent_block, sizeof(int));

		DESTROY_BLOCK(block);
		return 1;

	}
	else { /* There is no space in the old block so we need to make a new index Block */
		//break
		CREATE_NEW_BLOCK(fd, new_block);
		new_data = BF_Block_GetData(new_block);

		//write the metadata in the new block
		offs2=0;

		//determine that it is an index block
		new_one_block.type = 'i';
		memcpy(new_data+offs2, &(new_one_block.type), sizeof(new_one_block.type));
		offs2 += sizeof(new_one_block.type);

		offs2 += sizeof(new_one_block.parent);

		//write how many entries there are in it
		if (maxPointerCount%2 == 0)
			old_block.blockFullness = old_block.blockFullness/2;
		else
			old_block.blockFullness = old_block.blockFullness/2 + 1;

		new_one_block.blockFullness = maxPointerCount - old_block.blockFullness;

		memcpy(new_data+offs2, &(new_one_block.blockFullness), sizeof(new_one_block.blockFullness));
		offs2 += sizeof(new_one_block.blockFullness);

		offs = sizeof(old_block.type) + sizeof(old_block.parent);
		memcpy(data+offs, &(old_block.blockFullness), sizeof(old_block.blockFullness));

		//copy from the first block
		offs = INDEX_HEADER_SIZE + old_block.blockFullness*sizeof(int) + old_block.blockFullness*attrLength1;
		memcpy(new_data + offs2, data + offs, new_one_block.blockFullness*sizeof(int) + (new_one_block.blockFullness - 1)*attrLength1);
		//get the middle key
		middle_key = malloc(attrLength1);
		offs -= attrLength1;
		memcpy(middle_key, data + offs, attrLength1);

		WRITE_GARBAGE(data, offs);

		offs = INDEX_HEADER_SIZE + new_one_block.blockFullness*sizeof(int) + (new_one_block.blockFullness - 1)*attrLength1;
		WRITE_GARBAGE(new_data, offs);

		BF_GetBlockCounter(fd, &new_block_ptr);
		new_block_ptr--;
		//if the key is less than the middle value it wil be inserted in the initial
		//block or else in the new one
		if (compare(key, LESS_THAN, middle_key, attrType1)) {

			CHECK_ERROR(Insert_Index(child_block, parent_block, openFilesIndex, key));

		} else {

			CHECK_ERROR(Insert_Index(child_block, new_block_ptr, openFilesIndex, key));

		}

		//update the parents of all the children
		Update_Kids_Parents(new_block_ptr, openFilesIndex);
		Update_Kids_Parents(parent_block, openFilesIndex);

		//if we broke the root so we need to make a new root
		if (parent_block == root) {
			CREATE_NEW_BLOCK(fd, root_block);
			root_data = BF_Block_GetData(root_block);

			root_offs = 0;
			//Determine that the root block is an index block
			root_block_hd.type = 'i';
			memcpy(root_data, &(root_block_hd.type), sizeof(root_block_hd.type));
			root_offs+= sizeof(root_block_hd.type);

			//write the father of the new root which is gonna be 0
			root_block_hd.parent = 0;
			memcpy(root_data + root_offs, &(root_block_hd.parent), sizeof(root_block_hd.parent));
			root_offs+= sizeof(root_block_hd.parent);

			//write the block fullnes of the root which is gonna be 2
			root_block_hd.blockFullness = 2;
			memcpy(root_data + root_offs, &(root_block_hd.blockFullness), sizeof(root_block_hd.blockFullness));
			root_offs+= sizeof(root_block_hd.blockFullness);

			//write the first pointer which will point to the current block we tried to insert the key at
			memcpy(root_data + root_offs, &parent_block, sizeof(parent_block));
			root_offs += sizeof(parent_block);
			//write the key which is gonna be the middle key that we push above
			memcpy(root_data + root_offs, middle_key, attrLength1);
			root_offs += attrLength1;
			//write the second pointer which will point to the new block
			memcpy(root_data + root_offs, &new_block_ptr, sizeof(new_block_ptr));
			root_offs += sizeof(new_block_ptr);

			WRITE_GARBAGE(root_data, root_offs);
			DESTROY_BLOCK(root_block);

			//update the parents of the 2 index blocks
			BF_GetBlockCounter(fd, &root);
			root--;

			old_block.parent = new_one_block.parent = root;
			offs = sizeof(char);
			memcpy(data + offs, &(old_block.parent), sizeof(old_block.parent));
			memcpy(new_data + offs, &(old_block.parent), sizeof(old_block.parent));


			//update the root info in the table and the file
			Update_Root(root, openFilesIndex);

			DESTROY_BLOCK(block);
			DESTROY_BLOCK(new_block);
			free(middle_key);
			return 1;
		} else {
			//insert the middle key in the block above
			offs = sizeof(old_block.type);
			memcpy(&(old_block.parent), data + offs, sizeof(old_block.parent));
			CHECK_ERROR(Insert_Index(new_block_ptr, old_block.parent, openFilesIndex, middle_key));
		}

		free(middle_key);
		DESTROY_BLOCK(block);
		DESTROY_BLOCK(new_block);
		return 1;
	}
	free(middle_key);
	DESTROY_BLOCK(block);
	DESTROY_BLOCK(new_block);
	return 1;
}

//updates the father pointer of all the childs of the respective parent
//used when a block is splitted to update the father pointers of all the children
int Update_Kids_Parents(int parent, int openFilesIndex) {

	BF_Block* parent_block, *child_block;
	char* parent_data, *child_data;
	int ptr, offs, blockFullness,i;
	int child_offs = sizeof(char);
	int fd = open_files[openFilesIndex]->fd;
	int attrLength1 = open_files[openFilesIndex]->attrLength1;

	BF_Block_Init(&child_block);
	BF_Block_Init(&parent_block);

	CHECK_BF_ERROR(BF_GetBlock(fd, parent, parent_block));
	parent_data = BF_Block_GetData(parent_block);

	offs = sizeof(char) + sizeof(int);
	memcpy(&blockFullness, parent_data + offs, sizeof(int));
	offs = INDEX_HEADER_SIZE;

	//for every child update its parent
	for (i = 0; i < blockFullness; i++) {
		//get the next child
		memcpy(&ptr, parent_data + offs, sizeof(int));
		offs += sizeof(int) + attrLength1;

		CHECK_BF_ERROR(BF_GetBlock(fd, ptr, child_block));
		child_data = BF_Block_GetData(child_block);
		memcpy(child_data + child_offs, &parent, sizeof(int));

		BF_Block_SetDirty(child_block);
		BF_UnpinBlock(child_block);
	}


	BF_UnpinBlock(parent_block);
	BF_Block_Destroy(&parent_block);
	BF_Block_Destroy(&child_block);
	return 1;
}

//updates the root value in the file and all the entries of the file in open_files array
int Update_Root(int new_root, int openFilesIndex) {
	int i, offs;
	BF_Block *base_block;
	char* base_block_data;

	//update the root in the first block
	BF_Block_Init(&base_block);
	CHECK_BF_ERROR(BF_GetBlock(open_files[openFilesIndex]->fd, 0, base_block));
	base_block_data = BF_Block_GetData(base_block);
	offs = strlen("AM_FILE") + 1 + 2*(sizeof(char) + sizeof(int));
	memcpy(base_block_data + offs, &new_root, sizeof(new_root));
	DESTROY_BLOCK(base_block);

	//update the root in all the positions on open_files array where our file is
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (open_files[i] != NULL) {
			if (strcmp(open_files[openFilesIndex]->fileName, open_files[i]->fileName) == 0) {
				open_files[i]->root = new_root;
			}
		}
	}

}
