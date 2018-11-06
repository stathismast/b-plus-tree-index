#include <assert.h>
#include "AM.h"
#include <stdlib.h>
#include "structs.h"

extern file_info **open_files;
extern scan_info **open_scans;
extern entry entryToReturn;

/* Initialization of global array open_files */
void Open_Files_Init( void ){
	int i;

	open_files = malloc ( MAX_OPEN_FILES * sizeof(file_info *) );
	assert( open_files!= NULL );

	for( i = 0 ; i < MAX_OPEN_FILES ; i++ ){
		open_files[i] = NULL; 		/* NULL means that this position
									in the array is free, so we can use it */
	}
}

/* Initialization of global array open_scans */
void Open_Scans_Init( void ){
	int i;
	open_scans = malloc ( MAX_SCANS * sizeof(scan_info *) );
	assert( open_scans!= NULL );
	for( i = 0 ; i < MAX_SCANS ; i++ ){
		open_scans[i] = NULL; 		/* NULL means that this position in the
								array is free, so we can use it  */

	}
}

/* Destruction of global array open_files */
void Open_Files_Destroy( void ){
	int i;
	for( i = 0 ; i < MAX_OPEN_FILES ; i++ ){
		if( open_files[i] != NULL ){
			free( open_files[i] );
		}
	}
	free(open_files);
}

/* Destruction of global array open_scans */
void Open_Scans_Destroy( void ){
	int i;
	for( i = 0 ; i < MAX_SCANS ; i++ ){
		if( open_scans[i] != NULL ){
			if( open_scans[i]->value != NULL ){
				free( open_scans[i]->value );
			}
			free( open_scans[i] );
		}
	}
	free(open_scans);
}
