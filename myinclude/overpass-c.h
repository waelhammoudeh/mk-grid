/*
 * overpass-c.h
 *
 *  Created on: Mar 10, 2021
 *      Author: wael
 */

#ifndef OVERPASS_C_H_
#define OVERPASS_C_H_

#include <stdio.h>
#include "curl_func.h"

extern FILE *rawDataFP;

// defined types
typedef struct POINT_ {

	double 	longitude,
					latitude;
	int			ns,
					ew;
} POINT;

typedef struct BBOX_ {

	   POINT		sw, ne;
} BBOX;

typedef struct HB_PT_ { // hundred block point

	int	ns, ew;
} HB_PT;

typedef struct GPS_PT_ {

	double 	longitude,
					latitude;
} GPS_PT;

/* XROADS structure used in getxrdsGPS() for input AND output  */
typedef struct XROADS_ {

	char				*firstRD, *secondRD;
	POINT			point;
	int				nodesFound;
	GPS_PT		*nodesGPS[]; // this limits structure future modification

} XROADS;

/* macros to validate longitude and latitude values in the
 * Phoenix, Arizona area - includes most of Maricopa county.
 *          *****xxxxx******xxxx******
 * change those values, or comment out the tests. */
#define LONGITUDE_OK(i) (((i) > -113.0 && (i) < -111.0))
#define LATITUDE_OK(i) (((i) > 32.8 && (i) < 33.95))

// functions prototypes
int getXrdsGPS (XROADS *xrds, BBOX *bbox, char *server,
		                     char *outfile, int (*parseFunc) (XROADS *xrds, void *source));

char* xrdsFillTemplate (XROADS *xrds, BBOX *bbox);

int curlGetXrdsGPS(XROADS *xrds, BBOX *bbox, char *server,
		HTTP_METHOD method, int (*parseFunc) (XROADS *xrds, void *source));

int isOkResponse (char *response, char *header);

int namesFillTemplate(char **dst, BBOX *bbox);

int getStreetNames (BBOX *bbox, char *server, char *dstFile);

int isBbox(BBOX *bbox);

int cloneXrds (XROADS **dest, XROADS *src);

int initialXrds (XROADS **xrds, int numGPS);


#endif /* OVERPASS_C_H_ */
