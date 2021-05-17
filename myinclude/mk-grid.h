/*
 * mk-grid.h
 *
 *  Created on: Apr 21, 2021
 *      Author: wael
 */

#ifndef MYINCLUDE_MK_GRID_H_
#define MYINCLUDE_MK_GRID_H_

#include "overpass-c.h" // has XROADS
#include "ztError.h"

#define	SERVICE_URL		"http://localhost/api/interpreter"
//#define	SERVICE_URL		"http://127.0.0.1/api/interpreter"
//#define	SERVICE_URL		"http://www.overpass-api.de/api/interpreter"

//exported global variables
extern const char *prog_name;
extern int	verbose;

void printUsage();
void shortUsage (FILE *toFP, ztExitCodeType exitCode);
int mkOutputFile (char **dest, char *givenName, char *rootDir);

#endif /* MYINCLUDE_MK_GRID_H_ */
