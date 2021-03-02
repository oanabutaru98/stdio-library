#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <fcntl.h>
#include "so_stdio.h"

#define BUFFSIZE 4096

typedef struct _so_file {
	unsigned char buffer[BUFFSIZE];
	int file_descriptor;
	long file_pointer;
	int inside_buffer;
	int remaining_chars;
	int is_at_eof;
	int last_operation; // 0-read, 1-write
	int has_errors; //0-no, 1-yes
}_so_file;


SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *file = (SO_FILE *)calloc(1, sizeof(SO_FILE));

	if (file == NULL) {
		file->has_errors = 1;
		return NULL;
	}

	if (pathname == NULL)
		return NULL;
	if (mode == NULL)
		return NULL;

	/* check open mode */
	if (strcmp(mode, "r") == 0) {
		file->file_descriptor = open(pathname, O_RDONLY);
		if (file->file_descriptor < 0) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}

	if (strcmp(mode, "r+") == 0) {
		file->file_descriptor = open(pathname, O_RDWR);
		if (file->file_descriptor < 0) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}

	if (strcmp(mode, "w") == 0) {
		file->file_descriptor = open(pathname,
			O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (file->file_descriptor < 0) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}

	if (strcmp(mode, "w+") == 0) {
		file->file_descriptor = open(pathname,
			O_RDWR | O_CREAT | O_TRUNC, 0644);
		if (file->file_descriptor < 0) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}

	if (strcmp(mode, "a") == 0) {
		file->file_descriptor = open(pathname,
			O_WRONLY | O_APPEND | O_CREAT, 0644);
		if (file->file_descriptor < 0) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}

	if (strcmp(mode, "a+") == 0) {
		file->file_descriptor = open(pathname,
			O_RDWR | O_APPEND | O_CREAT, 0664);
		if (file->file_descriptor < 0) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}

	if (file != NULL)
		free(file);

	return NULL;
}

int so_fclose(SO_FILE *stream)
{
	int ret;

	/* last operation was write */
	if (stream->last_operation == 1) {
		ret = so_fflush(stream);
		if (ret < 0) {
			free(stream);
			return -1;
		}
	}

	/* close the file */
	ret = close(stream->file_descriptor);
	if (ret < 0) {
		stream->has_errors = 1;
		free(stream);
		return -1;
	}

	free(stream);
	return ret;
}

int so_fileno(SO_FILE *stream)
{
	return stream->file_descriptor;
}

int so_fflush(SO_FILE *stream)
{
	int bytes_written = 0;
	int bytes_written_now;

	/* start writing in the file */
	while (bytes_written < stream->inside_buffer) {
		bytes_written_now = write(stream->file_descriptor,
			stream->buffer + bytes_written,
			stream->inside_buffer - bytes_written);
		if (bytes_written_now <= 0) {
			stream->has_errors = 1;
			return -1;
		}
		bytes_written += bytes_written_now;
	}

	/* reset the pointer inside the buffer to 0 */
	stream->inside_buffer = 0;
	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int ret;

	/* last operation was write */
	if (stream->last_operation == 1)
		so_fflush(stream);

	ret = lseek(stream->file_descriptor, offset, whence);
	if (ret < 0) {
		stream->has_errors = 1;
		return -1;
	}

	/* reset buffer */
	memset(stream->buffer, 0, BUFFSIZE);
	stream->inside_buffer = 0;
	stream->remaining_chars = 0;

	stream->file_pointer = ret;

	return 0;
}

long so_ftell(SO_FILE *stream)
{
	long ret = stream->file_pointer;

	if (ret < 0) {
		stream->has_errors = 1;
		return -1;
	}
	return ret;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int i;
	int c;

	for (i = 0; i < nmemb * size; i++) {
		if (stream->is_at_eof == 0) {
			c = so_fgetc(stream);
			((unsigned char *)ptr)[i] = (unsigned char)c;
		} else
			return i/size - 1;
	}

	return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int i;
	int c = 0;

	for (i = 0; i < nmemb * size; i++) {
		c = so_fputc(((unsigned char *)ptr)[i], stream);
		if (c < 0) {
			stream->has_errors = 1;
			return -1;
		}
	}

	return nmemb;
}

int so_fgetc(SO_FILE *stream)
{
	char returned_character;
	int returned_character_converted;
	int bytes_read;

	/* last operation will be read */
	stream->last_operation = 0;
	/* no more chars in the buffer */
	if (stream->remaining_chars <= 0) {
		/* check for the EOF */
		if (so_feof(stream) == 1) {
			stream->has_errors = 1;
			return SO_EOF;
		}

		/* reset buffer and read new chars */
		memset(stream->buffer, 0, BUFFSIZE);
		bytes_read = read(stream->file_descriptor,
			stream->buffer, BUFFSIZE);
		stream->remaining_chars = bytes_read;
		if (bytes_read <= 0) {
			stream->has_errors = 1;
			stream->is_at_eof = 1;
			return SO_EOF;
		}

		stream->file_pointer++;

		stream->inside_buffer = 1;
		stream->remaining_chars--;
		returned_character = stream->buffer[0];
		returned_character_converted = (int)returned_character;

		return returned_character_converted;
	}

	stream->file_pointer++;

	stream->inside_buffer++;
	stream->remaining_chars--;
	returned_character = stream->buffer[stream->inside_buffer - 1];
	returned_character_converted = (int)returned_character;

	return returned_character_converted;
}

int so_fputc(int c, SO_FILE *stream)
{
	stream->file_pointer++;

	/* last operation will be write */
	stream->last_operation = 1;

	/* check if the buffer is full */
	if (stream->inside_buffer >= 4096)
		so_fflush(stream);

	stream->buffer[stream->inside_buffer % 4096] = (unsigned char)c;
	stream->inside_buffer++;
	return c;
}

int so_feof(SO_FILE *stream)
{
	if (stream->is_at_eof == 1)
		return 1;
	else
		return 0;
}

int so_ferror(SO_FILE *stream)
{
	if (stream->has_errors == 1)
		return 1;
	else
		return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	return NULL;
}

int so_pclose(SO_FILE *stream)
{
	return 0;
}
