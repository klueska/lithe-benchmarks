/***************************************************************************
 * Copyright (C) 1998 - 2012, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>

int unique_id = 0;

struct thread_args {
  pthread_t pthread;
  void *retval;
  char *infile;
  char *outfile;
  char *url;
};

void *get_thumbnails(void *__arg)
{
  struct thread_args *arg = (struct thread_args *)__arg;
  CURL *curl;
  CURLcode res;
  FILE *hd_src, *hd_dst;
  struct stat file_info;

  /* get the file size of the local file */
  stat(arg->infile, &file_info);

  /* get a FILE * of the same file, could also be made with
     fdopen() from the previous descriptor, but hey this is just
     an example! */
  hd_src = fopen(arg->infile, "rb");
  hd_dst = fopen(arg->outfile, "wb");

  /* get a curl handle */
  curl = curl_easy_init();
  if(curl) {
    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* HTTP PUT please */
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);

    /* specify target URL, and note that this URL should include a file
       name, not only a directory */
    curl_easy_setopt(curl, CURLOPT_URL, arg->url);

    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);

    /* and specify which file to write to */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, hd_dst);

    /* provide the size of the upload, we specicially typecast the value
       to curl_off_t since we must be sure to use the correct data size */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)file_info.st_size);

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  fclose(hd_src); /* close the local file */
  return NULL;
}

int main(int argc, char **argv)
{
  char *infile, *url;
  int num_threads = 100;
  char *url_base = "http://c99.millennium.berkeley.edu:8080/generate_thumbnails";
  const char *outfile_suffix = "thumbnails";
  const char *outfile_ext = ".tgz";

  /* Parse the arguments.
   * Set the infile and update the url_base if appropriate. */
  if(argc < 2 || argc > 4)
    return 1;
  infile = argv[1];
  if(argc > 2)
    num_threads = atoi(argv[2]);
  if(argc > 3)
    url_base = argv[3];

  /* Get the length of the basename from the infile. */
  int infile_base_len;
  char *dotindex = rindex(infile, '.');
  if (dotindex == NULL)
    infile_base_len = strlen(infile);
  else infile_base_len = dotindex - infile;

  /* Set the url. */
  url = malloc(strlen(url_base) + 6 /* ?file= */ + strlen(infile) + 1 /* \0 */);
  sprintf(url, "%s?file=%s", url_base, infile);

  /* Actually get the thumbnails. */
  curl_global_init(CURL_GLOBAL_ALL);
  struct thread_args targs[num_threads];
  for (int i=0; i<num_threads; i++) {
    targs[i].infile = infile;
    targs[i].url = url;
    targs[i].outfile = malloc(infile_base_len + 1 /* - */
                            + strlen(outfile_suffix) + 1 /* - */
                            + 20 /* max_num_digits */ 
                            + strlen(outfile_ext) + 1 /* \0 */);
    sprintf(targs[i].outfile, "%.*s-%s-%d%s",
            infile_base_len, infile, outfile_suffix, i, outfile_ext);
    pthread_create(&targs[i].pthread, NULL, get_thumbnails, &targs[i]);
  }
  for (int i=0; i<num_threads; i++) {
    pthread_join(targs[i].pthread, &targs[i].retval);
  }
  curl_global_cleanup();

  /* Cleanup allocated memory. */
  free(url);
  return 0;
}
