#ifndef AM_H_
#define AM_H_

#include "bf.h"
#include "check.h"
#include "defn.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "Insert_Functions.h"
#include "structs.h"
#include "SearchFunctions.h"
#include "InitDestroy.h"

/* Error codes */

extern int AM_errno;

typedef enum AM_errorcode {
	AME_OK = 1,
	AME_CREATION_FILE_ERROR = -1,	//an error occured during the creation process of a file
	AME_OPENING_FILE_ERROR = -2,		//an error occured during the opening process of a file
	AME_CLOSING_FILE_ERROR = -3,		//an error occured during the closing process of a file
	AME_NO_SUCH_FILE = -4,			//the file that was attempted to be processed does not exist
	AME_EOF = -5,					//no more entries that meet the requirements of open scan
	AME_OPEN_FILES_LIMIT = -6,		//there are MAX_OPEN_FILES open files
	AME_OPEN_SCNANS_LIMIT = -7,		//there are MAX_SCANS open scans
	AME_FILE_IS_USED = -8,			//the file that was tried to be deleted/closed is used
	AME_INSERTION_ERROR = -9,		//an error occured during the insertion of an entry
	AME_SCAN_ERROR = -10,				//an error occured while scanning the error
	AME_VALUE_OUT_LIMITS = -11,		//the value to be used is out of its respective limits for the place being used
	AME_WRONG_TYPE_LENGTH = -12,		//the attrLength is invalid with the respective type
	AME_CLOSE_SCAN = -13,				//an error occured while closing a scan
	AME_ERROR = -14
} AM_errorcode;

#define EQUAL 1
#define NOT_EQUAL 2
#define LESS_THAN 3
#define GREATER_THAN 4
#define LESS_THAN_OR_EQUAL 5
#define GREATER_THAN_OR_EQUAL 6

/* Max number of open files/scans at the same time */
#define MAX_OPEN_FILES 20
#define MAX_SCANS 20
typedef struct entry entry;

void AM_Init( void );

int AM_CreateIndex(
  char *fileName, /* όνομα αρχείου */
  char attrType1, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength1, /* μήκος πρώτου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
  char attrType2, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength2 /* μήκος δεύτερου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
);


int AM_DestroyIndex(
  char *fileName /* όνομα αρχείου */
);


int AM_OpenIndex (
  char *fileName /* όνομα αρχείου */
);


int AM_CloseIndex (
  int fileDesc /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
);


int AM_InsertEntry(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  void *value1, /* τιμή του πεδίου-κλειδιού προς εισαγωγή */
  void *value2 /* τιμή του δεύτερου πεδίου της εγγραφής προς εισαγωγή */
);


int AM_OpenIndexScan(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  int op, /* τελεστής σύγκρισης */
  void *value /* τιμή του πεδίου-κλειδιού προς σύγκριση */
);


void *AM_FindNextEntry(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);


int AM_CloseIndexScan(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);


void AM_PrintError(
  char *errString /* κείμενο για εκτύπωση */
);

void AM_Close();


#endif /* AM_H_ */
