#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>

#include "compiler.h"

#define VERSION "R2-Piler 0.1.0"

typedef struct Args {
	char *out;
	char *path;
	int dump;
} Args;

static void print_help (const char *build)
{
	printf("%s\n", VERSION);
	printf("Nowadays, R2-Piler is not finished and does not work\n\n");

	printf("USAGE\n");
	printf("    %s [OPTIONS] codefile.r2\n\n", build);

	printf("OPTIONS\n");
	printf("    -v|--version        Shows running version.\n");
	printf("    -h|--help           Shows this message.\n");
	printf("    -o|--out            Actually nothing.\n");
	printf("    -T|--dump-tokens    Prints your code tokens to stdout\n");
}

static Args parse_args (int argc, char *argv[])
{
	struct option long_options[] = {
		{"out", required_argument, 0, 'o'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"dump-tokens", no_argument, 0, 'T'},
		{0, 0, 0, 0}
	};
	Args args = {
		.path = NULL,
		.out = NULL,
		.dump = 0
	};

	int opt;
	char *short_opts = "To:hv";
	while ((opt = getopt_long(argc, argv, short_opts, long_options, NULL)) != -1) {
		switch (opt)
		{
		case 'o': args.out = optarg; break;
		case 'h': print_help(argv[0]); exit(0);
		case 'v': printf("%s\n", VERSION); exit(0);
		case 'T': args.dump = 1; break;
		default: fprintf(stderr, "Unknown option. Execute '%s --help' for more info.\n", argv[0]); exit(0);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No source file found\n");
		fprintf(stderr, "Execute '%s --help' for more info\n", argv[0]);
		exit(1);
	}
	args.path = argv[optind];

	return args;
}

static long file_size (FILE *f, const char *file)
{
	if (fseek(f, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error: cannot seek in %s\n", file);
		exit(1);
	}
	long size = ftell(f);
	if (size < 0) {
		fprintf(stderr, "Error: cannot determine size of %s\n", file);
		exit(1);
	}
	rewind(f);
	return size;
}

static char *read_src (const char *file)
{
	struct stat st;
	if (stat(file, &st) != 0 || !S_ISREG(st.st_mode)) {
		fprintf(stderr, "Error: %s is not a regular file\n", file);
		exit(1);
	}

	FILE *f = fopen(file, "rb");
	if (!f) {
		fprintf(stderr, "Error: could not read file %s\n", file);
		exit(1);
	}
	size_t src_size = (size_t)file_size(f, file);

	char *src = arena_alloc(src_size + 1);
	if (fread(src, sizeof(char), src_size, f) != src_size) {
		fprintf(stderr, "Could not read the file %s correctly\n", file);
		fclose(f);
		exit(1);
	}
	src[src_size] = '\0';
	fclose(f);
	return src;
}

int main (int argc, char *argv[])
{
	Args args = parse_args(argc, argv);
	arena_init();
	char *src = read_src(args.path);
	Compiler *compiler = init_compiler(src);

	if (args.dump) {
		int failed = dump_tokens(&compiler->lexer) != 0;
		arena_destroy();
		return failed ? 1 : 0;
	}

	arena_destroy();
	return 0;			       
}					       
