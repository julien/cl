#include <sqlite3.h>
#include <getopt.h>
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
	char *sql = "CREATE TABLE IF NOT EXISTS notes(id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"title TEXT, body TEXT, date TEXT, done INT, label TEXT);";

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
	fprintf(stderr, "usage: %s [options]\n", program_name);
	fprintf(stderr, "  -a/--add.........add a new note\n");
	fprintf(stderr, "  -d/--delete id...delete note (specified by id)\n");
	fprintf(stderr, "  -v/--view id.....view note (specified by id)\n");
	fprintf(stderr, "  -l/--list........list all notes\n");
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

void list() {
	puts("listing notes:");

	sqlite3 *db = open_db();
	int rc = create_table(db);
	if (rc == -1) {
		exit(EXIT_FAILURE);
	}

	char *sql = "SELECT id,title from notes;";
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

int main(int argc, char **argv) {
	int c;
	static int id = 0;

	static struct option long_options[] = {
		{"add",    required_argument, 0, 'a'},
		{"delete", required_argument, 0, 'd'},
		{"view",   required_argument, 0, 'v'},
		{"list",   no_argument      , 0, 'l'},
		{0, 0, 0, 0},
	};

	/* while(1) { */

		int option_index = 0;
		c = getopt_long(argc, argv, "adv:l:", long_options, &option_index);

		if (c == -1) {
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}

		switch(c) {
			/* case 0: */
			/* 	if (optarg) */
			/* 		printf(" with arg %s", optarg); */
			/* 	break; */


			case 'd':
				{
					if (!argv[optind]) {
						printf("nopes\n");
						break;
					}
					printf("option -d with value %s\n", argv[optind]);
					puts("warning: you're about to delete a note");

					sqlite3 *db = open_db();
					int rc = create_table(db);
					if (rc == -1) {
						exit(EXIT_FAILURE);
					}

					char *noteid = argv[optind];
					char stmt[60];
					sprintf(stmt, "DELETE FROM notes WHERE ID = %s;", noteid);
					char *err_msg = 0;
					rc = sqlite3_exec(db, stmt, 0, 0, &err_msg);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "sql error: %s\n", err_msg);
						sqlite3_free(err_msg);
						sqlite3_close(db);
						return -1;
					}
					printf("note %s deleted\n.", noteid);
					sqlite3_close(db);
					exit(EXIT_SUCCESS);
				}
				break;

			case 'l':
				list();
				break;

			/* default: */
			/* 	break; */
		}
	/* } */

	/* if (optind < argc) { */
	/* 	printf("non-option argv elements: "); */
	/* 	while (optind < argc) */
	/* 		printf("%s ", argv[optind++]); */
	/* 	putchar('\n'); */
	/* } */

	return EXIT_SUCCESS;
}