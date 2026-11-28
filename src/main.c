#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "compiler.h"

#define VERSION "R2-Piler 0.1.0"

static void print_help (const char *build)
{
	printf("%s\n", VERSION);
	printf("Work in progress: only the x86-64 backend is available\n\n");

	printf("USAGE\n");
	printf("    %s [OPTIONS] codefile.r2\n\n", build);

	printf("OPTIONS\n");
	printf("    -v|--version            Shows running version.\n");
	printf("    -V|--verify-regalloc    Check the register-allocation contract (off by default).\n");
	printf("    -F|--werror             Treat warnings as errors.\n");
	printf("    -h|--help               Shows this message.\n");
	printf("    -o|--out FILE           Output assembly path (default: <source>.s). '-' for stdout\n");
	printf("    -m|--modules-dir DIR    Directory for generated .r2i files (default: modules)\n");
	printf("    -p|--path DIR           Extra directory to search for imported .r2i files (repeatable)\n");
	printf("    -n|--no-r2i             Do not generate the .r2i interface file\n");
	printf("    -H|--header[=DIR]       Generate a C/C++ header in DIR (default: include)\n");
	printf("    -a|--arch ARCH          Indicates the architecture. Default: x86-64.\n");
	printf("    -e|--execute            Runs the program with the interpreter instead of generating assembly (not implemented yet).\n");
	printf("    -O[LEVEL]|--opt[=LEVEL] Optimization level 0, 1, 2 or 3 (no space; plain -O means 2). Default: 2\n");
	printf("    -T|--dump-tokens        Prints your code tokens to stdout\n");
	printf("    -A|--dump-ast           Prints the parsed AST to stdout\n");
	printf("    -S|--dump-symbols       Prints the resolved symbol table to stdout\n");
	printf("    -I|--dump-ir            Prints the generated IR to stdout\n");
	printf("    -D|--dump-opt           Prints the optimized IR to stdout\n");
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
	{"modules-dir", required_argument, 0, 'm'},
	{"path", required_argument, 0, 'p'},
	{"no-r2i", no_argument, 0, 'n'},
	{"header", optional_argument, 0, 'H'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'v'},
	{"verify-regalloc", no_argument, 0, 'V'},
	{"werror", no_argument, 0, 'F'},
	{"arch", required_argument, 0, 'a'},
	{"execute", no_argument, 0, 'e'},
	{"dump-tokens", no_argument, 0, 'T'},
	{"dump-ast", no_argument, 0, 'A'},
	{"dump-symbols", no_argument, 0, 'S'},
	{"dump-ir", no_argument, 0, 'I'},
	{"dump-opt", no_argument, 0, 'D'},
	{"opt", optional_argument, 0, 'O'},
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

static OptLevel get_opt_level (const char *s)
{
	if (!s || !s[0] || s[1] != '\0') {
		fprintf(stderr, "Unknown optimization level '%s'. Use 0, 1, 2 or 3\n", s ? s : "(null)");
		exit(1);
	}

	if (s[0] == '0') return NO_OPT;
	if (s[0] == '1') return OPT_REG;
	if (s[0] == '2') return OPT_BASIC;
	if (s[0] == '3') return OPT_FULL;

	fprintf(stderr, "Unknown optimization level '%s'. Use 0, 1, 2 or 3\n", s ? s : "(null)");
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
	int dumping = args->dump_tokens || args->dump_ast
			|| args->dump_symbols || args->dump_ir || args->dump_opt;

	if (optind >= argc) {
		fprintf(stderr, "No source file found\n");
		fprintf(stderr, "Execute '%s --help' for more info\n", argv[0]);
		exit(1);
	}
	args->path = argv[optind];
	if (optind + 1 < argc)
		fprintf(stderr, "Warning: only '%s' will be compiled\n", args->path);

	if (args->out == NULL) args->out = default_out(args->path);
	if (!dumping && same_file(args->out, args->path)) {
		fprintf(stderr, "Error: output file would overwrite the source file\n");
		exit(1);
	}
}

static CompilerOpts parse_args (int argc, char *argv[])
{
	CompilerOpts args = { .r2i_dir = "modules", .header_dir = "include",
			.arch = ARCH_X86_64, .opt_level = OPT_BASIC, .r2i = 1 };
	int opt;
	int interp = 0;
	int opt_spec = 0;
	const char **paths = NULL;
	int path_count = 0;

	opterr = 0;
	const char *short_opts = ":TIASDO::o:m:p:na:ehvH::VF";
	while ((opt = getopt_long(argc, argv, short_opts, long_options, NULL)) != -1) {
		switch (opt)
		{
		case 'o': args.out = optarg; break;
		case 'm':
			if (!optarg[0]) {
				fprintf(stderr, "Error: --modules-dir cannot be empty\n");
				exit(1);
			}
			args.r2i_dir = optarg;
			break;
		case 'p': {
			if (!optarg[0]) {
				fprintf(stderr, "Error: --path cannot be empty\n");
				exit(1);
			}
			const char **grown = realloc(paths, sizeof(char *) * (size_t)(path_count + 1));

			if (!grown) {
				fprintf(stderr, "Not enough memory\n");
				exit(1);
			}
			grown[path_count++] = optarg;
			paths = grown;
			break;
		}
		case 'n': args.r2i = 0; break;
		case 'H':
			args.header = 1;
			if (optarg && optarg[0] == '=') optarg++;
			if (optarg && optarg[0]) args.header_dir = optarg;
			break;
		case 'a': args.arch = get_arch(optarg); break;
		case 'e': interp = 1; break;
		case 'h': print_help(argv[0]); exit(0);
		case 'v': printf("%s\n", VERSION); exit(0);
		case 'V': cg_verify_regalloc = 1; break;
		case 'F': args.werror = 1; break;
		case 'T': args.dump_tokens = 1; break;
		case 'A': args.dump_ast = 1; break;
		case 'S': args.dump_symbols = 1; break;
		case 'I': args.dump_ir = 1; break;
		case 'D': args.dump_opt = 1; break;
		case 'O':
			args.opt_level = optarg ? get_opt_level(optarg) : OPT_BASIC;
			opt_spec = 1;
			break;
		case ':':
			fprintf(stderr, "Option '-%c' requires an argument.\n", optopt);
			exit(1);
		default:
			fprintf(stderr, "Unknown option '%s'. Execute '%s --help' for more info.\n",
					argv[optind - 1], argv[0]);
			exit(1);
		}
	}
	if (args.dump_tokens + args.dump_ast + args.dump_symbols
			+ args.dump_ir + args.dump_opt > 1)
		fprintf(stderr, "Warning: multiple dump flags given; only the first stage will be printed\n");
	resolve_paths(&args, argc, argv);
	if (interp) args.arch = ARCH_INTERP;
	if (args.dump_opt && !opt_spec) args.opt_level = OPT_FULL;
	args.import_paths = paths;
	args.import_path_count = path_count;
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
