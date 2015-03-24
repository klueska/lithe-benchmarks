#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <Epeg.h>

#define num_thumbnails (sizeof(sizes)/sizeof(sizes[0]))
const int quality = 100;
const int sizes[] = { 75, 100, 125, 150, 200, 400, 600 };
char *infilename = NULL;
char *outfilename = NULL;

struct thumbnail_data {
	char *infilebase;
	unsigned char *instream;
	int insize;
	unsigned char *outstream;
	int outsize;
	int w;
	int h;
	int quality;
	char filename[256];
};

static void *gen_thumbnail(void *arg)
{
	struct thumbnail_data *td = (struct thumbnail_data*)arg;
	sprintf(td->filename, "%s-%dx%d.jpg", td->infilebase, td->w, td->h);
	Epeg_Image *input = epeg_memory_open(td->instream, td->insize);
	epeg_decode_size_set(input, td->w, td->h);
	epeg_quality_set(input, td->quality);
	epeg_memory_output_set(input, &td->outstream, &td->outsize);
	epeg_encode(input);
	epeg_close(input);
}

void *write_archive(int fd, struct thumbnail_data *td)
{
	struct archive *a;
	struct archive_entry *entry;

	a = archive_write_new();
	archive_write_add_filter_gzip(a);
	archive_write_set_format_pax_restricted(a);
	archive_write_open_fd(a, fd);
	for (int i = 0; i < num_thumbnails; i++) {
		entry = archive_entry_new();
		archive_entry_set_pathname(entry, td[i].filename);
		archive_entry_set_size(entry, td[i].outsize);
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_perm(entry, 0644);
		archive_write_header(a, entry);
		archive_write_data(a, td[i].outstream, td[i].outsize);
		archive_entry_free(entry);
	}
	archive_write_close(a);
	archive_write_free(a);
}

int main(int argc, char **argv)
{
	struct thumbnail_data td[num_thumbnails];
	pthread_t threads[num_thumbnails];

	int ch;
 	while ((ch = getopt(argc, argv, "i:")) != -1) {
		switch (ch) {
		case 'i':
				infilename = optarg;
				break;
		default:
				exit (0);
		}
	}

	/* Read the entire input file into memory. */
	int infd = open(infilename, O_RDONLY);
	struct stat st;
	fstat(infd, &st);
	void *instream = malloc(st.st_size);
	read(infd, instream, st.st_size);

	/* Truncate the extension from the intpufilename to get the base name. */
	char *dotindex = rindex(infilename, '.');
	if (dotindex != NULL)
		*dotindex = '\0';

	/* Create the thumbnails. */
	for (int i = 0; i < num_thumbnails; i++) {
		td[i].infilebase = infilename;
		td[i].instream = instream;
		td[i].insize = st.st_size;
		td[i].outstream = NULL;
		td[i].outsize = 0;
		td[i].w = sizes[i];
		td[i].h = sizes[i];
		td[i].quality = quality;
		pthread_create(&threads[i], NULL, gen_thumbnail, &td[i]);
	}
	for (int i = 0; i < num_thumbnails; i++) {
		pthread_join(threads[i], NULL);
	}
	close(infd);

	/* Write out an archive of all the thumbnails to a file. */
	outfilename = malloc(strlen(infilename) + strlen("thumbnails.tgz") + 1);
	sprintf(outfilename, "%s-%s", infilename, "thumbnails.tgz");
	int outfd = open(outfilename, O_WRONLY | O_CREAT, 0644);
	write_archive(outfd, td);
	free(outfilename);
	close(outfd);
	return 0;
}

