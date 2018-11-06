#ifndef IF_H_
#define IF_H_

#include "AM.h"



//Inserts data when there are no blocks. It creates the root and teh first data block
int Base_Insert(int fileDesc, void* value1, void* value2);

//writes the data  in the block
void Write_Data(int* offs, char* data, void* value1, void* value2, int openFilesIndex);
void Write_Data2(int* offs, char* data, void* value1, void* value2,int attrLength1,int attrLength2,int openFilesIndex);

//Insertion data in any other case expect root. (potentially to use for root too. we'll see)
int Insert_Data(int block_num, int openFilesIndex, void* value1, void* value2);

//Inserts the pair key,pointer in our index block
int Insert_Index(int child_block, int parent_block, int openFilesIndex, void* key);

int Update_Kids_Parents(int parent, int openFilesIndex);
int Update_Root(int new_root, int openFilesIndex);

#endif /* IF_H_ */
