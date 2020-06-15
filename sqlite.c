#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_DIR ".config"
#define NOTES_DB "notes.db"
#define NOTES_DIR "notes"
#define PATH_SEPARATOR "/"

char *get_db_path() {
	unsigned int sizes[5];
	unsigned int sizes_len = sizeof(sizes) / sizeof(sizes[0]);

	char *home_dir = getenv("HOME");
	if (home_dir == NULL) {
		home_dir = "";
	}
	sizes[0] = strlen(home_dir);
	sizes[1] = strlen(PATH_SEPARATOR);

	char *data_dir = getenv("XDG_DATA_DIR");
	if (data_dir == NULL) {
		data_dir = CONFIG_DIR;
	}
	sizes[2] = strlen(data_dir);
	sizes[3] = strlen(PATH_SEPARATOR);
	sizes[4] = strlen(NOTES_DB);

	unsigned int total_len = 0;
	for (unsigned int i = 0; i < sizes_len; i++) {
		total_len += sizes[i];
	}

	char *dest = (char *)malloc(total_len * sizeof(char));
	strncat(dest, home_dir, sizes[0]);
	strncat(dest, PATH_SEPARATOR, sizes[1]);
	strncat(dest, data_dir, sizes[2]);
	strncat(dest, PATH_SEPARATOR, sizes[3]);
	strncat(dest, NOTES_DB, sizes[4]);
	return dest;
}

int main(void) {

	// TODO
	// Options
	//        -a   : add note
	//        -d ID: delete note
	//        -e ID: edit note
	//        -l   : list all notes
	//        -v ID: view note

	char *db_path = get_db_path();

	sqlite3 *db;
	int rc = sqlite3_open(db_path, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "can't open database: %s\n", sqlite3_errmsg(db));
		free(db_path);
		sqlite3_close(db);
		return 1;
	}

	sqlite3_stmt *res;

	char *sql = "DROP TABLE IF EXISTS notes;"
							"CREATE TABLE notes(id INTEGER PRIMARY KEY AUTOINCREMENT,"
							"title TEXT, body TEXT, date TEXT, done INT, label TEXT);";

	char *err_msg = 0;
	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sql error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return 1;
	}

	free(db_path);
	sqlite3_close(db);

	return 0;
}