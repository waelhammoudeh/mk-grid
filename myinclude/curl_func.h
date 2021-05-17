/*
 * curl_func.h
 *
 *  Created on: Mar 31, 2021
 *      Author: wael
 */

#ifndef CURL_FUNC_H_
#define CURL_FUNC_H_

#ifndef CURLINC_CURL_H
#include  <curl/curl.h>
#endif

// structure from examples/getinmemory.c - added  typedef
typedef struct MEMORY_STRUCT_  {

	char		*memory;
	size_t	size;
} MEMORY_STRUCT;

typedef enum HTTP_METHOD_ {

	Get = 1, Post
}HTTP_METHOD;

int curlInitialSession(void);
void curlCloseSession(void);
int curlMemoryDownload(MEMORY_STRUCT *dst, char *urlPath,
		                                char *whichData, HTTP_METHOD method);

#endif /* CURL_FUNC_H_ */
