#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <windows.h>
#define DLL_EXPORTS ".c"
#include "so_stdio.h"

#define BUFFSIZE 4096

typedef struct _so_file {
	unsigned char buffer[BUFFSIZE];
	long file_pointer;
	HANDLE file_descriptor;
	int remaining_chars;
	int is_at_eof;
	int inside_buffer;
	int last_operation;
	int has_errors;
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
		
	
	/* check cases according to mode */
	if (strcmp(mode, "r") == 0) {
		file->file_descriptor = CreateFile(pathname,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (file->file_descriptor == INVALID_HANDLE_VALUE) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else 
			return file;
	}
	
	if (strcmp(mode, "r+") == 0) {
		file->file_descriptor = CreateFile(pathname,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		
		if (file->file_descriptor == INVALID_HANDLE_VALUE) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}
	
	if (strcmp(mode, "w") == 0) {
		file->file_descriptor = CreateFile(pathname,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		
		if (file->file_descriptor == INVALID_HANDLE_VALUE) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}
	
	if (strcmp(mode, "w+") == 0) {
		file->file_descriptor = CreateFile(pathname,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ |
			FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		
		if (file->file_descriptor == INVALID_HANDLE_VALUE) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}
	
	if (strcmp(mode, "a") == 0) {
		file->file_descriptor = CreateFile(pathname,
			FILE_APPEND_DATA,
			FILE_SHARE_READ |
			FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		
		if (file->file_descriptor == INVALID_HANDLE_VALUE) {
			file->has_errors = 1;
			free(file);
			return NULL;
		} else
			return file;
	}
	
	if (strcmp(mode, "a+") == 0) {
		file->file_descriptor = CreateFile(pathname,
			GENERIC_READ | FILE_APPEND_DATA,
			FILE_SHARE_READ |
			FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		
		if (file->file_descriptor == INVALID_HANDLE_VALUE) {
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
	BOOL bRet;
	int ret;
	
	/* last operation was write */
	if (stream->last_operation == 1) {
		/* write in file and clear buffer */
		ret = so_fflush(stream);
		if (ret < 0) {
			free(stream);
			return -1;
		}
	}
	
	/* close file */
	bRet = CloseHandle(stream->file_descriptor);
	if (bRet == FALSE) {
		stream->has_errors = 1;
		free(stream);
		return -1;
	}
	
	free(stream);
	return 0; 
}

HANDLE so_fileno(SO_FILE *stream)
{
	return stream->file_descriptor;
}


int so_fflush(SO_FILE *stream)
{
	int bytes_written = 0;
	int bytes_written_now;
	BOOL bRet;

	/* actually write in file */
	while (bytes_written < stream->inside_buffer) {
		bRet = WriteFile(stream->file_descriptor,
			stream->buffer + bytes_written,
			stream->inside_buffer - bytes_written,
			&bytes_written_now,
			NULL);
		
		if (bRet == FALSE) {
			stream->has_errors = 1;
			return -1;
		}
						
		if (bytes_written_now <= 0) {
			stream->has_errors = 1;
			return -1;
		} 
		
		bytes_written += bytes_written_now; 
	}
	
	stream->inside_buffer = 0;
	
	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int ret;

	if (stream->last_operation == 1)
		so_fflush(stream);

	/* change file pointer */
	ret = SetFilePointer(stream->file_descriptor,
		offset,
		NULL,
		whence);						
	
	if (ret < 0) {
		stream->has_errors = 1;
		return -1;
	}
	
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
			return i / size - 1;
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
	BOOL bRet;

	stream->last_operation = 0;
	
	/* check if the buffer is empty */
	if (stream->remaining_chars <= 0) {
		if (so_feof(stream) == 1) {
			stream->has_errors = 1;
			return SO_EOF;
		}

		/* reset buffer */
		memset(stream->buffer, 0, BUFFSIZE);

		bRet = ReadFile(stream->file_descriptor,
			stream->buffer,
			BUFFSIZE,
			&bytes_read,
			NULL);
						
		if (bRet == FALSE) {
			stream->has_errors = 1;
			return -1;
		}
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

	/* last operation was write */
	stream->last_operation = 1;
	
	if (stream->inside_buffer >= 4096)
		so_fflush(stream);

	stream->buffer[stream->inside_buffer] = (char)c;
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
