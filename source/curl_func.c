#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#include "curl_func.h"
#include "util.h"
#include "ztError.h"

// global variables
static	CURL 	*curl_handle;
static 	int		sessionFlag = 0;
static	int 		afterPostFlag = 0; // this is ugly! But maybe I am catering for stupid.

/* curlInitialSession() : initials curl session, must call this function before
 * using any other function, call curlCloseSession() to end the session ****/

int curlInitialSession(void){

	CURLcode	result;

	if (sessionFlag)

		return 0;

	result = curl_global_init(CURL_GLOBAL_ALL);
	if (result != 0){
	    fprintf(stderr, "curl_global_init() failed: %s\n",
	            curl_easy_strerror(result));
	    return result;

	}

	/* init the curl session */
	curl_handle = curl_easy_init();
	if (curl_handle == NULL){
		fprintf(stderr, "curlInitialSession(): Error curl_easy_init() failed. got NULL handle!\n\n");
		return ztGotNull;
	}

	sessionFlag = 1;

	return result;
}

void curlCloseSession(void){

	if (sessionFlag == 0)

		return;

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	/* we're done with libcurl, so clean it up */
	curl_global_cleanup();

	sessionFlag = 0;

	return;
}

static size_t WriteMemoryCallback (void *contents, size_t size,
															   size_t nmemb, void *userp) {

  size_t realsize = size * nmemb;
  MEMORY_STRUCT *mem = (MEMORY_STRUCT *) userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */
    printf("WriteMemoryCallback(): Error not enough memory "
    		"(realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}


/*** to use curlMemoryDownload() function, you must call curlInitialSession() FIRST.
 *    call curlCloseSession() when done. I have not tested mixing methods.
 *    Para: dst is a pointer to MemoryStrut allocated by the client / caller. Function
 *    allocates memory for memory member, downloaded data is copied there, function
 *    also sets size to the downloaded data byte count.
 *    urlPath: pointer to string including protocol plus the path to remote server; example:
 *      http://localhost/api/interpreter
 *    whichData: character pointer specifying the remote resource - file or script.
 *    method: enumerated number for HTTP method {Get or Post}.
 *    Return: ztSuccess, Numbers of error codes .....
 *    Caller on successful return should copy the data right away.
 */

int curlMemoryDownload(MEMORY_STRUCT *dst, char *urlPath,
		                                char *whichData, HTTP_METHOD method) {

  CURLcode 	res;
  char			*srvrPath;
  char 			*query;
  char			getBuf[PATH_MAX] = {0};
  char 			*curlEscaped = NULL;

  if (sessionFlag == 0){
	  fprintf(stderr, "curlMemoryDownload(): Error, session not initialized. You must call\n "
			     " curlInitialSession() first and check its return value.\n");
	  return ztInvalidUsage;
  }

  if (curl_handle == NULL){
	  fprintf(stderr, "curlMemoryDownload(): Error, curl_handle is NULL. "
			  "Maybe you should abort()!\n");
	  return ztUnknownError;
  }

  ASSERTARGS (dst && urlPath && whichData);

  /** USER SHOULD LIMIT SESSION TO ONE METHOD ONLY - DON'T MIX **/
  if ((method != Get) && (method != Post)) {
	  printf ("curlMemoryDownload(): Error unsupported method specified. \n"
			       "  supported methods are Get or Post.\n\n");
	  return ztInvalidArg;

  }

  dst->memory = malloc(1);
  dst->size = 0;

  srvrPath = urlPath;
  query = whichData;

  /* start common CURLOPTs *******/

  res = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  if(res != CURLE_OK) {
	  fprintf(stderr, "curl_easy_setopt() failed to set WRITEFUNCTION "
   				  "{CURLOPT_WRITEFUNCTION}: %s\n", curl_easy_strerror(res));
	  return res;
  }

  res = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)dst);
  if(res != CURLE_OK) {
	  fprintf(stderr, "curl_easy_setopt() failed to set WRITEDATA "
   				  "{CURLOPT_WRITEDATA}: %s\n", curl_easy_strerror(res));
	  return res;
  }

  res = curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  if(res != CURLE_OK) {
	  fprintf(stderr, "curl_easy_setopt() failed to set USERAGENT "
   				  "{CURLOPT_USERAGENT}: %s\n", curl_easy_strerror(res));
	  return res;
  }

  //curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);  // FIXME do not leave here

  // start Get method
  if (method == Get){

	  if (afterPostFlag){

		  res = curl_easy_setopt (curl_handle, CURLOPT_HTTPGET, 1L);
		  if(res != CURLE_OK) {
			  fprintf(stderr, "curl_easy_setopt() failed to set HTTPGET "
		   				  "{CURLOPT_HTTPGET}: %s\n", curl_easy_strerror(res));
			  return res;
		  }

		  afterPostFlag = 0;

	  }
	  /* I escape  (encode) the query part ONLY ... this may not be correct! */
	  curlEscaped = curl_easy_escape(curl_handle, query, strlen(query));
	  if ( curlEscaped == NULL ){
		  printf("curlMemoryDownload(): Error returned by curl_easy_esacpe(). It is NULL!!!\n");
		  return ztGotNull;
	  }

	  sprintf(getBuf, "%s?data=%s", srvrPath, curlEscaped);

	  // turn on TRANSFER_ENCODING
	  res = curl_easy_setopt (curl_handle, CURLOPT_TRANSFER_ENCODING, 1L);
	  if(res != CURLE_OK) {
		  fprintf(stderr, "curl_easy_setopt() failed to set TRANSFER_ENCODING"
	   				  "{CURLOPT_TRANSFER_ENCODING}: %s\n", curl_easy_strerror(res));
		  return res;
	  }

	  res = curl_easy_setopt(curl_handle, CURLOPT_URL, getBuf);
	  if(res != CURLE_OK) {
		  fprintf(stderr, "curl_easy_setopt() failed to set URL "
	   				  "{CURLOPT_URL getBuf}: %s\n", curl_easy_strerror(res));
		  return res;
	  }

  } // end if Get

  if (method == Post){

 	  afterPostFlag = 1;

 	  // tell it to use POST http method -- third parameter set to one
 	  res = curl_easy_setopt (curl_handle, CURLOPT_POST, 1L);
 	  if(res != CURLE_OK) {
 		  fprintf(stderr, "curl_easy_setopt() failed to set POST method "
 				  "{CURLOPT_POST}: %s\n", curl_easy_strerror(res));
 		  return res;
 	  }

 	  // what to POST -- third parameter is the pointer to our query string
 	  res = curl_easy_setopt (curl_handle, CURLOPT_POSTFIELDS, query);
 	  if(res != CURLE_OK) {
 		  fprintf(stderr, "curl_easy_setopt() failed to set POSTFIELD "
 				  "{CURLOPT_POSTFIELDS}: %s\n", curl_easy_strerror(res));

 		  return res;
 	  }

 	  res = curl_easy_setopt(curl_handle, CURLOPT_URL, srvrPath);
 	  if(res != CURLE_OK) {
 		  fprintf(stderr, "curl_easy_setopt() failed to set URL to srvrPath "
 				  "{CURLOPT_URL}: %s\n", curl_easy_strerror(res));

 		  return res;
 	  }
   } // end if Post


  /* get it! with a message: */

  printf ("Querying server %s\n", srvrPath);

  res = curl_easy_perform(curl_handle);

  /* check for errors */
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }

  else {

	  printf("Got response: "
			  	  "%lu bytes retrieved\n\n", (unsigned long) dst->size);

  }

  if (curlEscaped)
	  curl_free(curlEscaped);

  return ztSuccess;

}
