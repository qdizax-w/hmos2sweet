/*
 * lpwrite: write an image into a logical partition of the "super" partition,
 * straight through the LP metadata extents, without device-mapper.
 *
 * Used from the recovery (adb shell) when fastbootd refuses to write a freshly
 * created logical partition ("No such file or directory"). The partition must
 * already exist with a size >= the image (fastboot create-logical-partition +
 * resize-logical-partition).
 *
 *   lpwrite <super block device> <partition> <image>
 *
 * License: GPL-2.0
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LP_RESERVED 4096
#define LP_GEOMETRY_MAGIC 0x616c4467u
#define LP_HEADER_MAGIC 0x414C5030u

#pragma pack(push, 1)
struct lp_geometry {
	uint32_t magic, struct_size;
	uint8_t checksum[32];
	uint32_t metadata_max_size, metadata_slot_count, logical_block_size;
};
struct lp_table_desc { uint32_t offset, num_entries, entry_size; };
struct lp_header {
	uint32_t magic;
	uint16_t major, minor;
	uint32_t header_size;
	uint8_t header_checksum[32];
	uint32_t tables_size;
	uint8_t tables_checksum[32];
	struct lp_table_desc partitions, extents, groups, block_devices;
};
struct lp_partition {
	char name[36];
	uint32_t attributes, first_extent_index, num_extents, group_index;
};
struct lp_extent {
	uint64_t num_sectors;
	uint32_t target_type;
	uint64_t target_data;
	uint32_t target_source;
};
#pragma pack(pop)

#define CHUNK (4 << 20)

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "usage: %s <super> <partition> <image>\n", argv[0]);
		return 2;
	}
	int sfd = open(argv[1], O_RDWR | O_SYNC);
	if (sfd < 0) {
		perror(argv[1]);
		return 1;
	}
	struct lp_geometry g;
	if (pread(sfd, &g, sizeof(g), LP_RESERVED) != sizeof(g) || g.magic != LP_GEOMETRY_MAGIC) {
		fprintf(stderr, "no LP geometry on %s\n", argv[1]);
		return 1;
	}
	char *meta = malloc(g.metadata_max_size);
	if (!meta || pread(sfd, meta, g.metadata_max_size, LP_RESERVED + 2 * 4096) != (ssize_t)g.metadata_max_size) {
		fprintf(stderr, "cannot read the LP metadata\n");
		return 1;
	}
	struct lp_header *h = (struct lp_header *)meta;
	if (h->magic != LP_HEADER_MAGIC) {
		fprintf(stderr, "bad LP header\n");
		return 1;
	}
	char *tables = meta + h->header_size;
	struct lp_partition *pt = NULL;
	for (uint32_t i = 0; i < h->partitions.num_entries; i++) {
		struct lp_partition *p = (struct lp_partition *)(tables + h->partitions.offset + i * h->partitions.entry_size);
		if (!strncmp(p->name, argv[2], 36)) {
			pt = p;
			break;
		}
	}
	if (!pt) {
		fprintf(stderr, "partition %s not found in super\n", argv[2]);
		return 1;
	}
	int ifd = open(argv[3], O_RDONLY);
	struct stat st;
	if (ifd < 0 || fstat(ifd, &st)) {
		perror(argv[3]);
		return 1;
	}
	uint64_t total = 0;
	for (uint32_t k = 0; k < pt->num_extents; k++) {
		struct lp_extent *e = (struct lp_extent *)(tables + h->extents.offset +
							   (pt->first_extent_index + k) * h->extents.entry_size);
		total += e->num_sectors * 512;
	}
	printf("%s: %u extents, %llu bytes; image %lld bytes\n", argv[2], pt->num_extents,
	       (unsigned long long)total, (long long)st.st_size);
	if ((uint64_t)st.st_size > total) {
		fprintf(stderr, "the partition is smaller than the image: resize it first\n");
		return 1;
	}
	char *buf = malloc(CHUNK), *chk = malloc(CHUNK);
	uint64_t done = 0;
	for (uint32_t k = 0; k < pt->num_extents && done < (uint64_t)st.st_size; k++) {
		struct lp_extent *e = (struct lp_extent *)(tables + h->extents.offset +
							   (pt->first_extent_index + k) * h->extents.entry_size);
		if (e->target_type != 0) { /* zero extent */
			continue;
		}
		uint64_t base = e->target_data * 512, len = e->num_sectors * 512, off = 0;
		while (off < len && done < (uint64_t)st.st_size) {
			size_t n = CHUNK;
			if (n > len - off)
				n = len - off;
			if (n > st.st_size - done)
				n = st.st_size - done;
			if (pread(ifd, buf, n, done) != (ssize_t)n) {
				perror("read image");
				return 1;
			}
			if (pwrite(sfd, buf, n, base + off) != (ssize_t)n) {
				perror("write super");
				return 1;
			}
			if (pread(sfd, chk, n, base + off) != (ssize_t)n || memcmp(buf, chk, n)) {
				fprintf(stderr, "verify failed at %llu\n", (unsigned long long)done);
				return 1;
			}
			off += n;
			done += n;
		}
	}
	fsync(sfd);
	printf("%s: %llu bytes written and verified\n", argv[2], (unsigned long long)done);
	return done == (uint64_t)st.st_size ? 0 : 1;
}
