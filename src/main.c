#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>

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
	printf("    -o|--out FILE       Output assembly path (default: out.s). '-' for stdout\n");
	printf("    -a|--arch ARCH      Indicates the architecture. Default: x86-64.\n");
	printf("    -e|--execute        Executes the program as an interpreter inestead generating assembly.\n");
	printf("    -T|--dump-tokens    Prints your code tokens to stdout\n");
	printf("    -A|--dump-ast       Prints the parsed AST to stdout\n");
	printf("    -S|--dump-symbols   Prints the resolved symbol table to stdout\n");
	printf("    -I|--dump-ir        Prints the generated IR to stdout\n");
}

typedef struct ArchAlias {
	const char *name;
	Arch arch;
} ArchAlias;

static const ArchAlias arch_aliases[] = {
	{"x86_64", ARCH_X86_64}, {"x86-64", ARCH_X86_64}, {"amd64", ARCH_X86_64},
	{"arm", ARCH_ARM}, {"aarch64", ARCH_ARM},
	{"riscv", ARCH_RISCV}, {"riscv64", ARCH_RISCV}
};

static const struct option long_options[] = {
	{"out", required_argument, 0, 'o'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'v'},
	{"arch", required_argument, 0, 'a'},
	{"execute", no_argument, 0, 'e'},
	{"dump-tokens", no_argument, 0, 'T'},
	{"dump-ast", no_argument, 0, 'A'},
	{"dump-symbols", no_argument, 0, 'S'},
	{"dump-ir", no_argument, 0, 'I'},
	{0, 0, 0, 0}
};

static Arch get_arch (const char *s)
{
	size_t count = sizeof(arch_aliases) / sizeof(arch_aliases[0]);

	for (size_t i = 0; i < count; i++)
		if (strcasecmp(s, arch_aliases[i].name) == 0)
			return arch_aliases[i].arch;

	fprintf(stderr, "Unknown architecture '%s' (valid: x86-64, arm, riscv)\n", s);
	exit(1);
}

static char *default_out (const char *path)
{
	const char *base = strrchr(path, '/');
	const char *dot;
	size_t stem;
	char *out;

	base = base ? base + 1 : path;
	dot = strrchr(base, '.');
	stem = (dot && dot != base) ? (size_t)(dot - path) : strlen(path);

	out = malloc(stem + 3);
	if (!out) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	memcpy(out, path, stem);
	memcpy(out + stem, ".s", 3);
	return out;
}

static int same_file (const char *a, const char *b)
{
	struct stat sa, sb;
	if (strcmp(a, b) == 0) return 1;
	if (stat(a, &sa) != 0 || stat(b, &sb) != 0) return 0;
	return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static void resolve_paths (CompilerOpts *args, int argc, char *argv[])
{
	if (optind >= argc) {
		fprintf(stderr, "No source file found\n");
		fprintf(stderr, "Execute '%s --help' for more info\n", argv[0]);
		exit(1);
	}
	args->path = argv[optind];
	if (optind + 1 < argc)
		fprintf(stderr, "Warning: only '%s' will be compiled\n", args->path);

	if (args->out == NULL) args->out = default_out(args->path);
	if (same_file(args->out, args->path)) {
		fprintf(stderr, "Error: output file would overwrite the source file\n");
		exit(1);
	}
}

static CompilerOpts parse_args (int argc, char *argv[])
{
	CompilerOpts args = { .arch = ARCH_X86_64 };
	int opt;
	int interp = 0;

	opterr = 0;
	while ((opt = getopt_long(argc, argv, ":TIASo:a:ehv", long_options, NULL)) != -1) {
		switch (opt)
		{
		case 'o': args.out = optarg; break;
		case 'a': args.arch = get_arch(optarg); break;
		case 'e': interp = 1; break;
		case 'h': print_help(argv[0]); exit(0);
		case 'v': printf("%s\n", VERSION); exit(0);
		case 'T': args.dump_tokens = 1; break;
		case 'A': args.dump_ast = 1; break;
		case 'S': args.dump_symbols = 1; break;
		case 'I': args.dump_ir = 1; break;
		case ':':
			fprintf(stderr, "Option '-%c' requires an argument.\n", optopt);
			exit(1);
		default:
			fprintf(stderr, "Unknown option. Execute '%s --help' for more info.\n", argv[0]);
			exit(1);
		}
	}
	resolve_paths(&args, argc, argv);
	if (interp) args.arch = ARCH_INTERP;
	return args;
}

int main (int argc, char *argv[])
{
	CompilerOpts opts = parse_args(argc, argv);

	Compiler comp;
	compiler_init(&comp);

	int status = compile(&comp, &opts);
	compiler_destroy(&comp);
	return status;
}
