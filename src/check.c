
/* Checks if type is 'c'/string , 'i'/integer or 'f'/float and if the length
	is compatible with the given type */
int checkTypeLength( char type, int length ){
	switch( type ){
		case 'i':
		case 'f':
					if ( length != 4 ){
						return -1;       /* incompatible length */
					}
					break;
		case 'c':
 					if ( length < 1 || length > 255 ){
						return -1;       /* incompatible length */
					}
					break;
		default :               /* invalid type */
					return -1;
	}
	return 1;
}

/* Check if min <= number <= max */
int checkNumberLimits( int number, int min, int max ){

	if( (number >= min) && (number <= max) ){
		return	1;
	}
	else{
		return -1;
	}
	
}
