#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <zstd.h>

#define TEMP_TEMPLATE "/tmp/pd-mapperXXXXXX"

int zstd_decomp(const char *file)
{
	int return_fd;
	char temp_file[sizeof(TEMP_TEMPLATE)];
	struct stat sb;
	void *src_buf;
	void *dst_buf;
	size_t src_size;
	size_t dst_capacity;
	size_t decomp_size;
	int fd;
	int ret;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "%s: Error opening input file: %s\n", file, strerror(errno));
		return -1;
	}

	if (fstat(fd, &sb) < 0) {
		close(fd);
		return -1;
	}

	src_size = sb.st_size;
	src_buf = malloc(src_size);
	if (!src_buf) {
		close(fd);
		return -1;
	}

	ret = read(fd, src_buf, src_size);
	close(fd);
	if (ret != src_size) {
		free(src_buf);
		return -1;
	}

	// Determine decompressed size, default to 1MB if unknown
	unsigned long long content_size = ZSTD_getFrameContentSize(src_buf, src_size);
	if (content_size == ZSTD_CONTENTSIZE_ERROR || content_size == ZSTD_CONTENTSIZE_UNKNOWN) {
		dst_capacity = 1024 * 1024; // 1MB fallback
	} else {
		dst_capacity = content_size;
	}

	dst_buf = malloc(dst_capacity);
	if (!dst_buf) {
		free(src_buf);
		return -1;
	}

	decomp_size = ZSTD_decompress(dst_buf, dst_capacity, src_buf, src_size);
	free(src_buf);

	if (ZSTD_isError(decomp_size)) {
		fprintf(stderr, "%s: ZSTD decoder error: %s\n", file, ZSTD_getErrorName(decomp_size));
		free(dst_buf);
		return -1;
	}

	strcpy(temp_file, TEMP_TEMPLATE);
	return_fd = mkstemp(temp_file);
	if (return_fd < 0) {
		free(dst_buf);
		return -1;
	}
	unlink(temp_file);

	ret = write(return_fd, dst_buf, decomp_size);
	free(dst_buf);

	if (ret != decomp_size) {
		close(return_fd);
		return -1;
	}

	lseek(return_fd, 0, SEEK_SET);
	return return_fd;
}
