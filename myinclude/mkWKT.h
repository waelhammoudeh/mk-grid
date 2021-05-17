/*
 * mkWKT.h
 *
 *  Created on: Apr 21, 2021
 *      Author: wael
 */

#ifndef MYINCLUDE_MKWKT_H_
#define MYINCLUDE_MKWKT_H_

#include "dList.h"
#include "mk-grid.h"

#define MAX_SRC_LENGTH  65
#define MAX_ID_LENGTH  17

#define MIN_ROW_COL 2
#define MAX_ROW_COL 32
#define OUT_OF_RANGE(i) ( (i < MIN_ROW_COL) || (i > MAX_ROW_COL) )

typedef struct MK_WKT_RETURN_ {

	DL_LIST	*wktDL; // both are strings lists
	DL_LIST	*notFndDL; // not found list

}MK_WKT_RETURN;

typedef enum INFORMAT_ { // input file format

	UNKNOWN_FORMAT = 0,
	FOUR_CORNERS,
	ROWxCOL

}INFORMAT;


typedef struct RECT_DATA_ {

	char		srcFile[MAX_SRC_LENGTH];
	char		idRxC[MAX_ID_LENGTH];

} RECT_DATA;

typedef struct RECTANGLE_ {

	POINT		    nw, ne, se, sw;
	RECT_DATA	data;

}RECTANGLE;

typedef struct DIMENSION_ {

	int	row, col;

} DIMENSION;

typedef struct XRDS_MTRX_ {

	DIMENSION		dim;
	XROADS			**xrds;

}XRDS_MTRX;


typedef struct RECT_MTRX_ {

	DIMENSION		dim;
	RECTANGLE		**rect;

} RECT_MTRX;


int mkPlygnWKT (MK_WKT_RETURN *retDest, char *server, char *infile);

INFORMAT getFormat (char *str);

int mkWKT4corners (MK_WKT_RETURN *retDest, char *server, DL_LIST *infileDL);

int mkWKTrowXcol (MK_WKT_RETURN *retDest, char *server, DL_LIST *infileDL);

int formatRectWKT (char **dest, RECTANGLE *rect);

void printXrdsArray (XROADS **src);

int str2Dim (DIMENSION *dim, char *str);

int initialXrdsMtrx (XRDS_MTRX *mtrx, DIMENSION dim);

int initialRectMtrx (RECT_MTRX *mtrx, DIMENSION dim);

void fillRectangle (RECTANGLE *rect, POINT sw, POINT nw,
		                         POINT ne, POINT se, RECT_DATA *rectData);

int xrdsMtrx2RectMtrx (RECT_MTRX *rectMtrx, XRDS_MTRX *xrdsMtrx, char *srcFile);

void printPolygon (RECTANGLE *rect);

int namesFillXrdsMtrx (XRDS_MTRX *mtrx, DL_LIST *rowDL, DL_LIST *colDL);

int curlXrdsGPS4Mtrx (XRDS_MTRX *mtrx, BBOX *bbox, char *server,
		HTTP_METHOD method, int (*parseFunc) (XROADS *xrds, void *source) );


#endif /* MYINCLUDE_MKWKT_H_ */
