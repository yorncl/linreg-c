#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFF_SIZE (size_t)(1024 * 1024 * 10)

#define uint8 unsigned char


typdef struct pair_t {
	double x;
	double y;
} pair_t;

typdef struct data_t {
	pair_t*	entries;
	char[256] labelx;
	char[256] labely;
} data_t;

void usage(){
    printf("Usage: ./train [data.csv]\n");
}

// minimal format validation for 2 columns
int validate_csv(uint8* buff) {

	char* ptr = (char*)buff;
	while(*ptr) {
		// if virgule
		if (*ptr == ',') {
			ptr++;
			//expect a new line, except if last ,
			while (*ptr) {
				if (*ptr == ',')
					return 1;
				else if (*ptr == '\n') {
					ptr++;
					break;
				}
				else
					ptr++;
			}
		}
		ptr++;
	}
	return 0;
}

void train(int n, double* theta0, double* theta1) {

	double t0 = 0, t1 = 0;

	for (int i = 0; i < n; i++) {
		printf("Epoch %d out of %d\n", i, n);

	}
	*theta0 = t0;
	*theta1 = t1;
}

int save_variables(double t0, double t1) {
	printf("Saving theta0(%lf) and theta1(%lf)\n", t0, t1);
	FILE* f = fopen("vars.csv", "w+");
	if (f == NULL) return 1;
	int r = fprintf(f, "%lf,%lf\n", t0, t1);
	if (r < 0) return 1;
	fclose(f);
	return 0;
}

int main(int ac, char** av) {

    if (ac != 2) {
        usage();
	return 1;
    }

    // alloc BUFF_SIZE for file content
    uint8* filebuff = calloc(1, BUFF_SIZE);
    if (filebuff == NULL) {
	    perror("allocation failed ");
	    return 2;
    }

    FILE* f = fopen(av[1], "r");
    if (f == NULL) {
	    perror("Could not open data file");
	    return 1;
    }
    fread(filebuff, BUFF_SIZE, 1, f);
    if (ferror(f)) {
	    perror("Error reading the data file");
	    return 1;
    }
    
    if (validate_csv(filebuff)) {
	fprintf(stderr, "csv data file is wrong: expecting 2 columns\n");
	exit(1);
    }

    double t0, t1;
    train(20, &t0, &t1);
    
    if (save_variables(t0, t1)) {
	perror("could not save to file");
	exit(2);
    }

    fclose(f);

    //system("xdg-open ./result.html");
    return 0;
}
