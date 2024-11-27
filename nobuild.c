#define NOBUILD_IMPLEMENTATION
#include "./nobuild.h"
#define CFLAGS "-Wall", "-Wextra", "-std=c11", "-pedantic", "-ggdb"

int main(int argc, char **argv){
	GO_REBUILD_URSELF(argc, argv);
	CMD("cc", CFLAGS, "-o", "minicel", "src/main.c");
		if(argc > 1){
			if(strcmp(argv[1], "run") == 0){
				CMD("./minicel", argv[2]);
			} else {
				PANIC("'%s' IS UNKNOWN SUBCOMMAND!", argv[1]);
			}
		}
	return 0;
}
