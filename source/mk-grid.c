/*
 * mk-grid.c
 *
 *  Created on: Apr 15, 2021
 *      Author: wael
 *
 *   simple program to produce Well Known Text file to draw polygons in QGIS
 *  output example as csv file:
 wkt;
"POLYGON ((-112.0739105 33.5597757, -112.0652852 33.5605235, -112.0652213 33.5527166, -112.0739008 33.5528019, -112.0739105 33.5597757))"
"POLYGON ((-112.0739008 33.5528019, -112.0652213 33.5527166, -112.0651409 33.5455279, -112.0738334 33.5455559, -112.0739008 33.5528019))"
"POLYGON ((-112.0738334 33.5455559, -112.0651409 33.5455279, -112.0650791 33.5382827, -112.0737604 33.5383198, -112.0738334 33.5455559))"
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h> // error number used with fopen() call
#include <getopt.h> // GNU getopt_long() use
#include <unistd.h> // access() call
#include <curl/curl.h>

#include "mk-grid.h"
#include "overpass-c.h"
#include "util.h"
#include "ztError.h"
#include "fileio.h" // this includes dList.h on top
#include "network.h"
#include "url_parser.h"
#include "curl_func.h"
#include "op_string.h"
#include "mkWKT.h"

// global variables
const char *prog_name;  // prog_name is global
int		verbose = 0; // verbose is off by default
FILE     *rawDataFP = NULL; // rawDataFP is global


// function prototype
static int appendDL (DL_LIST *dest, DL_LIST *src);

int main(int argc, char* const argv[]) {

	char		*home,
	            progDir[PATH_MAX] = {0};

	const 	char*	const	shortOptions = "ho:vfr:";
	const	struct	option	longOptions[] = {
			{"help", 		0, NULL, 'h'},
			{"output", 	1, NULL, 'o'},
			{"verbose", 0, NULL, 'v'},
			{"force", 0, NULL, 'f'},
			{"raw-data", 1, NULL, 'r'},
			{NULL, 0, NULL, 0}

	};

	int		nextOption; // getopt_long() returns -1 when done

	int		overWrite = 0; // do not over write existing file
	char		*outputFileName = NULL;
	char		*rawDataFileName = NULL;

	char		outputFileNameCSV[PATH_MAX] = {0};

	FILE		*outputFilePtr = NULL;

	char		**argvCurrent;

	int		result,
				reachable;
	char		*ipBuf;
	char		*service_url,
				*serverOnly,
				*proto; // protocol
	struct parsed_url 	*urlParsed = NULL;

	MK_WKT_RETURN	*retWKT; // mkPlygnWKT() returns 2 lists in ONE structure
	DL_LIST					*wktSessionDL;
	DL_LIST					*notFndSessionDL;

	/* FIRST things to do set program name, used for feedback ...
	lastOfPath(), we might get called with a path */
	prog_name = lastOfPath (argv[0]);

	if (argc < 2) // missing required argument

		shortUsage(stderr, ztMissingArgError);

	// set program output directory - {user home}/prog_name
	home = getHome();
	ASSERTARGS(home);

	if(IsSlashEnding(home))
		// size = (PATH_MAX - 100) allow for file name
		snprintf (progDir, (PATH_MAX - 100), "%s%s", home, prog_name);
	else
		snprintf (progDir, (PATH_MAX - 100), "%s/%s", home, prog_name);

	result = myMkDir(progDir); // creates directory if it does not exist.
	if (result != ztSuccess){

		fprintf(stderr, "%s: Error. Failed to create program output directory.\n", prog_name);
		fprintf(stderr, " The error was: %s\n", code2Msg(result));
		return result;
	}


	/* let getopt_long() do the work of parsing command line */
	do {

		nextOption = getopt_long(argc, argv, shortOptions, longOptions, NULL);

		switch (nextOption){

		case 'h': // show help and exit.

			printUsage(stderr, ztMissingArgError);
			return ztSuccess;
			break;

		case 'v': // turn verbose mode on, verbose does not take any argument

			verbose = 1;
			fprintf (stdout, "verbose option is NOT implemented yet! Care to do it?\n");
			break;

		case 'f':

			overWrite = 1;
			break;

		case 'o':

			/* optarg points to output file name *****/

			if ( ! IsGoodFileName(optarg) ){
				fprintf (stderr, "%s: Error invalid file name specified for output: <%s>\n",
						      prog_name, optarg);
				return ztBadFileName;
			}

			// make output file name to include progDir PATH -- NEEDS checking!!
			mkOutputFile (&outputFileName, optarg, progDir);
			ASSERTARGS (outputFileName);

			break;

		case 'r':

			if ( ! IsGoodFileName(optarg) ){
				fprintf (stderr, "%s: Error invalid file name specified for raw-data: <%s>\n",
						    prog_name, optarg);
				return ztBadFileName;
			}

			// make raw data file name
			mkOutputFile (&rawDataFileName, optarg, progDir);
			ASSERTARGS (rawDataFileName); // bad choice for checking

			break;

		case '?':

			fprintf (stderr, "%s: Error unknown option specified.\n", prog_name);
			shortUsage (stderr, ztUnknownOption); // show usage and exit.

			break;

		case -1: // done with options

			break;

		default : // something unusual? who knows?

			abort();

		} // end switch (nextOption)

	} while ( nextOption != -1 );

	// optind is current argv[] index --

	if (optind == argc){ // nothing is left for required input filename

		fprintf (stderr, "%s: Error missing required argument for input file name.\n",
				    prog_name);

		shortUsage (stderr, ztMissingArgError);
	}

	/* check connection to server
	 * parse the SERVICE_URL -- using parse_url() function here.
	 * Note that there is a libcurl function doing the same thing!
	 * curl_url_get() was added in version 7.62.0
	 **********************************************************/
	service_url = strdup (SERVICE_URL);

	urlParsed = parse_url(service_url);
	if (NULL == urlParsed){

		fprintf(stderr, "%s error: FROM parse_url() function.\n", prog_name);
		return ztParseError;
	}
	    /* first parameter to checkURL() is the server name only */
	serverOnly = urlParsed->host;
	proto = urlParsed->scheme;
	result = checkURL (serverOnly , proto, &ipBuf);
	reachable = (result == ztSuccess);
	if ( ! reachable ){

		fprintf(stderr, "%s: Error SERVER: (%s) is NOT reachable.\n", prog_name, serverOnly);
		fprintf(stderr, " The error was: %s\n", code2Msg(result));

		return result;
	}

	if (ipBuf)
		free(ipBuf); // not needed anymore

	// if output file name is specified, append ".csv" if not provided
	if ( outputFileName ){

		if (strrchr(outputFileName, '.'))
			strcpy (outputFileNameCSV, outputFileName);
		else {
			sprintf (outputFileNameCSV, "%s.csv", outputFileName);
			fprintf (stdout, "%s: saving your output to file: %s\n", prog_name, outputFileNameCSV);
		}

		// do not overwrite existing output file, unless force was used
		if ( (access(outputFileNameCSV, F_OK) == 0) && ! overWrite ){

		printf ("%s: Error specified output file <%s> already exists. "
				"Use \"force\" option to over write it.\n", prog_name, outputFileNameCSV);
		return ztInvalidArg;
		}

	} // end if (outputFileName)

	// assume input file list -- check files : exist, readable, empty?
	// do not use same name 4 input & output
	argvCurrent = (char **) (argv + optind);
	while (*argvCurrent){

		result = IsArgUsableFile(*argvCurrent);
		if (result != ztSuccess){

			fprintf(stderr, "%s error: input file <%s> is Not usable file!\n",
					prog_name, *argvCurrent);
			fprintf(stderr,  " The error was: %s\n", code2Msg(result));
			return result;
		}

		/* do not overwrite any INPUT file ANYTIME */
		if (outputFileName && (strcmp(*argvCurrent, outputFileNameCSV) == 0)){ // think about it TODO
			fprintf(stderr, "%s Error: Can not write to an input file: <%s>\n\n",
					prog_name, *argvCurrent);
			return ztInvalidArg;
		}

		if (rawDataFileName && (strcmp(*argvCurrent, rawDataFileName) == 0)){
			fprintf(stderr, "%s Error: Can not write raw data to an input file: <%s>\n\n",
					prog_name, *argvCurrent);
			return ztInvalidArg;
		}

		argvCurrent++;

	} // end  while () .. check files

	// open output file(s) if set. Also append "csv" file extension when there is none
	if (outputFileName){

		/* append csv (comma separated variables) extension to output file name ONLY
		 * when file name has no extension  */

		if (strrchr(outputFileName, '.'))
			strcpy (outputFileNameCSV, outputFileName);
		else {
			sprintf (outputFileNameCSV, "%s.csv", outputFileName);
			fprintf (stdout, "%s: Added extension '.csv' to output to file: %s\n", prog_name, outputFileNameCSV);
		}

		outputFilePtr = openOutputFile (outputFileNameCSV);
		if ( ! outputFilePtr) {
			fprintf (stderr, "%s: Error opening output file: <%s>\n",
			             prog_name, outputFileName);
			return ztOpenFileError;
		}
	}

	if (rawDataFileName){ // writing is done by curlGetXrdsGPS() in overpass.c

		rawDataFP = openOutputFile (rawDataFileName);
		if ( ! rawDataFP ) {
			fprintf (stderr, "%s: Error opening raw data output file: <%s>\n",
					     prog_name, rawDataFileName);
			return ztOpenFileError;
		}
	}

	// initial lists
	wktSessionDL = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (wktSessionDL == NULL){
		fprintf (stderr, "%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}

	notFndSessionDL = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (notFndSessionDL == NULL){
		fprintf (stderr, "%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}

	retWKT = (MK_WKT_RETURN *) malloc(sizeof(MK_WKT_RETURN));
	if (retWKT == NULL){
		fprintf (stderr, "%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}

	initialDL (wktSessionDL, zapString, NULL); // zapString() is in util.c
	initialDL (notFndSessionDL, zapString, NULL);

	// start with Well Known Text marker
	insertNextDL (wktSessionDL, DL_HEAD(wktSessionDL), "wkt;");

	argvCurrent = (char **) (argv + optind); // reset pointer to first input file

	while (*argvCurrent){

		retWKT->wktDL = (DL_LIST *) malloc (sizeof(DL_LIST));
		retWKT->notFndDL = (DL_LIST *) malloc (sizeof(DL_LIST));
		if ( ! (retWKT->wktDL && retWKT->notFndDL) ){
			printf ("%s: Error allocating memory.\n", prog_name);
			return ztMemoryAllocate;
		}
		// initial lists for return
		initialDL (retWKT->wktDL, zapString, NULL);
		initialDL (retWKT->notFndDL, zapString, NULL);

		result = mkPlygnWKT (retWKT, service_url, *argvCurrent);
		if (result != ztSuccess){
			fprintf (stderr, "%s: Error returned from mkPlygnWKT() for file: <%s>\n",
					prog_name, *argvCurrent);
			return result; // should clean up
		}

		if (DL_SIZE(retWKT->notFndDL)){ // let user know about NOT FOUND cross roads
			fprintf(stdout, "%s: Warning :: Could not get GPS for some cross roads in file <%s>.\n",
					   prog_name, *argvCurrent);
			fprintf(stdout, "%s: Note cross roads below total of <%d> line(s) ...\n",
					   prog_name, DL_SIZE(retWKT->notFndDL));
			printStringDL(retWKT->notFndDL);
		}
/*		else
			printf("%s: Seems NOT Found File DL IS EMPTY. Way to go. Lucky YOU :)\n", prog_name);
*/

		// show WKT for current input file
		if (DL_SIZE(retWKT->wktDL)) {
			fprintf(stdout, "%s: Formated WKT list for input file <%s> has [%d] lines as follows:\n",
						prog_name, *argvCurrent, DL_SIZE(retWKT->wktDL));

			printStringDL (retWKT->wktDL);
		}
		else
			printf("%s: This should not happen! But we have an EMPTY list for file <%s>\n",
					prog_name, *argvCurrent);

		appendDL (wktSessionDL, retWKT->wktDL);
		appendDL (notFndSessionDL, retWKT->notFndDL);

		argvCurrent++; // next input file?

	} // end  while (*argvCurrent)

	// write output
	writeDL (outputFilePtr, wktSessionDL, writeString2FP);

	if (DL_SIZE(notFndSessionDL)){
		fprintf (stdout, "%s: Could not get GPS for some cross roads! Please see list below.\n\n", prog_name);
		writeDL (NULL, notFndSessionDL, writeString2FP);
	}
	//printStringDL (notFndSessionDL);

	if (outputFilePtr){
		fflush (outputFilePtr);
		fclose(outputFilePtr);
		fprintf (stdout, "%s: Wrote output file : <%s>\n\n", prog_name, outputFileNameCSV);
	}

	if (rawDataFP){
		fflush (rawDataFP);
		fclose(rawDataFP);
		fprintf (stdout, "%s: Wrote raw-data file to : <%s>\n\n", prog_name, rawDataFileName);
	}

	return ztSuccess;

} // END main()

void printUsage(){ // prog_name is global here!


	char *infile_desc =
			"%s: Well Known Text for grid.\n"
			"Given street names for cross roads (intersections) and a bounding box program\n"
			"produces Well Known text (WKT) formatted file for polygons (rectangles) with\n"
			"GPS coordinates fetched from Open Street Map Overpass server. This output can\n"
			"be used to draw a grid over a map in Graphical Information System (GIS) such\n"
			"as QGIS.\n"
			"For information about QGIS see their website at: https://qgis.org/en/site/\n\n"

			"Usage: %s [options] inputfile [files ...]\n"

			"Where \"inputfile\" is a required argument specifying the input file name, or\n"
			"optionally [file ..] a list of files, space separated.\n"
			"And \"options\" are as follows: (either short or long option)\n"

			"  -h   --help              Displays full program description.\n"
			"  -o   --output filename   Writes output to specified \"filename\"\n"
			"  -f   --force             Use with output option to force overwriting existing \"filename\"\n"
			"  -r   --raw-data filename Writes received (downloaded) data from server to \"filename\"\n\n"

			"  Output directory: On invocation program creates a directory entry with program\n"
			"name \"mk-gridWKT\" under the effective user home directory, this directory is\n"
			"used for all program output files specified as single file name with any output\n"
			"option, if \"filename\" includes file system path then that is used instead.\n"
			"Currently this behaviour can not be changed.\n\n"

			"Program options: Options can be specified with short or long form, \"filename\"\n"
			"after the option indicates a file name required argument for that option.\n\n"

			" --help : displays this help information.\n\n"

			" --output filename : By default, program writes its output to stdout, using this\n"
			"                     option instructs program to write its output to \"filename\".\n"
			"                     Program will append '.csv' to filename if it has no extension.\n\n"

			" --force : By default, program will not overwrite existing file, by using this\n"
			"           option user can force program to change this default.\n\n"

			" --raw-data filename : Program uses curl library memory download, query results\n"
			"                       are not saved to disk. Using this option user can save\n"
			"                       query results to \"filename\".\n\n"

			"Input file format:\n"
			"Input file is a text file with two supported formats. The first is the 'Four\n"
			"Corners Format' and the second is the 'Row X column Format'.\n"
			"Lines starting with a hash mark '#' or semi-colon ';' are comments lines and\n"
			"will be ignored.\n\n"
			"The Bounding Box Line:\n"
			"The second line in both formats is the bounding box which is defined by two\n"
			"points, namely South-West (sw) point and North-East (ne) point specified in\n"
			"decimal degrees listed with (latitude, longitude) order as follows:\n"
			" sw-latitude, sw-longitude, ne-latitude, ne-longitude\n"
			"Examples:\n"
			"33.53097, -112.07400, 33.56055 , -112.0567345\n"
			"33.45040, -112.07449, 33.51019, -111.99489\n\n"

			"Four Corners Format:\n"
			"The first line is an integer specifying the number of rectangles included or\n"
			"listed in the file.\n"
			"The second line is the bounding box enclosing ALL cross roads listed.\n"

			"The third line starts listing rectangles, with four points each, each point is\n"
			"an intersection of two road / street names comma separated per line.\n"
			"Example of ONE rectangle):\n\n"
			"East Camelback Road, North 7Th Street\n"
			"East Bethany Home Road, North 7Th Street\n"
			"East Bethany Home, North 16Th Street\n"
			"East Camelback Road, North 16Th Street\n\n"

			"Continue listing other rectangles below that, as many as you stated on line one.\n"
			"Please be consistent with your movement around the rectangles; clockwise or\n"
			"counterclockwise with all listed.\n\n"

			"Row X column Format:\n"
			"First line is for dimension: RR X CC order, where RR is the number of rows and CC\n"
			"is the number of columns. And X is one in three character set [ xX* ].\n"
			"A minimum of 2 rows and 2 columns are required to make at least one rectangle grid.\n"
			"Rows will be crossed with columns to make two street names intersections.\n"
			"The rows are for horizontal [east-west] streets / roads, and columns are for vertical\n"
			"or [north-south] streets / roads.\n"
			"The second line is the bounding box described above to enclose ALL intersections.\n"
			"The third line starts listing RR rows number of the dimension above one street\n"
			"name per line.\n"
			"After rows are completed, start listing CC number of columns street names, one\n"
			"per line also.\n"

			"The rows are listed moving from south to north, columns are list west to east.\n"
			"That is moving UP and to the RIGHT on a sheet of paper.\n\n"

			"Please do not mix format in one input file. Program can process a list of\n"
			"files, those files can be in different format. One output is produced for\n"
			"all files.\n\n"

			"How to use the program:\n"
			"Program utilizes Open Street Maps data and services to accomplish its goal.\n"
			"WKT format is a standard recognized by GIS drawing programs such as QGIS, text\n"
			"files in Well Known Text format can be used to add a layer in QGIS maybe others.\n"
			"There maybe instances where intersection GPS will not be found. The program will\n"
			"produce a list of not found intersections. Make sure your bounding box includes\n"
			"all intersections first. Sometimes you may have to manually edited the output\n"
			"file.\n"
			"That said, using QGIS we add a layer as 'Delimited Text layer' using the output\n"
			"from this program as the input file for that layer. From the top menu select:\n"
			" Layer -> Add Layer -> Add Delimited Text Layer\n"
			"then in the dialog specify your file name (or browse to it), the 'Add' button\n"
			"should be enabled - providing you do not have any missing GPS.\n"
			"Then in layer properties make your 'Fill Color' Transparent Fill. Make your layer\n"
			"the upper layer on top of OpenStreetMap, enjoy :) .\n\n"

			"An example for row X column format input file is below:\n\n"
			"# start with dimension row X column\n"
			"5 X 6\n\n"
			"# lines starting with # or ; are comment lines and are ignored\n"
			"# format for BBOX: (sw, ne); point: (latitude, longitude)\n"
			"33.45040, -112.07449, 33.51019, -111.99489\n\n"
			"# east-west roads - 5 rows, from south to north\n"
			"\n"
			"East Van Buren Street\n"
			"East McDowell Road\n"
			"East Thomas Road\n"
			"East Indian School Road\n"
			"East Camelback Road\n"
			"\n"
			"# north-south roads 6 columns, from west to east\n"
			"North Central Avenue\n"
			"North 7Th Street\n"
			"North 16Th Street\n"
			"North 24Th Street\n"
			"North 32ND Street\n"
			"North 40Th Street\n\n";


	printf (infile_desc, prog_name, prog_name);

	return;
}

void shortUsage (FILE *toFP, ztExitCodeType exitCode){

	fprintf (stderr, "\n%s: Make Well Known Text for grid; program queries OSM Overpass server for GPS of\n"
			"cross roads to produce Well Known Text formatted output to draw grid in QGIS. Cross roads\n"
			"are read from a text file.\n\n", prog_name);

	fprintf (toFP, "Usage: %s [options] inputfile [files ...]\n\n", prog_name);

	fprintf (toFP, "Where \"inputfile\" is a required argument specifying the input file name or optionally\n"
			"[a list of files, space separated] to process.\n"
			"And \"options\" are as follows:\n\n"
			"  -h   --help              Displays full program description.\n"
            "  -o   --output filename   Writes output to specified \"filename\" instead of standard output.\n"
			"  -f   --force             Use with output option to force overwriting existing \"filename\".\n"
			"  -r   --raw-data filename Writes received response from server to \"filename\".\n"
			"\n"
			"Try: '%s --help'\n\n", prog_name);

	exit (exitCode);

}

/* appendDL () appends src list to dest list - assumes the data pointer in
 * element is for character string, no checking for that is done! */
static int appendDL (DL_LIST *dest, DL_LIST *src){

	DL_ELEM	*srcElem;
	char			*srcData;
	char			*newStr;
	int			result;

	ASSERTARGS (dest && src);

	if (DL_SIZE(src) == 0)

		return ztSuccess;

	srcElem = DL_HEAD(src);
	while (srcElem){

		srcData = (char *) DL_DATA(srcElem);
		newStr = strdup(srcData);

		if (! newStr)

			return ztMemoryAllocate;

		result = insertNextDL (dest, DL_TAIL(dest), newStr);
		if (result != ztSuccess)

			return ztMemoryAllocate;

		srcElem = DL_NEXT(srcElem);

	}

	return ztSuccess;
}

/* mkOutFile(): make output file name, sets dest to givenName if it has a slash,
 * else it appends givenName to rootDir and then sets dest to appended string
 */
int mkOutputFile (char **dest, char *givenName, char *rootDir){

	char		slash = '/';
	char		*hasSlash;
	char		tempBuf[PATH_MAX] = {0};

	ASSERTARGS (dest && givenName && rootDir);

	hasSlash = strchr (givenName, slash);

	if (hasSlash)

		*dest = (char *) strdup (givenName); // strdup() can fail .. check it FIXME

	else {

		if(IsSlashEnding(rootDir))

			snprintf (tempBuf, PATH_MAX -1 , "%s%s", rootDir, givenName);
		else
			snprintf (tempBuf, PATH_MAX - 1, "%s/%s", rootDir, givenName);

		*dest = (char *) strdup (&(tempBuf[0]));

	}

	/* mystrdup() ::: check return value of strdup() TODO */

	return ztSuccess;
}

