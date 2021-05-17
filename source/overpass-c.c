/*
 * overpass-c.c
 *
 *  Created on: Mar 10, 2021
 *      Author: wael
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "overpass-c.h"
#include "util.h"
#include "ztError.h"
#include "curl_func.h"
#include "op_string.h"
#include "fileio.h"

/* Note: with (extern FILE *rawDataFP;) declaration in overpass.h
 * user can declare as global variable, and open a file - received data will
 * be written to that file. Declaration is in the overpass.h header file with:
 * extern FILE *rawDataFP;
 * client declaration should global with: (see mk-grid.c file)
 * FILE *rawDataFP;
 * and code in in curlGetXrdsGPS() function below. ***/

 /* getXrdsGPS(): Function retrieves the GPS for the FIRST common node - if
 * 		found - between two named roads from an OSM overpass server.
 *
 *  Parameters:
 *  	xrds: a pointer to XROADS structure; used for input and output;
 *  	caller / client sets firstRD and secondRD members on calling,
 *  	function fills nodesFound, longitude and latitude on exit.
 *
 *  	bbox: a pointer to BBOX structure (bounding box), a rectangle that
 *  	defines the area to search.
 *
 *  	server: a character pointer with overpass server name with "http://" suffix.
 *
 *  	outfile: a character pointer with file name to save output from the call to
 *  	wget command; outfile is passed to wget as output file name option.
 *
 *  Return: Function returns ztSuccess if successful.
 *      Function may return one of checked run time errors:
 *      ztChildProcessFailed, ztMemoryAllocate, file I/O errors, or returned parse
 *      errors when calling str2Xrds() function.
 *      Function sets XROADS members: longitude, latitude and nodesFound which
 *      is the number of nodes returned by the overpass server. Client should check
 *      the return value before looking at nodesFound.
 *
 *  How It Works:
 *		Function constructs an overpass query to find the common node between
 * 	  	2 roads. Function uses system "wget" command to send the query to the
 * 	 	overpass server using "http" protocol. The function sets wget output
 *  	option with file name passed in outfile parameter. Function waits for
 *  	"wget" command to finish, then it opens the output file and reads to
 *  	parses result from server.
 *  	Function sets nodesFound member of XROADS parameter pointer.
 *  	Function then calls str2Xrds() function if and only if a common node
 *  	was found to set longitude and latitude XROADS pointer members.
 *
 *  Note:
 *      We do not check street / road names for anything! Caller should make
 *      sure at least named roads exist within the bounding box.
 *  TODO: Make "bbox" as static variable, use previous box until user changes it.
 *
 ******************************************************************************/

int getXrdsGPS(XROADS *xrds, BBOX *bbox, char *server,
		                    char *outfile, int (*parseFunc) (XROADS *xrds, void *source)){

	char				*templateBuf = NULL;
	char				*wgetExec = "/usr/bin/wget",
						option[LONG_LINE] = {0},
						statement[LONG_LINE] = {0},
						*argList[4];

	int 				result;

	// do not allow nulls
	ASSERTARGS(xrds && bbox && server && outfile);

	// the two roads members should be set in the struct
	ASSERTARGS(xrds->firstRD && xrds->secondRD);

	/* set output option with outfile name; no space between
	   -O and file name, else wget will not work!! **/
	sprintf(option, "-O%s", outfile);

	templateBuf = xrdsFillTemplate (xrds, bbox);
	if (! templateBuf){
		printf("getXrdsGPS(): Error, failed xrdsFillTemplate() call.");
		return ztGotNull;
	}

	// lazy way to avoid compiler warning!
	ASSERTARGS(snprintf(statement, LONG_LINE, "%s?data=%s",
				              server, templateBuf) < LONG_LINE);

	argList[0] = wgetExec;
	argList[1] = &(option[0]);
	argList[2] = &(statement[0]);
	argList[3] = NULL;

	result = spawnWait(wgetExec, argList);
	if (result != ztSuccess){
		printf("getXrdsGPS(): wget child process exited abnormally\n");
		return ztChildProcessFailed;
	}

	/* NOTE: overpass server may return more than one common node between
	 * roads, however we only parse the GPS for the first returned node.
	 *************************************************************************/
	result = parseFunc (xrds, outfile);
	if (result != ztSuccess)

		printf("getXrdsGPS(): Error returned from parseFunc()!\n\n");

	return result;

} //End getXrdsGPS()

/* xrdsFillTemplate(): fills query template given firstRD + secondRD && bbox
 * Allocates required memory for the string - this is the query part of URL.
 * Returns char* for the query string or NULL on error.
************************************************************************ */

char *xrdsFillTemplate (XROADS *xrds, BBOX *bbox){

	char			*queryTemplate =
						"[out:csv(::lat,::lon,::count)]"
						"[bbox:%10.7f,%10.7f,%10.7f,%10.7f];"
						"(way['highway'!='service']['name'~'%s', i];>;)->.outNS;"
						"(way['highway'!='service']['name'~'%s', i];>;)->.outEW;"
						"node.outNS.outEW;"
						//"out; out count;&";
						"out; out count;";   // do no include the '&' at the end
						// used single NOT double quotes above

	char			tmpBuf[LONG_LINE * 2] = {0}; // large buffer
	char			*retValue = NULL;
	char			*cleanFirstRD, *cleanSecondRD;
	int			result;

	ASSERTARGS (xrds && bbox);

	// the two roads members should be set in the struct
	ASSERTARGS(xrds->firstRD && xrds->secondRD);

	// remove leading and trailing white space
	cleanFirstRD = strdup (xrds->firstRD);
	cleanSecondRD = strdup (xrds->secondRD);

	removeSpaces (&cleanFirstRD);
	removeSpaces (&cleanSecondRD);

	if ( ! isBbox(bbox)){

		printf("xrdsFillTemplate(): Error isBbox() return FALSE! "
				   "Invalid BOUNDING BOX.\n");
		//return ztInvalidArg;
		//FIXME xrdsFillTemplate() should return integer TODO

		return retValue; // set to NULL -
	}

	/* Note for snprintf(): the return type is of "size_t" AND if the return
	 * value is (LONG_LINE * 2) or more that means that the output was
	 * truncated - partial copy is an error.
	*******************************************************************/
	result = (int) snprintf (tmpBuf, (LONG_LINE * 2), queryTemplate,
					                      bbox->sw.latitude,bbox->sw.longitude,
										  bbox->ne.latitude, bbox->ne.longitude,
										  cleanFirstRD, cleanSecondRD);

	if (result > (LONG_LINE * 2) ){

		printf ("xrdsFillTemplate(): Error, QUERY buffer - tmpBuf size of "
				"(LONG_LINE * 2) is TOO SMALL.\n");
//return ztSmallBuf; // I return char* FIXME make return integer - else set global error
		return NULL;
	}

	retValue = (char *) malloc ((strlen(tmpBuf) + 1) * sizeof(char));
	if (retValue == NULL){
		printf ("xrdsFillTemplate(): Error allocating memory.\n");
		return retValue;
	}

	strcpy (retValue, tmpBuf);

	return retValue;

} //END xrdsFillTemplate()

int curlGetXrdsGPS(XROADS *xrds, BBOX *bbox, char *server,
		HTTP_METHOD method , int (*parseFunc) (XROADS *xrds, void *source)){

	MEMORY_STRUCT		myDataStruct;
	char		*query;
	int		result;
	char		*hdrSignature = "@lat	@lon	@count"; // no "\n" included

	ASSERTARGS (xrds && bbox && server && parseFunc);

	query = xrdsFillTemplate (xrds, bbox);
	if (query == NULL){

		printf("curlGetXrdsGPS(): Error returned from xrdsFillTemplate().\n");
		return ztMemoryAllocate;
	}

	result = curlMemoryDownload (&myDataStruct, server, query, method);
	if (result != ztSuccess){
		printf("curlGetXrdsGPS(): Error returned from curlMemoryDownload()!\n"
				" The error was: %s\n\n", code2Msg (result));
		return result;
	}

/* client can declare a global variable "rawDataFP" then open
 * file for writing with "rawDataFP" as the file pointer,
 * the received data will be written to that file.
 ******************************************************************************/

	/* write received data to client opened file if rawDataFP is set */
	if (rawDataFP){

		fprintf (rawDataFP, "Data for cross roads: [ %s && %s ]\n\n",
				xrds->firstRD, xrds->secondRD);

		fprintf (rawDataFP, "%s", myDataStruct.memory);
		fprintf (rawDataFP, "\n ++++++++++++++++++++++++++++++++++++++++++++++++\n\n");
		fflush (rawDataFP);

	}


	/* the above function call and a successful result test ONLY tell us that
	 * we received some response from the server. Is it what we want? Or
	 * is it an error message - bad query, busy or down server, maybe we
	 * got band by the server or ...? For Overpass server we get an HTML
	 * page in case of an error, where in a successful response the first line
	 * will be the HEADER we requested in our query by the following line from
	 * our query template:
	 * "[out:csv(::lat,::lon,::count)]"
	 * The above line use the default of "true or yes" for the header, to
	 * turn off header listing you have to change the above line to:
	 * "[out:csv(::lat,::lon,::count ; false)]"
	 *
	 * So with this out of the way, we examine the first line and compare
	 * it to the expected header before we proceed, an okay response is
	 * one that has a header which matches our expected header.
	 ************************************************************************/

	result = isOkResponse (myDataStruct.memory, hdrSignature);
	if (result != ztSuccess){
		printf("curlGetXrdsGPS(): Error returned from isOkResponse()!\n"
					" The error was: %s\n\n", code2Msg (result));
		return result;
	}

	/* NOTE: overpass server may return more than one common node between
	 * roads, however we only parse the GPS for the first returned node.
	 *************************************************************************/
	result = parseFunc (xrds, &myDataStruct); // call parseCurlXrdsData()
	if (result != ztSuccess){
		printf("curlGetXrdsGPS(): Error returned from parseCurlXrdsData()!\n"
					" The error was: %s\n\n", code2Msg (result));
		return result;
	}

	return ztSuccess;
}

/* isOkResponse(): remote server response is okay if first line in the
 * response matches header. - see comment in curlGetXrdsGPS() func
 * Question? : I am hoping this tells me the query was understood by
 * the server; the query syntax was correct.
 */

int isOkResponse (char *response, char *header){

	char		*firstLine, *memStart, *chPtr;
	int		chCount, retCode;

	ASSERTARGS (response && header);

	memStart = response;
	chPtr = strchr (memStart, '\n');

	chCount = (int) (chPtr - memStart);

	firstLine = (char *) malloc (sizeof(char) *  chCount + 1);
	if ( ! firstLine ){

		printf("isOkResponse(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}

	strncpy (firstLine, memStart, chCount);
	firstLine[chCount] = '\0';

	if (strcmp (firstLine, header) == 0){

		retCode = ztSuccess;
	}
	else {

		printf ("isOkResponse(): Error: Not a valid response. Server may responded "
				    "with an error message! The server response was:\n\n");
		printf (" Start server response below >>>>:\n\n");
		printf ("%s\n\n", response);
		printf (" >>>> End server response.\n\n");
		retCode = ztInvalidResponse;
	}

	free (firstLine);
	return retCode;
}

/* isBbox() : function checks that the structure pointed to by bbox holds
 * a valid bounding box as defined by Overpass API; that is the south-west
 * POINT is really south-west relative to north-east POINT.
 * Return: function returns TRUE if structure holds a valid bounding box and
 * FALSE otherwise.
 * Note: this replaces the never implemented checkBbox().
 ***************************************************************************/
int isBbox(BBOX *bbox){

	ASSERTARGS (bbox);

	// maybe a lot of noise!?
	if (bbox->sw.longitude > bbox->ne.longitude)
		printf ("isBbox(): invalid member is: LONGITUDE.\n");

	if (bbox->sw.latitude > bbox->ne.latitude)
		printf ("isBbox(): invalid member is: LATITUDE.\n");

	if ( (bbox->sw.longitude < bbox->ne.longitude) &&
		  (bbox->sw.latitude < bbox->ne.latitude) )

		return TRUE;

	return FALSE;

}

int namesFillTemplate(char **dst, BBOX *bbox){

	char		*qryTemplate = "[out:csv('name' ;false)];"
										  "way(%10.7f ,%10.7f ,%10.7f ,%10.7f)"
										  "[highway]; out;";
	int		result;
	char		tmpBuf[LONG_LINE * 2] = {0}; // large buffer


	ASSERTARGS (dst && bbox);

	*dst = NULL; // set value to return to NULL

	//result = checkBbox (bbox);
	//if (result != ztSuccess){
	if ( ! isBbox(bbox) ){

		printf("namesFillTemplate(): Error isBbox() return FALSE! "
				   "Invalid BOUNDING BOX.\n");
		return ztInvalidArg;
	}

	result = snprintf (tmpBuf,  (LONG_LINE * 2), qryTemplate,
				  	  	  	  	  bbox->sw.latitude,bbox->sw.longitude,
								  bbox->ne.latitude, bbox->ne.longitude);

	if (result > (LONG_LINE * 2) ){

		printf("namesFillTemplate(): Error; tmpBuf size of "
				   "(LONG_LINE * 2) is TOO SMALL.\n");
		return ztSmallBuf;
	}

	*dst = (char *) malloc ((strlen(tmpBuf) + 1) * sizeof(char));
	if (*dst == NULL){
		printf ("namesFillTemplate(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}

	strcpy (*dst, tmpBuf);

	return ztSuccess;
}

int getStreetNames (BBOX *bbox, char *server, char *dstFile){

	char 	*query;
	int		result;

	MEMORY_STRUCT	myMS;
	HTTP_METHOD		method;
	DL_LIST	lineDL;

	DL_ELEM		*elem;
	char				*str;

	ASSERTARGS (bbox && server && dstFile);

	result = namesFillTemplate (&query, bbox);
	if (result != ztSuccess){

		printf("getStreetNames(): Error returned from namesFillTemplate().\n");
		return result;
	}

	method = Get;

	result = curlMemoryDownload (&myMS, server, query, method);
	if (result != ztSuccess){
		printf("curlGetXrdsGPS(): Error returned from curlMemoryDownload()!\n"
				" The error was: %s\n\n", code2Msg (result));
		return result;
	}

	initialDL (&lineDL, NULL, NULL);

	result = response2LineDL (&lineDL, myMS.memory);
	if (result != ztSuccess){
		printf("curlGetXrdsGPS(): Error returned from response2LineDL()!\n");
		return result;
	}

printf("getStreetNames(): UNSORTED street names "
		"with count = %d street names below: \n", lineDL.size);
	elem = lineDL.head;
	while (elem){

		str = (char *) DL_DATA (elem);
		printf ("%s\n", str);

		elem = DL_NEXT (elem);
	}

printf("getStreetNames(): END UNSORTED street names I got.\n\n");

	/* sort the list */
	DL_LIST	sortedDL;

	initialDL (&sortedDL, NULL, NULL);

	elem = lineDL.head;
	while (elem){

		str = (char *) DL_DATA (elem);

		//int ListInsertInOrder (DL_LIST *list, char *str)
		ListInsertInOrder (&sortedDL, str);

		elem = DL_NEXT (elem);
	}

printf("getStreetNames(): SORTED list size is: %d\n\n", sortedDL.size);
printf("\ngetStreetNames(): START SORTED street names "
		"with count = %d street names below: \n", sortedDL.size);
	elem = sortedDL.head;
	while (elem){

		str = (char *) DL_DATA (elem);
		printf ("%s\n", str);

		elem = DL_NEXT (elem);
	}

printf("getStreetNames(): END SORTED street names.\n\n");


	return ztSuccess;
}

/* XROADS structure has a pointer to an array (GPS points);
 * memcpy() is not enough to do the job!
 * We allocate memory for dest structure.
 */
int cloneXrds (XROADS **dest, XROADS *src){

	ASSERTARGS (dest && src);

	return ztSuccess;
}

int initialXrds (XROADS **xrds, int numGPS){


	return ztSuccess;
}
