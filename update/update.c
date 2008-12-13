#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#ifndef EXIT_SUCCESS
	#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
	#define EXIT_FAILURE -1
#endif

/* Need somewhere to store data as we parse,
 * because order is not guarenteed */
struct Package {
	char package       [ 50]; /* In Ubuntu, largest is 41 */
	char version       [ 50]; /* Plenty large */
	char section       [ 50]; /* Plenty large */
	char md5           [ 32]; /* MD5s are 32 characters */
	char maintainer    [100]; /* In Ubuntu, largest is 78 */
	char remote_path   [255]; /* In Ubuntu, largest subpath is 106 */
	char homepage      [255]; /* URLs are specced to a max length of 255 */
	char description   [300]; /* In Ubuntu, lagest is < 100 */
	int installed_size      ;
	int size                ;
};

int quotecat(char * dst, char * src, size_t n, int comma) {
	size_t i;
	size_t offset = strlen(dst)+1;
	dst[offset-1] = '\'';
	for(i = 0; i < (n-offset-3) && src[i] != '\0'; i++) {
		if(src[i] == '\'') {
			if(offset + i + 2 > n) break;
			dst[offset + i] = src[i];
			offset++;
			dst[offset + i] = '\'';
		} else {
			dst[offset + i] = src[i];
		}
	}
	dst[offset + i] = '\'';
	if(comma) dst[offset + i + 1] = ',';
	dst[offset + i + 2] = '\0';
	if(offset + i + 3 > n) return -1;
	return 0;
}

int main(int argc, char ** argv) {
	char line[300]; /* No line will be larger than the largest field */
	char sql[1500] = "\0";
	char * sep;
	char * endcomma;
	sqlite3 * db = NULL;
	struct Package current = {"","","","","","","","",0,0};
	int code = 0;

	/* TODO: getopt for specifying db (not to do VACCUM?)
	 *       UPDATE queries
	 *       dependency parsing
	 *       SQL output
	 */

	/* Open database */
	if(sqlite3_open("test.db", &db) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	/* Do everything as one transaction. Many times faster */
	if(sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	/* Create tables if they do not exist */
	if(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS packages (package TEXT PRIMARY KEY, version TEXT, maintainer TEXT, installed_size INTEGER, size INTEGER, homepage TEXT, section TEXT, remote_path TEXT, md5 TEXT, description TEXT, status INTEGER);", NULL, NULL, NULL) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
	if(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS depends (package TEXT, depend TEXT, version TEXT);", NULL, NULL, NULL) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	if(sqlite3_exec(db, "DELETE FROM depends;", NULL, NULL, NULL) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	/* Loop over lines from stream */
	while(fgets(line, sizeof(line), stdin)) {
		/* Blank line means end of this package definition */
		if(line[0] == '\n') {
			strncpy(sql, "INSERT INTO packages (package, version, maintainer, homepage, section, remote_path, md5, description, installed_size, size) VALUES (", sizeof(sql)-1);
			quotecat(sql, current.package, sizeof(sql), 1);
			quotecat(sql, current.version, sizeof(sql), 1);
			quotecat(sql, current.maintainer, sizeof(sql), 1);
			quotecat(sql, current.homepage, sizeof(sql), 1);
			quotecat(sql, current.section, sizeof(sql), 1);
			quotecat(sql, current.remote_path, sizeof(sql), 1);
			quotecat(sql, current.md5, sizeof(sql), 1);
			quotecat(sql, current.description, sizeof(sql), 1);
			sprintf(sql, "%s%d,%d);", sql, current.installed_size, current.size);

			if((code = sqlite3_exec(db, sql, NULL, NULL, NULL)) != 0) {
				if(code == SQLITE_CONSTRAINT) {
					puts("TODO: update");
				} else {
					fprintf(stderr, "%s\n", sqlite3_errmsg(db));
					exit(EXIT_FAILURE);
				}
			}
			/* Reset things */
			code = 0;
			memset(&current, 0, sizeof(current));
		} else {
			/* Chomp */
			if((sep = strchr(line, '\n'))) {
				*sep = '\0';
			}
			/* Description spans multiple lines at the end, concat stuff */
			if(code) {
				strncat(current.description, "\n", sizeof(current.description)-1);
				strncat(current.description, line, sizeof(current.description)-1);
			} else {
				/* Split on colon */
				if((sep = strchr(line, ':'))) {
					*sep = '\0';
					/* Skip over the space too */
					sep = sep + 2;
					/* If we haven't seen the field yet, do a string compare to see if
					 * this is it. Copu remainder of line into struct */
					if(       current.package[0]      == '\0' && strcmp(line, "Package")        == 0) {
						strncpy(current.package,     sep, sizeof(current.package)-1);
					} else if(current.version[0]      == '\0' && strcmp(line, "Version")        == 0) {
						strncpy(current.version,     sep, sizeof(current.version)-1);
					} else if(current.section[0]      == '\0' && strcmp(line, "Section")        == 0) {
						strncpy(current.section,     sep, sizeof(current.section)-1);
					} else if(current.md5[0]          == '\0' && strcmp(line, "MD5sum")         == 0) {
						strncpy(current.md5,         sep, sizeof(current.md5)-1);
					} else if(current.maintainer[0]   == '\0' && strcmp(line, "Maintainer")     == 0) {
						strncpy(current.maintainer,  sep, sizeof(current.maintainer)-1);
					} else if(current.remote_path[0]  == '\0' && strcmp(line, "Filename")       == 0) {
						strncpy(current.remote_path, sep, sizeof(current.remote_path)-1);
					} else if(current.homepage        == '\0' && strcmp(line, "Homepage")       == 0) {
						strncpy(current.homepage,    sep, sizeof(current.homepage)-1);
					} else if(current.installed_size  ==   0  && strcmp(line, "Installed-Size") == 0) {
						current.installed_size = atoi(sep);
					} else if(current.size            ==   0  && strcmp(line, "Size")           == 0) {
						current.size = atoi(sep);
					} else if(                                   strcmp(line, "Depends")        == 0) {
						sep = strtok(sep, " (");
						if((endcomma = strchr(sep, ','))) {
							*endcomma = '\0';
							strncpy(sql, "INSERT INTO depends (package, depend, version) VALUES (", sizeof(sql)-1);
							quotecat(sql, current.package, sizeof(sql), 1);
							quotecat(sql, sep, sizeof(sql), 1);
							strncat(sql, "'');", sizeof(sql-1));
							puts(sql);
						} else {
							strncpy(sql, "INSERT INTO depends (package, depend, version) VALUES (", sizeof(sql)-1);
							quotecat(sql, current.package, sizeof(sql), 1);
							quotecat(sql, sep, sizeof(sql), 1);
							strtok(NULL, " ");
							if((sep = strtok(NULL, ")")) != NULL) {
								quotecat(sql, sep, sizeof(sql), 0);
								strncat(sql, ");", sizeof(sql-1));
								puts(sql);
								strtok(NULL, " ");
							} else {
								strncat(sql, "'');", sizeof(sql-1));
								puts(sql);
							}
						}
						while(sep != NULL) {
							sep = strtok(NULL, " (");
							if(sep == NULL) break;
							if((endcomma = strchr(sep, ','))) {
								*endcomma = '\0';
								puts(sep);
							} else {
								printf("\tPackage: %s\n", sep);
								strtok(NULL, " ");
								sep = strtok(NULL, ")");
								printf("\tVersion: %s\n", sep);
								sep = strtok(NULL, " ");
							}
						}
					} else if(                                   strcmp(line, "Description")    == 0) {
						strncpy(current.description, sep, sizeof(current.description)-1);
						code = 1;
					}
				}
			} /* if code */
		} /* if line[0] == '\n' */
	} /* while */

	/* End the transaction only when all data has been inserted */
	if(sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	/* Clean up disk space */
	if(sqlite3_exec(db, "VACUUM;", NULL, NULL, NULL) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	/* Close database */
	if(sqlite3_close(db) != 0) {
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
