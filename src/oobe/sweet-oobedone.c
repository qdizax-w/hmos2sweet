/*
 * sweet-oobedone: mark the HarmonyOS first-boot setup (OOBE) as finished.
 *
 * The setup wizard (com.huawei.hmos.hwstartupguide) is left out of the sweet
 * system image. SceneBoard starts it whenever the settings say the device is
 * not provisioned, so write the keys the wizard sets when it finishes:
 *   device_provisioned = 1, user_setup_complete = 1, basic_statement_agreed = 1
 * into every settings table of every settingsdata database found under /data.
 *
 * Runs as root at post-fs-data, before the settings data ability opens the
 * database; ownership and SELinux label of anything sqlite creates are copied
 * from the database file so the settings app can still open it.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#include "sqlite3.h"

static const char *KEYS[][2] = {
	{"device_provisioned", "1"},
	{"user_setup_complete", "1"},
	{"basic_statement_agreed", "1"},
};

static void klog(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	int n = snprintf(buf, sizeof(buf), "<6>sweet-oobedone: ");
	va_start(ap, fmt);
	n += vsnprintf(buf + n, sizeof(buf) - n - 2, fmt, ap);
	va_end(ap);
	buf[n++] = '\n';
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	if (fd >= 0) {
		if (write(fd, buf, n) < 0) {
		}
		close(fd);
	}
}

static int has_keyword_column(sqlite3 *db, const char *table)
{
	char sql[256];
	sqlite3_stmt *st;
	int found = 0;
	snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", table);
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
		return 0;
	while (sqlite3_step(st) == SQLITE_ROW) {
		const char *col = (const char *)sqlite3_column_text(st, 1);
		if (col && !strcasecmp(col, "KEYWORD"))
			found |= 1;
		if (col && !strcasecmp(col, "VALUE"))
			found |= 2;
	}
	sqlite3_finalize(st);
	return found == 3;
}

static void set_key(sqlite3 *db, const char *table, const char *key, const char *val)
{
	char sql[256];
	sqlite3_stmt *st;
	snprintf(sql, sizeof(sql), "UPDATE \"%s\" SET VALUE=?1 WHERE KEYWORD=?2", table);
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
		klog("%s: %s", table, sqlite3_errmsg(db));
		return;
	}
	sqlite3_bind_text(st, 1, val, -1, SQLITE_STATIC);
	sqlite3_bind_text(st, 2, key, -1, SQLITE_STATIC);
	sqlite3_step(st);
	sqlite3_finalize(st);
	if (sqlite3_changes(db) > 0) {
		klog("%s: %s = %s (updated)", table, key, val);
		return;
	}
	snprintf(sql, sizeof(sql), "INSERT INTO \"%s\"(KEYWORD, VALUE) VALUES(?1, ?2)", table);
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
		klog("%s: %s", table, sqlite3_errmsg(db));
		return;
	}
	sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
	sqlite3_bind_text(st, 2, val, -1, SQLITE_STATIC);
	int r = sqlite3_step(st);
	sqlite3_finalize(st);
	klog("%s: %s = %s (%s)", table, key, val, r == SQLITE_DONE ? "inserted" : sqlite3_errmsg(db));
}

static void fix_owner(const char *path, const struct stat *ref, const char *label)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return;
	if (chown(path, ref->st_uid, ref->st_gid) != 0) {
	}
	chmod(path, ref->st_mode & 07777);
	if (label[0])
		setxattr(path, "security.selinux", label, strlen(label) + 1, 0);
}

static void patch_db(const char *path)
{
	struct stat ref;
	char label[256] = "";
	char side[600];
	sqlite3 *db;

	if (stat(path, &ref) != 0)
		return;
	ssize_t ln = getxattr(path, "security.selinux", label, sizeof(label) - 1);
	label[ln > 0 ? ln : 0] = 0;

	if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
		klog("%s: open failed: %s", path, sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}
	sqlite3_busy_timeout(db, 3000);

	const char *tables[64];
	char names[64][64];
	int nt = 0;
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table'", -1, &st, NULL) != SQLITE_OK) {
		klog("%s: not a readable database: %s", path, sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}
	while (sqlite3_step(st) == SQLITE_ROW && nt < 64) {
		const char *n = (const char *)sqlite3_column_text(st, 0);
		if (n && strcasestr(n, "SETTINGSDATA")) {
			snprintf(names[nt], sizeof(names[nt]), "%s", n);
			tables[nt] = names[nt];
			nt++;
		}
	}
	sqlite3_finalize(st);
	klog("%s: %d settings tables", path, nt);

	sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
	for (int t = 0; t < nt; t++) {
		if (!has_keyword_column(db, tables[t]))
			continue;
		for (unsigned k = 0; k < sizeof(KEYS) / sizeof(KEYS[0]); k++)
			set_key(db, tables[t], KEYS[k][0], KEYS[k][1]);
	}
	sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	sqlite3_exec(db, "PRAGMA wal_checkpoint(TRUNCATE)", NULL, NULL, NULL);
	sqlite3_close(db);

	fix_owner(path, &ref, label);
	snprintf(side, sizeof(side), "%s-wal", path);
	fix_owner(side, &ref, label);
	snprintf(side, sizeof(side), "%s-shm", path);
	fix_owner(side, &ref, label);
	snprintf(side, sizeof(side), "%s-journal", path);
	fix_owner(side, &ref, label);
}

static int found;

static void walk(const char *dir, int depth)
{
	DIR *d = opendir(dir);
	struct dirent *de;
	char p[600];
	if (!d)
		return;
	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] == '.')
			continue;
		snprintf(p, sizeof(p), "%s/%s", dir, de->d_name);
		struct stat st;
		if (lstat(p, &st) != 0)
			continue;
		if (S_ISDIR(st.st_mode)) {
			if (depth < 8)
				walk(p, depth + 1);
		} else if (S_ISREG(st.st_mode) && strstr(de->d_name, ".db") &&
			   !strstr(de->d_name, "-wal") && !strstr(de->d_name, "-shm") &&
			   strcasestr(p, "settingsdata")) {
			found++;
			patch_db(p);
		}
	}
	closedir(d);
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		for (int i = 1; i < argc; i++)
			patch_db(argv[i]);
		return 0;
	}
	const char *roots[] = {"/data/app/el1", "/data/app/el2", "/data/service/el1", "/data/service/el2"};
	for (unsigned i = 0; i < sizeof(roots) / sizeof(roots[0]); i++)
		walk(roots[i], 0);
	klog("done, %d settingsdata databases", found);
	return 0;
}
