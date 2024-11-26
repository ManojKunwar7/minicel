#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

void usage(FILE *stream){
	fprintf(stream ,"Usage: ./minicel <input.csv>\n");
}

char *slurp_file(const char *file_path,size_t size){
	assert(0 && "not implemented");
	return 0;
}

int main(int argc, char **argv){

	if(argc < 2){
		usage(stderr);
		fprintf(stderr, "ERROR: input file is not provided\n");
		exit(1);
	}

	const char *input_file_path = argv[1];
	size_t file_size = 0; 
	const char *content = slurp_file(input_file_path, file_size);
	if(content == NULL){
		fprintf(stderr, "ERROR: could not read file %s:%s \n", input_file_path,strerror(errno));
	}
	// 28:26
	return 0;
}
