#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CONFIG_DIR ".config"
#define NOTES_DB "notes.db"
#define NOTE_DONE_COLUMN 3
#define NOTE_TEXT_LENGTH 1024
#define NOTE_MAX_DISPLAY_LENGTH 50
#define PATH_SEPARATOR "/"

static char
*get_db_path(void) {
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

	char *dest = (char *)malloc(total_len + 1 * sizeof(char));
	memset(dest, 0, total_len*sizeof(char));

	strncat(dest, home_dir, sizes[0]);
	strncat(dest, PATH_SEPARATOR, sizes[1]);
	strncat(dest, data_dir, sizes[2]);
	strncat(dest, PATH_SEPARATOR, sizes[3]);
	strncat(dest, NOTES_DB, sizes[4]);

	dest[total_len] = '\0';

	return dest;
}

static sqlite3
*open_db(void) {
	char *db_path = get_db_path();
	sqlite3 *db;
	if (sqlite3_open(db_path, &db) != SQLITE_OK) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		free(db_path);
		sqlite3_close(db);
		return NULL;
	}
	free(db_path);
	return db;
}

static void
create_table(sqlite3 *db) {
	char *sql = "CREATE TABLE IF NOT EXISTS notes(id INTEGER PRIMARY KEY "
		"AUTOINCREMENT, text TEXT, done INT);";

	char *err = 0;
	if (sqlite3_exec(db, sql, 0, 0, &err) != SQLITE_OK) {
		fprintf(stderr, "SQL Error: %s\n", err);
		sqlite3_free(err);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
}

static void
usage(char *program_name) {
	fprintf(stdout, "Usage: %s [OPTION]... [ARGUMENT]...\n", program_name);
	fprintf(stdout, "Simple note taking\n\n");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -a     add a new note\n");
	fprintf(stdout, "  -d id  delete note (specified by id)\n");
	fprintf(stdout, "  -e id  edit note (specified by id)\n");
	fprintf(stdout, "  -h     print this message\n");
	fprintf(stdout, "  -l     list all notes\n");
	fprintf(stdout, "  -m id  mark note (specified by id) as completed\n");
	fprintf(stdout, "  -v id  view note (specified by id)\n");
	fprintf(stdout, "\nIf no options are provided, the notes will be listed.\n");
}

static int
get_rows(void *pArg, int argc, char **argv, char **columNames) {
	if (argc != 1) return 0;

	char **ptr = argv;
	int *result = (int *)pArg;
	*result = atoi(*ptr);
	return 0;
}

static int
exists(sqlite3 *db, const char *id) {
	char sql[128];
	sprintf(sql, "SELECT count(*) FROM notes WHERE id = %s;", id);

	int numrows = 0;
	char *err = 0;
	if (sqlite3_exec(db, sql, get_rows, &numrows, &err) != SQLITE_OK) {
		fprintf(stderr, "SQL ERrror: %s\n", err);
		sqlite3_free(err);
		sqlite3_close(db);
		return -1;
	}
	return numrows;
}

static void
delete(const char *id) {
	sqlite3 *db = open_db();
	create_table(db);

	if (exists(db, id) == 0) {
		printf("Note %s not found.\n", id);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}

	char *sql = "DELETE FROM notes WHERE id = ?;";
	size_t size = strlen(sql) + strlen(id);
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, size, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		printf("Couldn't delete note.\n");
		exit(EXIT_FAILURE);
	}
	sqlite3_bind_int(stmt, 1, atoi(id));
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	printf("Note %s deleted.\n.", id);
}

static int
print_notes(void *pArg, int argc, char **argv, char **columNames) {
	char **cols = columNames;
	int numcols = 0;
	for (char *a = *cols; a; a = *++cols, numcols++)
		;
	numcols = numcols-argc;

	char **ptr = argv;
	int i = 0;
	for (char *c = *ptr; c; c = *++ptr, i++) {
		/* (Dirty) check to hide "done" column */
		if (strcmp(c, "1") == 0) {
			printf("\n");
			continue;
		}

		/* Truncate */
		size_t len = strlen(c);
		if (len >= NOTE_MAX_DISPLAY_LENGTH) {
			for (int j = NOTE_MAX_DISPLAY_LENGTH-3; j < NOTE_MAX_DISPLAY_LENGTH; j++)
				c[j] = '.';
			c[NOTE_MAX_DISPLAY_LENGTH] = '\0';
		}

		if (numcols == NOTE_DONE_COLUMN)
			printf("\e[9m%s\e[0m ", c);
		else
			printf("%s ", c);

		if (i == numcols-1) printf("\n");
	}

	return 0;
}

static void
list(void) {
	sqlite3 *db = open_db();
	create_table(db);

	char sql[64] = "SELECT count(*) FROM notes;";
	char *err1 = 0;
	int numrows = 0;
	if (sqlite3_exec(db, sql, get_rows, &numrows, &err1) != SQLITE_OK) {
		fprintf(stderr, "SQL Error: %s\n", err1);
		sqlite3_free(err1);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}

	if (numrows == 0) {
		printf("No notes where found.\n");
		printf("Try with the -h option for more information.\n");
	}

	memset(sql, 0, sizeof(sql));
	sprintf(sql,"SELECT id,text,done FROM notes;");

	char *err2 = 0;
	if (sqlite3_exec(db, sql, print_notes, 0, &err2) != SQLITE_OK) {
		fprintf(stderr, "SQL Error: %s\n", err2);
		sqlite3_free(err2);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
	sqlite3_close(db);
}

static int
print_note(void *pArg, int argc, char **argv, char **columNames) {
	char **ptr = argv;
	for (char *c = *ptr; c; c = *++ptr) {
		printf("%s ", c);
	}
	printf("\n");
	return 0;
}

static void
view(const char *id) {
	sqlite3 *db = open_db();
	create_table(db);

	if (exists(db, id) == 0) {
		printf("Note %s not found.\n", id);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}

	char sql[128];
	sprintf(sql, "SELECT text FROM notes WHERE id = %s;", id);

	char *err = 0;
	if (sqlite3_exec(db, sql, print_note, 0, &err) != SQLITE_OK) {
		fprintf(stderr, "SQL Error: %s\n", err);
		sqlite3_free(err);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
	sqlite3_close(db);
}

void
add(void) {
	char text[NOTE_TEXT_LENGTH];
	printf("(hit ENTER to finish):\n");
	fflush(stdout);
	if (fgets(text, NOTE_TEXT_LENGTH, stdin) == NULL)
		exit(EXIT_FAILURE);

	size_t len = strlen(text);
	if (len < 1) {
		exit(EXIT_FAILURE);
	}
	text[len-1] = '\0';

	sqlite3 *db = open_db();
	create_table(db);

	char *sql = "INSERT INTO notes(text) values(?);";
	size_t size = strlen(sql) + len;
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, size, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		printf("Couldn't add note.\n");
		exit(EXIT_FAILURE);
	}
	sqlite3_bind_text(stmt, 1, text, len, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	printf("Note added.\n");
}

void
mark(char *id) {
	sqlite3 *db = open_db();
	create_table(db);

	if (exists(db, id) == 0) {
		printf("Note %s not found.\n", id);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}

	char *sql = "UPDATE notes SET done = 1 WHERE id = ?;";
	size_t size = strlen(sql) + strlen(id);
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, size, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		printf("Couldn't mark note as complete.\n");
		exit(EXIT_FAILURE);
	}

	sqlite3_bind_int(stmt, 1, atoi(id));
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	printf("Note %s marked as completed.\n", id);
}

void
edit(char *id) {
	sqlite3 *db = open_db();
	create_table(db);

	if (exists(db, id) == 0) {
		printf("Note %s not found.\n", id);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}

	char text[NOTE_TEXT_LENGTH];
	printf("(hit ENTER to finish):\n");
	fflush(stdout);
	if (fgets(text, NOTE_TEXT_LENGTH, stdin) == NULL)
		exit(EXIT_FAILURE);

	size_t len = strlen(text);
	if (len < 1) {
		exit(EXIT_FAILURE);
	}
	text[len-1] = '\0';

	char *sql = "UPDATE notes SET text = ? WHERE id = ?;";
	size_t size = strlen(sql) + strlen(text) + strlen(id);
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, size, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		printf("Couldn't update note.\n");
		exit(EXIT_FAILURE);
	}

	sqlite3_bind_text(stmt, 1, text, strlen(text), NULL);
	sqlite3_bind_int(stmt, 2, atoi(id));
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	printf("Note %s updated.\n", id);
}

int
main(int argc, char **argv) {
	char *prg = argv[0];

	if (argc < 2) {
		list();
		exit(EXIT_SUCCESS);
	}

	int opt;
	while ((opt = getopt(argc, argv, "ad:e:hlm:v:")) != -1) {
		switch (opt) {
			case 'a': add(); break;
			case 'd': delete(optarg); break;
			case 'e': edit(optarg); break;
			case 'h': usage(prg); break;
			case 'l': list(); break;
			case 'm': mark(optarg); break;
			case 'v': view(optarg); break;
			default:
				fprintf(stderr, "Try '%s -h' for more information.\n", prg);
				break;
		}
	}

	return EXIT_SUCCESS;
}