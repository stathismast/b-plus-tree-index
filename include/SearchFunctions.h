#ifndef SF_H_
#define SF_H_

#include "AM.h"

int GetEntry(int scanDesc);
int InitFirstEntry(int scanDesc);
int EntryCompare(int scanDesc);
int compare(void * left, int op, void *right, char type);
int getDataBlock(int openFilesIndex, int index, void * key);
int findFirstEntry(int fd, int blockNum, void * key);
int isDataBlock(char * data);
int findFirstKey(int openFilesIndex, int blockNum, void * key);


#endif /* SF_H_ */
