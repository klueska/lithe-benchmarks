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
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "timing.h"
#include "futex.h"

float threads_per_second = 1;
int max_outstanding = 10;
char *url_base = "http://c99.millennium.berkeley.edu:8080/generate_thumbnails";
const char *outfile_suffix = "thumbnails";
const char *outfile_ext = ".tgz";

size_t unique_id = 0;
size_t num_ready = 0;
size_t num_serviced = 0;
double *latencies = NULL;
size_t latencies_size = 0;
pthread_mutex_t latencies_lock = PTHREAD_MUTEX_INITIALIZER;

struct thread_args {
  char *infile;
  int infile_base_len;
  char *url;
};

void calibrate_all_tscs();

static double gettime()
{
  struct timeval t;
  gettimeofday(&t, 0);
  return t.tv_sec + 1e-6*t.tv_usec;
}

static double interval_throughput(uint64_t last_tag, uint64_t curr_tag,
                               double last_time, double curr_time)
{
  uint64_t range = curr_tag - last_tag;
  if (range == 0)
    return 0;

  return (curr_tag - last_tag) / (double)(curr_time - last_time);
}

static double interval_avg_latency(uint64_t last_tag, uint64_t curr_tag)

{
  double sum = 0;
  uint64_t range = curr_tag - last_tag;
  if (range == 0)
    return 0;

  pthread_mutex_lock(&latencies_lock);
  for (uint64_t i = last_tag; i < curr_tag; i++) {
    sum += latencies[i];
  }
  pthread_mutex_unlock(&latencies_lock);
  return sum / (double)(curr_tag - last_tag);
}

static double interval_std_latency(uint64_t last_tag, uint64_t curr_tag,
                                  double avg_lat)
{
  double sum = 0;
  uint64_t range = curr_tag - last_tag;
  if (range == 0)
    return 0;

  pthread_mutex_lock(&latencies_lock);
  for (uint64_t i = last_tag; i < curr_tag; i++) {
    sum =+ pow(latencies[i] - avg_lat, 2);
  }
  pthread_mutex_unlock(&latencies_lock);
  return sqrt(sum / (double)(curr_tag - last_tag));
}

static void *print_stats(void *arg)
{
  double start_time, t0, t1;
  uint64_t tag0, tag1;
  start_time = t0 = gettime();
  tag0 = 0;

  while (1) {
    sleep(10);
    t1 = gettime();
    pthread_mutex_lock(&latencies_lock);
    tag1 = num_serviced;
    pthread_mutex_unlock(&latencies_lock);
    double tput = interval_throughput(tag0, tag1, t0, t1);
    double avg_lat = interval_avg_latency(tag0, tag1);
    double std_lat = interval_std_latency(tag0, tag1, avg_lat);
    fprintf(stdout, "%f requests/sec, "
                    "%f avg latency, "
                    "%f std latency, "
                    "%ld total requests, "
                    "%f interval time, "
                    "%f total time\n",
            tput, avg_lat, std_lat, tag1 - tag0, t1 - t0, t1 - start_time);
    fflush(stdout);
    tag0 = tag1;
    t0 = t1;
  }
}

static inline bool atomic_add_not_zero(size_t *number, long val)
{
    size_t old_num, new_num;
    do {
        old_num = __sync_fetch_and_add(number, 0);
        if (!old_num)
            return false;
        new_num = old_num + val;
    } while (!__sync_bool_compare_and_swap(number, old_num, new_num));
    return true;
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  int res = fwrite(ptr, size, nmemb, userdata);
  fflush(userdata);
  return res;
}

void *get_thumbnails(void *__arg)
{
  struct thread_args *arg = (struct thread_args *)__arg;
  CURL *curl;
  CURLcode res;
  FILE *hd_src, *hd_dst;
  struct stat file_info;

  /* Get the file size of the input file and open it. */
  stat(arg->infile, &file_info);
  hd_src = fopen(arg->infile, "rb");

  /* Set the output file name. */
  int id = __sync_fetch_and_add(&unique_id, 1);
  char *outfile = malloc(arg->infile_base_len + 1 /* - */
                         + strlen(outfile_suffix) + 1 /* - */
                         + 20 /* max_num_digits */
                         + strlen(outfile_ext) + 1 /* \0 */);
  sprintf(outfile, "%.*s-%s-%d%s", arg->infile_base_len, arg->infile,
                                   outfile_suffix, id, outfile_ext);

  /* Open the output file. */
  hd_dst = fopen(outfile, "wb");

  /* Setup a curllist to set the Expect: header. */
  struct curl_slist *list = NULL;
  list = curl_slist_append(list, "Expect:");

  while (1) {
    /* Do admission control. */
    while (!atomic_add_not_zero(&num_ready, -1))
      futex_wait(&num_ready, 0);

    /* Rewind the input and output files. */
    rewind(hd_src);
    rewind(hd_dst);

    /* Start the put operation. */
    curl = curl_easy_init();
    if (curl) {
      /* enable uploading */
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

      /* HTTP PUT please */
      curl_easy_setopt(curl, CURLOPT_PUT, 1L);

      /* Disable the 100-continue header */
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      /* specify target URL, and note that this URL should include a file
         name, not only a directory */
      curl_easy_setopt(curl, CURLOPT_URL, arg->url);

      /* now specify which file to upload */
      curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);

      /* and specify which file to write to */
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, hd_dst);

      /* provide the size of the upload, we specicially typecast the value
         to curl_off_t since we must be sure to use the correct data size */
      curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                       (curl_off_t)file_info.st_size);

      /* Now run off and do what you've been told! */
      double beg = gettime();
      res = curl_easy_perform(curl);
      double end = gettime();
      /* Check for errors */
      if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
      } else {
        pthread_mutex_lock(&latencies_lock);
          if (num_serviced + 1 > latencies_size) {
            latencies_size = 2 * (num_serviced + 1);
            latencies = realloc(latencies, latencies_size * sizeof(double));
          }
          latencies[num_serviced] = end - beg;
          num_serviced++;
        pthread_mutex_unlock(&latencies_lock);
      }
    }
    curl_easy_cleanup(curl);
  }
  free(outfile);
  return NULL;
}

int main(int argc, char **argv)
{
  char *infile, *url;

  /* Parse the arguments.
   * Set the infile and update the url_base if appropriate. */
  if(argc < 2 || argc > 5)
    return 1;
  infile = argv[1];
  if(argc > 2)
    threads_per_second = atof(argv[2]);
  if(argc > 3)
    max_outstanding = atof(argv[3]);
  if(argc > 4)
    url_base = argv[4];

  /* Get the length of the basename from the infile. */
  int infile_base_len;
  char *dotindex = rindex(infile, '.');
  if (dotindex == NULL)
    infile_base_len = strlen(infile);
  else infile_base_len = dotindex - infile;

  /* Set the url. */
  url = malloc(strlen(url_base) + 6 /* ?file= */ + strlen(infile) + 1 /* \0 */);
  sprintf(url, "%s?file=%s", url_base, infile);

  /* Calibrate all of the tscs. */
  calibrate_all_tscs();

  /* Start a pthread to dump the statistics. */
  pthread_t stats_thread;
  pthread_create(&stats_thread, NULL, print_stats, NULL);

  /* Actually get the thumbnails. */
  curl_global_init(CURL_GLOBAL_ALL);
  pthread_t threads[max_outstanding];
  struct thread_args targ[max_outstanding];
  for (int i=0; i< max_outstanding; i++) {
    targ[i].infile = infile;
    targ[i].infile_base_len = infile_base_len;
    targ[i].url = url;
    pthread_create(&threads[i], NULL, get_thumbnails, targ);
  }

  while (1) {
    int count = __sync_fetch_and_add(&num_ready, 1);
    futex_wakeup_one(&num_ready);
    usleep((useconds_t)(1000000/threads_per_second));
  }

  for (int i=0; i< max_outstanding; i++) {
    pthread_join(threads[i], NULL);
  }
  curl_global_cleanup();

  /* Cleanup allocated memory. */
  free(url);
  return 0;
}
