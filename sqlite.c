#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CONFIG_DIR ".config"
#define NOTES_DB "notes.db"
#define NOTES_DIR "notes"
#define NOTE_DETAILS_LENGTH 80
#define NOTE_TITLE_LENGTH 60
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

sqlite3 *open_db() {
	char *db_path = get_db_path();

	sqlite3 *db;
	int rc = sqlite3_open(db_path, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "can't open database: %s\n", sqlite3_errmsg(db));
		free(db_path);
		sqlite3_close(db);
		return NULL;
	}

	free(db_path);

	return db;
}

int create_table(sqlite3 *db) {
	char *sql = "CREATE TABLE IF NOT EXISTS notes(id INTEGER PRIMARY KEY "
		"AUTOINCREMENT, title TEXT, body TEXT, date TEXT, done INT, label TEXT);";

	char *err_msg = 0;
	int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sql error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return -1;
	}

	return 0;
}

void print_usage(char *program_name) {
	fprintf(stderr, "Usage: %s [OPTION]... [ARGUMENT]...\n", program_name);
	fprintf(stderr, "  -a/--add        add a new note\n");
	fprintf(stderr, "  -d/--delete id  delete note specified by id\n");
	fprintf(stderr, "  -v/--view   id  view note specified by id\n");
	fprintf(stderr, "  -l/--list       list all notes\n");
}

int delete_callback(void *pArg, int argc, char **argv, char **columNames) {
	if (argc != 1) return 0;

	char **ptr = argv;
	int *result = (int *)pArg;
	*result = atoi(*ptr);

	return 0;
}

void delete(const char *id) {
	sqlite3 *db = open_db();
	int rc = create_table(db);
	if (rc == -1) {
		exit(EXIT_FAILURE);
	}

	char sql[128];
	sprintf(sql, "SELECT count(*) FROM notes WHERE id = %s;", id);

	char *err_msg1 = 0;
	int numrows = 0;
	rc = sqlite3_exec(db, sql, delete_callback, &numrows, &err_msg1);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sql error: %s\n", err_msg1);
		sqlite3_free(err_msg1);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}

	if (numrows == 0) {
		printf("error: no notes with the id: %s where found\n", id);
		exit(EXIT_FAILURE);
	}

	memset(sql, 0, sizeof(sql));
	sprintf(sql, "DELETE FROM notes WHERE id = %s;", id);

	char *err_msg2 = 0;
	rc = sqlite3_exec(db, sql, 0, 0, &err_msg2);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sql error: %s\n", err_msg2);
		sqlite3_free(err_msg2);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
	printf("note %s deleted\n.", id);
	sqlite3_close(db);
}

int list_callback(void *pArg, int argc, char **argv, char **columNames) {
	char **ptr = argv;
	int i = 0;
	for (char *c = *ptr; c; c = *++ptr, i++) {
		printf("%s ", c);
		if (i == argc - 1) printf("\n");
	}

	return 0;
}

void list(void) {
	sqlite3 *db = open_db();
	int rc = create_table(db);
	if (rc == -1) {
		exit(EXIT_FAILURE);
	}

	char *sql = "SELECT id,title FROM notes;";
	char *err_msg = 0;
	rc = sqlite3_exec(db, sql, list_callback, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sql error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
	sqlite3_close(db);
}

void view(const char *id) {
	sqlite3 *db = open_db();
	int rc = create_table(db);
	if (rc == -1)
		exit(EXIT_FAILURE);

	char sql[128];
	sprintf(sql, "SELECT title,body FROM notes WHERE id = %s;", id);
	char *err_msg = 0;
	rc = sqlite3_exec(db, sql, list_callback, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sql error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
	sqlite3_close(db);
}

void add(void) {
	char title[NOTE_TITLE_LENGTH];
	printf("enter a title: ");
	fflush(stdout);
	if (fgets(title, NOTE_TITLE_LENGTH, stdin) == NULL)
		exit(EXIT_FAILURE);

	size_t len = strlen(title);
	if (len < 1) {
		exit(EXIT_FAILURE);
	}
	title[len-1] = '\0';

	char details[NOTE_DETAILS_LENGTH];
	printf("enter details: ");
	fflush(stdout);
	if (fgets(details, NOTE_DETAILS_LENGTH, stdin) == NULL)
		exit(EXIT_FAILURE);

	len = strlen(details);
	if (len < 1) {
		exit(EXIT_FAILURE);
	}
	details[len-1] = '\0';

	sqlite3 *db = open_db();
	int rc = create_table(db);
	if (rc == -1)
		exit(EXIT_FAILURE);

	char sql[256];
	sprintf(sql, "INSERT INTO notes(title, body) values('%s', '%s');",
			title, details);

	char *err_msg = 0;
	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sql error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
	sqlite3_close(db);

	printf("note added\n");
}

int main(int argc, char **argv) {
	if (argc < 2) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	int opt;
	while ((opt = getopt(argc, argv, "ad:v:l")) != -1) {
		switch (opt) {
			case 'a':
				add();
				break;
			case 'd':
				delete(optarg);
				break;
			case 'l':
				list();
				break;
			case 'v':
				view(optarg);
				break;
			default:
				print_usage(argv[0]);
				break;
		}
	}

	return EXIT_SUCCESS;
}