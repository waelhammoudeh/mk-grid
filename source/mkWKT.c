#include <stdio.h>
#include <stdlib.h>

#include "mkWKT.h"
#include "mk-grid.h" // for prog_name
#include "fileio.h"
#include "util.h"
#include "overpass-c.h"
#include "op_string.h"

// minimum lines required in input file
#define LINES_REQUIRED	6

//int mkPlygnWKT (MK_WKT_RETURN *retDest, char *server, char *infile, int verbose){
int mkPlygnWKT (MK_WKT_RETURN *retDest, char *server, char *infile){

	DL_LIST		*infileDL;
	DL_ELEM		*elem;
	LINE_INFO	*lineInfo;
	char				*srcLine;
	int				result;

	INFORMAT	informat;

	ASSERTARGS (retDest && server && infile);

	// prepare list to read input file into list -- allocate mem & initial list
	infileDL = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (infileDL == NULL){
		printf("%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}
	initialDL (infileDL, zapLineInfo, NULL); // no return value to check ...

	/* file2List() fills the list with lines from text file,
	 * where data pointer is a pointer to LINE_INFO structure,
	   ignoring lines starting with # or ; */
	result = file2List(infileDL, infile);
	if (result != ztSuccess){
		printf("%s: Error returned by file2List()!\n", prog_name);
		printf (" The error from file2List() was: %s ... Exiting.\n",
				    code2Msg(result));
		return result;
	}

	/* input file should have at least 6 lines:
	 * (dim + bbox + 2 rows + 2 columns)	OR (dim + bbox + 4 cross roads) */
	if (DL_SIZE(infileDL) < LINES_REQUIRED){

		printf("%s: Error seems incomplete input file [ %s ],\n number of lines found was < %d >.\n",
				   prog_name, infile, DL_SIZE(infileDL));
		printf (" Input file should have at least [ %d ] lines:\n"
				   " Dimension line + Bounding box line + 2 rows + 2 columns. OR\n"
				   " Dimension line + Bounding box line + 4 cross roads pairs.\n"
				   " Please see the text file format. 'Try: %s --help'\n",
				   LINES_REQUIRED, prog_name);

		return ztMissFormatFile;
	}

//printf ("mkPlygnWKT(): %s has [ %d ] lines\n\n", infile, DL_SIZE(infileDL));

	/* determine input file format from first line */
	elem = DL_HEAD(infileDL);
	lineInfo = DL_DATA(elem);
	srcLine = lineInfo->string;

	ASSERTARGS (srcLine);

	informat = getFormat (srcLine);

	if (informat == UNKNOWN_FORMAT){

		printf ("mkPlygnWKT(): Error; could not determine file FORMAT for file < %s >\n"
				" with first line has:\n"
				" <%s>\n"
				"disallowed character or invalid format! Valid formats are:\n"
				"nn          : four corners file; one integer specifying the number of polygons.\n"
				"rr X cc     : row X col file; where rr is number of rows, cc is number of columns\n"
				"              and X is ONE of X, x or *\n\n", infile, srcLine);
		return ztMissFormatFile;
	}

printf ("mkPlygnWKT(): FORMAT found to be: [ %s ]\n",
		     (informat == FOUR_CORNERS) ? "FOUR_CORNERS" : "ROWxCOL");

	if (informat == FOUR_CORNERS)

		mkWKT4corners (retDest, server, infileDL);

	else

		mkWKTrowXcol (retDest, server, infileDL);



	return ztSuccess;

}// END mkPlygnWKT()

/* getFormat(): str points to a line with either one integer by itself or
 * 2 integers with one character of 'Xx*' in between; examples:
 * 27
 *    27
 * 5 X 4
 *   5 	x 4
 * 6 X* 4 : error
 * 4 x : error
 * 5x : error
 * X5 : error
 * *****/
INFORMAT getFormat (char *str) {

	char		*anyCross = "Xx*";
	char		*allowed = "0123456789Xx*\040\t"; // allow space and tab
	char		*spaceSet = "\040\t";
	char		*digits = "0123456789";

	char		*myStr;
	char		*firstToken, *secondToken, *thirdToken, *tokenPtr;
	int		count;

	ASSERTARGS (str);

	// check for disallowed characters
	if (strspn(str, allowed) != strlen(str)) // str has disallowed char

		return UNKNOWN_FORMAT;

	myStr = strdup (str);

	firstToken = secondToken = thirdToken = NULL;

	// get at most 3 tokens
	for (count = 1; (tokenPtr = strtok (myStr, spaceSet)); myStr = NULL, count++){

		if (count == 1) 	firstToken = strdup (tokenPtr);

		else if (count == 2)	secondToken = strdup (tokenPtr);

		else if (count == 3)	thirdToken = strdup (tokenPtr);

		else

			break;
	}

	// tokenPtr pointing at something -- more tokens than needed
	 if ( (count > 3) && tokenPtr)

		return UNKNOWN_FORMAT;

	if (secondToken) {

		if ( (strlen(secondToken) > 1) ||
			 (strspn(secondToken, anyCross) != strlen(secondToken)) )

			return UNKNOWN_FORMAT;
	}

	if ( strspn(firstToken, digits) != strlen(firstToken) )

		return UNKNOWN_FORMAT;


	if (thirdToken && strspn(thirdToken, digits) != strlen(thirdToken) )

		return UNKNOWN_FORMAT;

	if (! secondToken)

		return FOUR_CORNERS;

	else

		return ROWxCOL;

} // END getFormat()

/*
 * expected source file format for four corners rectangles:
 * 3
 * # format for BBOX: (sw, ne); point: (latitude, longitude)
 *   33.45040, -112.07449, 33.51019, -111.99489
 *
 * # 3 boxes with 4 corners each in order clockwise or counter-clockwise
 * # no checking is done for the order, it is user responsibility to get right.
 * north 12th street, east Roosevelt street
 * north 12th street, east oak street
 * North 20th street, east oak street
 * north 20th street, east roosevelt street
 *
 * north 12th street, east oak street
 * North 12Th Street, East Osborn Road
 * North 20Th Street, East Osborn Road
 * North 20Th Street, East Oak Street
 *
 * North 20Th Street, East Roosevelt Street
 * North 20Th Street, East Osborn Road
 * North 36Th Street, East Osborn Road
 * North 36Th Street, East Roosevelt Street
 *
 */
int mkWKT4corners (MK_WKT_RETURN *retDest, char *server, DL_LIST *infileDL){

	int				numBoxes;
	int				result;

	DL_ELEM		*elem;
	LINE_INFO	*lineInfo;
	char				*srcLine;
	char				*tempBuf;

	BBOX			bbox;

	XROADS		**xrdsArray;
	XROADS		**xrdsMover;
	int				numXrds;

	RECTANGLE		**rectArray;
	RECTANGLE		**rectMover;
	POINT 				sw, nw, ne, se;

	ASSERTARGS (retDest && server && infileDL);

	// get first line to get number of boxes
	elem = DL_HEAD(infileDL);
	lineInfo = (LINE_INFO *) DL_DATA(elem);
	srcLine = lineInfo->string;

	sscanf (srcLine, "%d", &numBoxes);

	if (OUT_OF_RANGE(numBoxes)){

		printf ("mkWKT4corners(): Error; specified number of boxes <%d> "
				"is out of range.\n", numBoxes);
		return ztOutOfRangePara;
	}

	/* do we have enough cross roads for numBoxes? 4 corners / box
	 * plus one for numBoxes + one for bbox 	**/
	if (DL_SIZE(infileDL) != (numBoxes * 4) + 2){

		printf ("mkWKT4corners(): Error; miss format file to draw < %d > rectangles.\n"
				"Number of lines in input file found was: < %d >\n"
				"We only draw rectangles with four corners of cross roads.\n\n",
					numBoxes, DL_SIZE(infileDL));
		return ztMissFormatFile;
	}

	// get bbox and parse it
	elem = DL_NEXT(elem);
	lineInfo = (LINE_INFO *) elem->data;
	srcLine = lineInfo->string;

	result = parseBbox (&bbox, srcLine);
	if (result != ztSuccess){

		printf("mkWKT4corners(): Error returned from parseBbox() function.\n");
		return result;
	}

	/* XROADS structure: key structure
	 * The input file - as seen above - has firstRD and secondRD members.
	 * parse those and place each pair names in a XROADS structure.
	 *
	 * We initial an array of pointers to XROADS structures, allocate memory
	 * for each pointer - pack pointers to structures in an array.
	 **********************************************************************/

	numXrds = DL_SIZE(infileDL) - 2;

	xrdsArray = (XROADS **) initialP2PArray (numXrds, sizeof(XROADS));
	if ( ! xrdsArray){
		printf ("mkWKT4corners(): Error; memory allocation.\n");
		return ztMemoryAllocate;
	}

	xrdsMover = xrdsArray;

	// point at next line after the bounding box line
	elem = DL_NEXT(elem);
	while (elem) {

		lineInfo = (LINE_INFO*) elem->data;
		srcLine= strdup(lineInfo->string);

		// parse source line and store road names in firstRD and secondRD members
		result = xrdsParseNames( *xrdsMover, srcLine);
		if (result != ztSuccess) {
			printf("mkWKT4corners(): Error parsing cross roads line # %d "
						"from function xrdsParseNames().\n",
						lineInfo->originalNum);
			return result;
		}

		xrdsMover++;
		elem = DL_NEXT(elem);
	}

	// print the just filled structures - has names ONLY so far
	if (verbose){
		printf("mkWKT4corners(): parsed names from infileDL and "
				"filled firstRD and secondRD members:\n");
		printXrdsArray (xrdsArray);
	}

	// initial curl session to query overpass server for cross roads GPS
	result = curlInitialSession();
	if (result != ztSuccess){
		printf ("mkWKT4corners(): Error returned by curlInitialSession().\n");
		return result;
	}

	xrdsMover = xrdsArray;
	while (*xrdsMover){

		// query overpass for GPS of cross roads - parseCurlXrdsData() fills XROADS
		result = curlGetXrdsGPS(*xrdsMover, &bbox, server, Get, parseCurlXrdsData);
		if (result != ztSuccess){
			printf ("mkWKT4corners(): Error returned by curlGetXrdsGPS().\n");
			return result;
		}
		xrdsMover++;
	}

	curlCloseSession();

	// print structures with names AND GPS filled
	if (verbose) {

		printf("mkWKT4corners(): Parsed query results, filled GPS members as follows:\n");
		printXrdsArray (xrdsArray);
	}
	printf("\n");

	// check for not found GPS, insert a character string in notFndDL for return
	xrdsMover = xrdsArray;
	while (*xrdsMover){

		if ( (*xrdsMover)->nodesFound == 0){

			tempBuf = (char *) malloc (sizeof(char) * LONG_LINE + 1 );
			if (! tempBuf){
				printf ("mkWKT4corners(): Error allocating memory.\n");
				return ztMemoryAllocate;
			}
			sprintf (tempBuf, "%s, %s", (*xrdsMover)->firstRD, (*xrdsMover)->secondRD);

			/* insertNextDL() does NOT allocate memory for your data -- does not know
			 * its size - it just sets data pointer in ELEM to your pointer, it all on you ***/
			insertNextDL (retDest->notFndDL, DL_TAIL(retDest->notFndDL), tempBuf);
		}
		xrdsMover++;
	}

	// if the list is not empty print it
	if (DL_SIZE(retDest->notFndDL)){
		printf ("mkWKT4corners(): Could not get GPS for the following Cross roads:\n");
		printStringDL (retDest->notFndDL);
	}
	printf("\n");

	// allocate and initial rectArray
	rectArray = (RECTANGLE **) initialP2PArray (numBoxes, sizeof(RECTANGLE));
	if ( ! rectArray){
			printf ("mkWKT4corners(): Error; memory allocation.\n");
			return ztMemoryAllocate;
	}

	rectMover = rectArray;
	xrdsMover = xrdsArray;

	// fill one rectangle with four points
	while (*rectMover){

		sw = (*xrdsMover)->point;
		xrdsMover++;

		nw =  (*xrdsMover)->point;
		xrdsMover++;

		ne =  (*xrdsMover)->point;
		xrdsMover++;

		se = (*xrdsMover)->point;
		xrdsMover++;

		// fillRectangle() does not return value, no result check.
		fillRectangle (*rectMover, sw, nw, ne, se, NULL);

		rectMover++;
	}

	/* format rectangle points in string buffer and insert buffer into return list */
	rectMover = rectArray;
	while (*rectMover){

		result = formatRectWKT (&tempBuf, *rectMover);
		if (result != ztSuccess){

			printf("mkWKT4corners(): Error returned from formatRectWKT(). Out of memory?\n");
			return result;
		}

		result = insertNextDL (retDest->wktDL, DL_TAIL(retDest->wktDL), tempBuf);
		if (result != ztSuccess){
			printf("mkWKT4corners(): Error returned from insertNextDL().\n");
			return result;
		}
		rectMover++;
	}

	if (verbose){
		printf("mkWKT4corners(): Done making Well Known Text format:\n");
		printStringDL(retDest->wktDL);
	}
	printf("\n");

	return ztSuccess;

} // End mkWKT4corners()

int formatRectWKT (char **dest, RECTANGLE *rect){

	char		buffer[LONG_LINE] = {0};
	int		sizeNeeded;

	ASSERTARGS (dest);

	sprintf (buffer, "\"POLYGON ((%11.7f %10.7f, %11.7f %10.7f, %11.7f %10.7f, %11.7f %10.7f, %11.7f %10.7f))\"",
				     rect->sw.longitude, rect->sw.latitude,
					 rect->nw.longitude, rect->nw.latitude,
					 rect->ne.longitude, rect->ne.latitude,
					 rect->se.longitude, rect->se.latitude,
					 rect->sw.longitude, rect->sw.latitude);

	sizeNeeded = strlen(buffer) * sizeof(char) + 1;

	*dest = (char *) malloc (sizeof(char) * sizeNeeded);
	if ( *dest == NULL) {
		printf("formatRectWKT(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}

	strcpy(*dest, buffer);

	return ztSuccess;
}

int mkWKTrowXcol (MK_WKT_RETURN *retDest, char *server, DL_LIST *infileDL){

	DL_ELEM		*elem;
	LINE_INFO	*lineInfo;
	DIMENSION	dim;
	BBOX			bbox;
	DL_LIST		rowDL, colDL;

	char		*srcLine;
	int		result;
	int		count;

	XRDS_MTRX		*xrdsMtrx; // one pointer to one matrix / array
	RECT_MTRX		*rectMtrx;
	DIMENSION		rctMtxDim;
	char					*srcFile = ""; // name ONLY of source file - no path or extension
	RECTANGLE		**rectMover;
	char					*tempBuf;
	XROADS			**xrdsMover;

	ASSERTARGS (retDest && server && infileDL);

	// get first line to get the dimension rr X cc
	elem = DL_HEAD(infileDL);
	lineInfo = (LINE_INFO *) DL_DATA(elem);
	srcLine = lineInfo->string;

	result = str2Dim (&dim, srcLine);
	if (result != ztSuccess){
		printf("mkWKTroxXcol(): Error getting the dimension form input file.\n");
		return result;
	}

	/* now with set dimension - do numbers add up */
	if (DL_SIZE(infileDL) != (dim.row + dim.col) + 2){

		printf ("mkWKTrowXcol(): Error; miss format file, dimension has < %s >\n"
				"Number of lines in input file found was: < %d >\n",
				srcLine, DL_SIZE(infileDL));
		return ztMissFormatFile;
	}

	elem = DL_NEXT(elem); // next line is the bounding box
	lineInfo = (LINE_INFO *) elem->data;

	// get our own copy, since it gets mangled by strtok()
	srcLine = strdup (lineInfo->string);
	result = parseBbox (&bbox, srcLine);
	if (result != ztSuccess){

		printf("mkWKTrowXcol(): Error parsing BBOX!\n\n");
		printf("Expected bounding box format:\n"
					"	swLatitude, swLongitude, neLatitude, neLongitude\n\n");
		return ztParseError;
	}

	// fill row and column lists from infileDL
	initialDL (&rowDL, NULL, NULL);
	initialDL (&colDL, NULL,NULL);

	elem = DL_NEXT(elem); // next line of infileDL
	count = dim.row;

	while (count){

		lineInfo = (LINE_INFO *) elem->data;
		srcLine = (char *) lineInfo->string;

		// lines from input file may contain leading or trailing space(s)
		removeSpaces (&srcLine);

		result = insertNextDL (&rowDL, DL_TAIL(&rowDL), (void *) srcLine);
		if (result != ztSuccess){
			printf ("mkWKTrowXcol(): Error returned by insertNextDL().\n");
			return result;
		}
		elem = DL_NEXT(elem);
		count--;
	}

	count = dim.col;

	while (count){

		lineInfo = (LINE_INFO *) elem->data;
		srcLine = (char *) lineInfo->string;

		result = insertNextDL (&colDL, DL_TAIL(&colDL), (void *) srcLine);
		if (result != ztSuccess){
			printf ("mkWKTrowXcol(): Error returned by insertNextDL().\n");
			return result;
		}
		elem = DL_NEXT(elem);
		count--;
	}

	// initial 2 dimensional array (MATRIX) to fill with XROADS structures
	xrdsMtrx = (XRDS_MTRX *) malloc (sizeof(XRDS_MTRX));
	if ( ! xrdsMtrx ){
		printf ("mkWKTrowXcol(): Error allocating memory in main().\n");
		return ztMemoryAllocate;
	}

	/* initialXrdsMtrx() allocates memory for ALL pointers in the array + one
	 * sets last pointer to NULL, and fills dim members. ***/
	result = initialXrdsMtrx (xrdsMtrx, dim);
	if (result != ztSuccess){
		printf ("mkWKTrowXcol(): Error returned from initialXrdsMtrx().\n");
		return result;
	}

	/* when making source file: we move up and to the right for street names -
	 * otherwise pointers will be wrong */
	result = namesFillXrdsMtrx (xrdsMtrx, &rowDL, &colDL); // data ptr 2 STRING
	if (result != ztSuccess){
		printf ("mkWKTrowXcol(): Error returned from namesFillXrdsMtrx().\n");
		return result;
	}

	result = curlXrdsGPS4Mtrx (xrdsMtrx, &bbox, server, Get, parseCurlXrdsData);
	if (result != ztSuccess){
		printf ("mkWKTrowXcol(): Error returned from curlXrdsGPS4Mtrx().\n");
		return result;
	}

	if (verbose){
		printf("mkWKTrowXcol(): Cross roads array with query result parsed:\n");
		printXrdsArray (xrdsMtrx->xrds);
	}

	// check for not found GPS
	xrdsMover = xrdsMtrx->xrds;
	while (*xrdsMover){

		if ( (*xrdsMover)->nodesFound == 0){

			tempBuf = (char *) malloc (sizeof(char) * LONG_LINE + 1 );
			if (! tempBuf){
				printf ("mkWKTrowXcol(): Error allocating memory.\n");
				return ztMemoryAllocate;
			}
			sprintf (tempBuf, "%s, %s", (*xrdsMover)->firstRD, (*xrdsMover)->secondRD);

			/* insertNextDL() does NOT allocate memory for your data -- does not know
			 * its size - it just sets data pointer in ELEM to your pointer, it all on you ***/
			insertNextDL (retDest->notFndDL, DL_TAIL(retDest->notFndDL), tempBuf);
		}
		xrdsMover++;
	}


	// set the dimension for rectMtrx - one less than intersection rows and cols
	rctMtxDim.row = dim.row - 1;
	rctMtxDim.col = dim.col - 1;

	rectMtrx = (RECT_MTRX *) malloc (sizeof(RECT_MTRX));
	if ( ! rectMtrx ){
		printf ("mkWKTrowXcol(): Error allocating memory for rectMtrx.\n");
		return ztMemoryAllocate;
	}

	result = initialRectMtrx (rectMtrx, rctMtxDim);
	if ( ! rectMtrx ){
			printf ("mkWKTrowXcol(): Error returned from initialRectMtrx()\n");
			return ztMemoryAllocate;
	}

	// make rectangle array from cross roads array
	xrdsMtrx2RectMtrx (rectMtrx, xrdsMtrx, srcFile);

	/* format rectangle points in string buffer and insert buffer into return list */
	rectMover = rectMtrx->rect;
	while (*rectMover){

		result = formatRectWKT (&tempBuf, *rectMover);
		if (result != ztSuccess){

			printf("mkWKT4corners(): Error returned from formatRectWKT(). Out of memory?\n");
			return result;
		}

		result = insertNextDL (retDest->wktDL, DL_TAIL(retDest->wktDL), tempBuf);
		if (result != ztSuccess){
			printf("mkWKT4corners(): Error returned from insertNextDL().\n");
			return result;
		}
		rectMover++;
	}


	return ztSuccess;

} // END mkWKTroxXcol()

void printXrdsArray (XROADS **src){

	XROADS	**mover;

	ASSERTARGS (src);

	mover = src;
	while (*mover){

		printXrds (*mover); // in file: op_string.c
		mover++;
	}

	return;
}

int str2Dim (DIMENSION *dim, char *str){

	int	row,
			column,
			result;

	ASSERTARGS (dim && str);

	//result = sscanf (str, "%d %*[xX*] %d", &row, &column); // read and ignore set
	//result = sscanf (str, "%d %*c %d", &row, &column); // read and ignore character
	result = sscanf (str, "%d %*s %d", &row, &column);

	/* ALL statements above are not perfect. USE strtok().
	 * The "*c%" for scanf() : means read a character and throw it a way  */

	if (result != 2){

		printf ("str2Dim(): Error invalid format for dimension. Expecting [r X c], found < %s >\n", str);
		return ztMissFormatFile;
	}

	dim->row = row;
	dim->col = column;

	return ztSuccess;

}

int initialXrdsMtrx (XRDS_MTRX *mtrx, DIMENSION dim){

	ASSERTARGS (mtrx);

	if ((OUT_OF_RANGE(dim.row)) || (OUT_OF_RANGE(dim.col))){
		printf ("initialXrdsMtrx((): Error OUT OF RANGE row or column.\n");
		return ztOutOfRangePara;
	}

	//void** allocate2Dim (int row, int col, size_t elemSize)
	mtrx->xrds = (XROADS **) allocate2Dim (dim.row, dim.col, sizeof(XROADS));
	if ( ! mtrx->xrds){
		printf("initialXrdsMtrx(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}
	/* set dim members */
	mtrx->dim.row = dim.row;
	mtrx->dim.col = dim.col;

	return ztSuccess;

}  /* END initialXrdsMtrx() */

int initialRectMtrx (RECT_MTRX *mtrx, DIMENSION dim){

	ASSERTARGS (mtrx);

	if ((OUT_OF_RANGE(dim.row)) || (OUT_OF_RANGE(dim.col))){
		printf ("initialRectMtrx((): Error OUT OF RANGE row or column.\n");
		return ztOutOfRangePara;
	}

	mtrx->rect = (RECTANGLE **) allocate2Dim (dim.row, dim.col, sizeof(RECTANGLE));
	if ( ! mtrx->rect){
		printf("initialRectMtrx(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}

	/* set dim members */
	mtrx->dim.row = dim.row;
	mtrx->dim.col = dim.col;

	return ztSuccess;

}  /* END initialRectMtrx() */

/* fillRectangle() just fills members values with rectData being optional
 * caller allocates memory for destination rect..
 ******************************************************************/
void fillRectangle (RECTANGLE *rect, POINT sw, POINT nw,
		                         POINT ne, POINT se, RECT_DATA *rectData){

	ASSERTARGS (rect);

	memset (rect, 0, sizeof(RECTANGLE));

	rect->sw = sw;
	rect->nw = nw;
	rect->ne = ne;
	rect->se = se;

	if (rectData){

		strncpy (rect->data.srcFile, rectData->srcFile, MAX_SRC_LENGTH - 1);
		strncpy (rect->data.idRxC, rectData->idRxC, MAX_ID_LENGTH -1);

	}

	return;
}

/* srcFile arg is to id rectangle */
int xrdsMtrx2RectMtrx (RECT_MTRX *rectMtrx, XRDS_MTRX *xrdsMtrx, char *srcFile){

	XROADS			**lowerXrds,
							**upperXrds;
	RECTANGLE		**rectMover;
	int					iRow, jCol;

	// ASSERTARGS (rectMtrx && xrdsMtrx && srcFile); - srcFile is to ID the matrix

	ASSERTARGS (rectMtrx && xrdsMtrx);

	rectMover = rectMtrx->rect;

	for (iRow = 0; iRow < rectMtrx->dim.row; iRow++){

		lowerXrds = xrdsMtrx->xrds + (iRow * xrdsMtrx->dim.col);
		upperXrds =  lowerXrds +  xrdsMtrx->dim.col;

		for (jCol = 0; jCol < rectMtrx->dim.col; jCol++){

			fillRectangle (*rectMover,
					(*lowerXrds)->point, (*upperXrds)->point,
					(*(upperXrds +1))->point,  (*(lowerXrds +1))->point, NULL);

//printPolygon (*rectMover);

			rectMover++;
			lowerXrds++;
			upperXrds++;

		}
	}

	return ztSuccess;
}
//"POLYGON ((-112.0738334 33.5455559, -112.0651409 33.5455279, -112.0650791 33.5382827, -112.0737604 33.5383198, -112.0738334 33.5455559))"
void printPolygon (RECTANGLE *rect){

	char		buffer[240] = {0};

	ASSERTARGS (rect);

	sprintf (buffer, "\"POLYGON ((%11.7f %10.7f, %11.7f %10.7f, %11.7f %10.7f, %11.7f %10.7f, %11.7f %10.7f))\"",
			     rect->sw.longitude, rect->sw.latitude,
				 rect->nw.longitude, rect->nw.latitude,
				 rect->ne.longitude, rect->ne.latitude,
				 rect->se.longitude, rect->se.latitude,
				 rect->sw.longitude, rect->sw.latitude);

	printf ("%s\n", buffer);
	return;

}


int namesFillXrdsMtrx (XRDS_MTRX *mtrx, DL_LIST *rowDL, DL_LIST *colDL){

	XROADS	**mover;
	int 			iRow, jCol;
	char			*ewRd, *nsRd;
	DL_ELEM	*ewElem, *nsElem;

	ASSERTARGS (mtrx && rowDL && colDL);

	mover = mtrx->xrds;
	ewElem = DL_HEAD(rowDL);

	for (iRow = 0; iRow < mtrx->dim.row; iRow++) {

		ewRd = (char *) DL_DATA(ewElem);
		nsElem = DL_HEAD(colDL);

		for (jCol = 0; jCol < mtrx->dim.col; jCol++){

			nsRd = (char *) DL_DATA(nsElem);

			(*mover)->firstRD = strdup (nsRd); // we need to allocate memory
			(*mover)->secondRD = strdup (ewRd);
			if ( ( (*mover)->firstRD == NULL) || ( (*mover)->firstRD == NULL) ){
				printf("namesFillXrdsMtrx(): Error allocating memory.\n");
				return ztMemoryAllocate;
			}

			mover++;
			nsElem = DL_NEXT(nsElem);

		}

		ewElem = DL_NEXT(ewElem);

	}

	return ztSuccess;
}

/* curlXrdsGPS4Mtrx() : call curlGetXrdsGPS() for each array element */
int curlXrdsGPS4Mtrx (XRDS_MTRX *mtrx, BBOX *bbox, char *server,
		HTTP_METHOD method, int (*parseFunc) (XROADS *xrds, void *source) ){

	XROADS		**mover;
	int				result;

	// initial curl session
	result = curlInitialSession();
	if (result != ztSuccess){
		printf ("curlXrdsGPS4Mtrx(): Error returned by curlInitialSession().\n");
		return result;
	}

	mover = mtrx->xrds;

	while (*mover){

		result = curlGetXrdsGPS(*mover, bbox, server, method , parseFunc);
		if (result != ztSuccess){
			printf ("curlXrdsGPS4Mtrx(): Error returned by curlGetXrdsGPS().\n");
			return result;
		}
		mover++;
	}

	curlCloseSession();

	return ztSuccess;
}
