#ifndef STRUCTS_H_
#define STRUCTS_H_

#define INDEX_HEADER_SIZE (sizeof(char) + sizeof(int) + sizeof(int))
#define DATA_HEADER_SIZE (sizeof(char) + sizeof(int) + sizeof(int) + sizeof(int))

/* Information about each file we open and we store it in a global array. */
typedef struct file_info{
	int fd;				/* File descriptor */
	char fileName[40];
	char attrType1;			/* type of first field : 'c' for string,
							'i' for integer,'f' for float */

	int attrLength1;			/* lenght of first field : 4 for 'i' or
							'f',1-255 for 'c' */

	char attrType2;			/* type of second field : 'c' for string,
								'i' for integer,'f' for float */

	int attrLength2;			/* lenght of second field : 4 for 'i' or 'f'
								,1-255 for 'c' */

	int root;				/* the block_num that we have the root of
								the B+ tree in it */

	int pointersPerBlock;		/* max number of pointers in each index block */
	int tuplesPerBlock;			/* max number of tuples ( key,value ) in each data block */
} file_info;

/* Information-Coordinates about last record we found with findNextEntry on an Open Scan */
typedef struct entryCoordinates{
	int blockNum;			/* The number of block in which the record is */
	int entryNum;			/* The "position" in the block in which the record is */
} entryCoordinates;

/* Information about each scan that we store in a global array. */
typedef struct scan_info{
	entryCoordinates lastEntry;	/* Last record we found */
	int openFilesIndex;		/* Position of file in the global OpenFiles array  */
	int operator;		/* Operator of the comparison */
	void *value;		/* Value to be compared */
} scan_info;

/* tuple (key,value) which can be found in an IndexBlock or in a DataBlock */
typedef struct entry{
	void *key;
	void *value;
} entry;

/* Header of a DataBlock */
typedef struct dataBlockHeader{
	char type;		/* 'd' for DataBlock / 'i' for IndexBlock */
	int parent;		/* Parent of current block */
	int blockFullness;	/* How many entries there are in this block */
	int nextDataBlock;	/* nextDataBlock number */
} dataBlockHeader;

/* Header of an IndexBlock */
typedef struct indexBlockHeader{
	char type;	/* 'd' for DataBlock / 'i' for IndexBlock */
	int parent;	/* Parent of current block */
	int blockFullness;	/* How many pointers there are in this block */
} indexBlockHeader;

#endif /* STRUCTS_H_ */
