#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "compiler.h"

#define VERSION "R2-Piler 0.1.0"

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

typedef struct Args {
	char *out;
	char *path;
	int dump_tokens;
} Args;

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
		.dump_tokens = 0
	};

	int opt;
	char *short_opts = ":To:hv";
	opterr = 0;
	while ((opt = getopt_long(argc, argv, short_opts, long_options, NULL)) != -1) {
		switch (opt)
		{
		case 'o': args.out = optarg; break;
		case 'h': print_help(argv[0]); exit(0);
		case 'v': printf("%s\n", VERSION); exit(0);
		case 'T': args.dump_tokens = 1; break;
		case ':':
			fprintf(stderr, "Option '-%c' requires an argument.\n", optopt);
			exit(1);
		default:
			fprintf(stderr, "Unknown option. Execute '%s --help' for more info.\n", argv[0]);
			exit(1);
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

int main (int argc, char *argv[])
{
	Args args = parse_args(argc, argv);

	Compiler comp;
	compiler_init(&comp);

	if (compiler_load_file(&comp, args.path) != 0) {
		compiler_destroy(&comp);
		return 1;
	}

	int status = 0;
	if (args.dump_tokens)
		status = dump_tokens(&comp.lexer) != 0 ? 1 : 0;

	compiler_destroy(&comp);
	return status;
}
