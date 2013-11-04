/**
 * E-UAE JIT
 *
 * Generator of JIT compiling instruction tables from 68k instruction descriptor text file
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

//*** Predefined constants
#define FILENAME_IN  "table68k_comp"
#define FILENAME_OUT_C "compstbl.c"
#define FILENAME_OUT_H "comptbl.h"

#define MAXOPCODENAME 512
#define MAXADDRMODE 100

#define FILEBUFFERLEN 500


//*** Macros
#define exception(s, ...) stopwitherror(__LINE__, s, ##__VA_ARGS__);

//*** Generic list node type
struct list
{
	struct list* next;
};

//*** Structure of a node in the opcode linked list
struct opcode
{
	struct opcode* next; //*** Pointer to the next list item

	int opcode; //*** Opcode translator number
	int implemented; //*** Is this instruction implemented in JIT (0/1)
	int ext; //*** Number of extension words
	int size; //*** Operation size
	int op1; //*** Addressing of operand1
	int op2; //*** Addressing of operand2
	int jump; //*** Jump instruction (stop compiling)
	int constjump; //*** Constant jump instruction
	char code[20]; //*** Binary code of the opcode word
};

//*** Structure of a node in the address linked list
struct address
{
	struct address* next; //*** Pointer to the next list item

	char name[20]; //*** Name of addressing mode
	char modus[4]; //*** Modus of mode
	char reg[4]; //*** Register of mode
	int number; //*** The number of this address
	int src_used; //*** Marker for usage
	int dest_used; //*** Marker for usage
};

//*** Predefinitions
void cleanup(void);
void stopwitherror(int line, char* errorstr, ...);
void dealloc(struct list* head);
unsigned int bindec(char* s);
void decbin(char* s, unsigned int i);
void selectstr(char* s, char* t, char c, int i);
void insertopcode(int opcode, char* code, struct address* src, struct address* dest, int implemented, int ext, int size, int jump, int constjump);
void readfromtablefile(void);
void buildaddresslist(void);
void buildopcodelist(void);
void generateheaderfile(void);
unsigned long generatecfile(void);
void dumpaddresstable(int src, int pre);

//*** Globals
struct address* adhead = NULL;
struct opcode* ophead = NULL;
char opcodename[MAXOPCODENAME][30];
int opcodenamecount = 0;
struct address* srcadr[MAXADDRMODE];
struct address* destadr[MAXADDRMODE];
FILE* headerfile = NULL;
FILE* cfile = NULL;
FILE* tablefile = NULL;
int line = 0;
char s[FILEBUFFERLEN];

//*** Main function
int main(int argc, char *argv[])
{
	char filenameout[300];
	char* filesdir;

	//Checking for the only accepted argument: path for files
	if (argc == 1)
	{
		filesdir = "";
	}
	else
	{
		if (argc == 2)
		{
			filesdir = argv[1];
		}
		else
		{
			exception("Wrong arguments");
		}
	}

	//Open opcode table file
	if (!(tablefile = fopen(FILENAME_IN, "r")))
	{
		exception("Could not open source file");
	}

	//Building the address list from the external description file
	buildaddresslist();

	//Table file must not end here just yet
	if (feof(tablefile))
	{
		exception("Unexpected end of file");
	}

	//Building the opcode list from the external description file
	buildopcodelist();

	fclose(tablefile);
	tablefile = NULL;

	//Opening output files
	strcpy(filenameout, filesdir);
	strcat(filenameout, FILENAME_OUT_H);
	if (!(headerfile = fopen(filenameout, "w")))
	{
		exception("Could not open header destination file");
	}

	strcpy(filenameout, filesdir);
	strcat(filenameout, FILENAME_OUT_C);
	if (!(cfile = fopen(filenameout, "w")))
	{
		exception("Could not open C destination file");
	}

	//Generate the header file into the sources folder
	generateheaderfile();

	//Generate the C source file into the sorces folder
	unsigned long instruction_count = generatecfile();

	//*** cleanup and errors, if something gone wrong

	cleanup();

	printf("%lu opcodes generated\n", instruction_count);

	return (EXIT_SUCCESS);

}

//*** Cleaning up allocated resources
void cleanup(void)
{
	//Releasing files
	if (cfile)
	{
		fflush(cfile);
		fclose(cfile);
		cfile = NULL;
	}

	if (headerfile)
	{
		fflush(headerfile);
		fclose(headerfile);
		headerfile = NULL;
	}

	if (tablefile)
	{
		fclose(tablefile);
		tablefile = NULL;
	}

	//Releasing memory
	dealloc((struct list*) adhead);
	adhead = NULL;
	dealloc((struct list*) ophead);
	ophead = NULL;
}

//Error while executing the code, clean up and stop
void stopwitherror(int line, char* errorstr, ...)
{
	char fullerrorstr[500];
	char linenum[500];
    va_list parms;

    sprintf(linenum, " (gencomp.c line: %d)\n", line);

    //Concat error string
    strcpy(fullerrorstr, "Error: ");
    strcat(fullerrorstr, errorstr);
    strcat(fullerrorstr, linenum);

    //Print error text with parameters
	va_start (parms, errorstr);
	vfprintf(stderr, fullerrorstr, parms);
	va_end (parms);

	cleanup();

	exit(EXIT_FAILURE);
}

void readfromtablefile(void)
{
	//Check for opened file
	if (tablefile == NULL)
	{
		exception("Table file was not open before reading");
	}

	fgets(s, FILEBUFFERLEN, tablefile);

	//Remove new line and carriage return characters
	char* p = s;
	for (; *p; p++)
	{
		if (*p == '\n' || *p == '\r') *p = '\0';
	}

	//Put exactly one new line character at the end of the string
	strcat(s, "\n");

	//Increment line counter for the table file
	line++;
}

//Deallocating of opcodelist
void dealloc(struct list* head)
{
	struct list* act;

	while (head)
	{
		act = head->next;
		free(head);
		head = act;
	}
}

unsigned int bindec(char* s)
{
	unsigned int i;
	int j;

	i = 0;
	for (j = 0; s[j]; j++)
	{
		i <<= 1;
		if (s[j] == '1') i |= 1;
	}
	return i;
}

void decbin(char* s, unsigned int i)
{
	int j;

	for (j = 0; j < 16; j++)
	{
		if (i & (1 << j))
		{
			s[15 - j] = '1';
		}
		else
		{
			s[15 - j] = '0';
		}
	}
	s[16] = '\0';

}

void selectstr(char* s, char* t, char c, int i)
{
	int j, k, n;

	t[0] = '\0';

	for (j = 0, n = 0; (n != i) && (s[j] != '\0'); j++)
	{
		if (s[j] == c) n++;
	}

	if (n != i) return;

	for (k = 0; (s[j] != c) && (s[j] != '\0'); k++, j++)
	{
		t[k] = s[j];
	}

	t[k] = '\0';

}

void insertopcode(int opcode, char* code, struct address* src, struct address* dest, int implemented, int ext, int size, int jump, int constjump)
{
	struct opcode* act;
	int i, j;

	if (!(act = malloc(sizeof(struct opcode))))
	{
		exception("Out of memory");
	}

	act->next = ophead;
	ophead = act;
	act->opcode = opcode;
	strcpy(act->code, code);
	act->implemented = implemented;
	act->ext = ext;
	act->size = size;
	act->jump = jump;
	act->constjump = constjump;
	if (src)
	{
		act->op1 = src->number;
		src->src_used = 1;
		for (i = 0; i < 16;)
		{
			switch (act->code[i])
			{
			case 'S':
				for (j = 0; (j < 3) && (act->code[i] == 'S'); j++)
				{
					if (src->modus[j] != 'r') act->code[i] = src->modus[j];
					i++;
				}
				break;
			case 's':
				for (j = 0; (j < 3) && (act->code[i] == 's'); j++)
				{
					if (src->reg[j] != 'r') act->code[i] = src->reg[j];
					i++;
				}
				break;
			default:
				i++;
			}
		}
	}
	else
	{
		act->op1 = 0;
	}

	if (dest)
	{
		act->op2 = dest->number;
		dest->dest_used = 1;
		for (i = 0; i < 16;)
		{
			switch (act->code[i])
			{
			case 'D':
				for (j = 0; (j < 3) && (act->code[i] == 'D'); j++)
				{
					if (dest->modus[j] != 'r') act->code[i] = dest->modus[j];
					i++;
				}
				break;
			case 'd':
				for (j = 0; (j < 3) && (act->code[i] == 'd'); j++)
				{
					if (dest->reg[j] != 'r') act->code[i] = dest->reg[j];
					i++;
				}
				break;
			default:
				i++;
			}
		}
	}
	else
	{
		act->op2 = 0;
	}
}

//Build address list from table file
void buildaddresslist(void)
{
	struct address* adact;
	int i = 0;

	//Searching for address section
	while (!(feof(tablefile)))
	{
		readfromtablefile();
		if (strcmp("#Addressing\n", s) == 0)
			{
				break;
			}
	}

	while (!(feof(tablefile)))
	{
		readfromtablefile();

		//Opcode description section is reached
		if (strcmp("#Opcodes\n", s) == 0) break;

		if ((s[0] != ';') && (strlen(s) > 4))
		{
			if (!(adact = malloc(sizeof(struct address))))
			{
				exception("Out of memory");
			}

			adact->next = adhead;
			adhead = adact;

			/* format: address modus register */
			sscanf(s, "%s %s %s", adact->name, adact->modus, adact->reg);
			adact->number = i++;
			adact->src_used = 0;
			adact->dest_used = 0;
		}
	}
}

//Building the list of opcodes from table file
void buildopcodelist(void)
{
	struct address* adact;
	char code[50];
	char c1[20];
	char c2[10];
	char c3[10];
	char c4[10];
	char dest[300];
	char src[300];
	char name[50];
	char str[300];
	int jump, constjump;
	int implemented;
	int ext, size;
	int i, j, num;

	while (!(feof(tablefile)))
	{
		readfromtablefile();
		if ((s[0] != ';') && (strlen(s) > 4))
		{
			/* format: opcode impl ext siz code1 code2 code3 code4 jump constjump srcaddr destaddr */

			sscanf(s, "%s %d %d %d %s %s %s %s %d %d %s %s", name, &implemented, &ext, &size, c1, c2, c3, c4, &jump, &constjump, src, dest);

			if (implemented != 0 && implemented != 1)
			{
				exception("Implemented property must be either 0 or 1 at line %d", line);
			}

			if (ext != 0 && ext != 1 && ext != 2 && ext != 3)
			{
				exception("Extension words property must be between 0 and 3 at line %d", line);
			}

			if (size != 0 && size != 1 && size != 2 && size != 4 && size != 16)
			{
				exception("Operation size property must be one of these values: 0, 1, 2, 4 or 16 at line %d", line);
			}

			for (i = 0; (i < opcodenamecount) && (strcmp(opcodename[i], name)); i++);

			if (!(i < opcodenamecount))
			{
				if (opcodenamecount == MAXOPCODENAME)
				{
					exception("Maximum number of opcode names reached, increase MAXOPCODENAME");
				}
				strcpy(opcodename[opcodenamecount], name);
				opcodenamecount++;
			}

			num = i;

			strcat(c1, c2);
			strcat(c1, c3);
			strcat(c1, c4);
			strcpy(code, c1);

			/* Splitting up source and destination addressing modes */

			i = 0;
			if (strcmp("none", src) != 0)
			{
				for (i = 0; i < MAXADDRMODE; i++)
				{
					selectstr(src, str, ',', i);
					if (strcmp("", str) == 0) break;
					for (adact = adhead; (adact != NULL) && (strcmp(adact->name, str) != 0); adact
							= adact->next)
						;
					if (adact == NULL)
					{
						exception("Unknown source addressing at line %d: \"%s\"", line,
								str);
					}
					srcadr[i] = adact;
				}

				if (i == MAXADDRMODE)
				{
					exception("Maximum number of address modes reached, increase MAXADDRMODE");
				}
			}
			srcadr[i] = NULL;

			i = 0;
			if (strcmp("none", dest) != 0)
			{
				for (; i < MAXADDRMODE; i++)
				{
					selectstr(dest, str, ',', i);
					if (strcmp("", str) == 0) break;
					for (adact = adhead; (adact != NULL) && (strcmp(adact->name, str) != 0); adact = adact->next);
					if (adact == NULL)
					{
						exception("Unknown destination addressing at line %d: \"%s\"\n", line, str);
					}
					destadr[i] = adact;
				}

				if (i == MAXADDRMODE)
				{
					exception("Maximum address mode number reached");
				}
			}
			destadr[i] = NULL;

			if (srcadr[0] == NULL)
			{
				if (destadr[0] == NULL)
				{
					insertopcode(num, code, NULL, NULL, implemented, ext, size, jump, constjump);
				}
				else
				{
					for (i = 0; destadr[i] != NULL; i++)
						insertopcode(num, code, NULL, destadr[i], implemented, ext, size, jump, constjump);
				}
			}
			else
			{
				if (destadr[0] == NULL)
				{
					for (i = 0; srcadr[i] != NULL; i++)
						insertopcode(num, code, srcadr[i], NULL, implemented, ext, size, jump, constjump);
				}
				else
				{
					for (j = 0; srcadr[j] != NULL; j++)
						for (i = 0; destadr[i] != NULL; i++)
							insertopcode(num, code, srcadr[j], destadr[i], implemented, ext, size, jump, constjump);
				}
			}
		}
	}
}

void generateheaderfile(void)
{
	struct address* adact;
	int i;

	fprintf(headerfile, "// Generated macroblock function protos\n// Do not edit manually!\n\n");

	fprintf(headerfile, "// Addressing modes\n");
	for (adact = adhead; (adact); adact = adact->next)
	{
		char* name = adact->name;

		if ((name[0] == 'C' && name[1] == 'C') || (name[0] == 'F' && name[1] == 'C' && name[2] == 'C'))
		{
			//Condition code (virtual) addressing mode
			fprintf(headerfile, "void comp_cond_pre_%s_src(const cpu_history* history, struct comptbl* props) REGPARAM;\n", name);
		} else {
			//Normal addressing mode
			fprintf(headerfile, "void comp_addr_pre_%s_src(const cpu_history* history, struct comptbl* props) REGPARAM;\n", name);
			fprintf(headerfile, "void comp_addr_pre_%s_dest(const cpu_history* history, struct comptbl* props) REGPARAM;\n", name);
			fprintf(headerfile, "void comp_addr_post_%s_src(const cpu_history* history, struct comptbl* props) REGPARAM;\n", name);
			fprintf(headerfile, "void comp_addr_post_%s_dest(const cpu_history* history, struct comptbl* props) REGPARAM;\n", name);
		}
	}

	fprintf(headerfile, "\n\n// Instructions\n");
	for(i = 0; i < opcodenamecount; i++)
	{
		fprintf(headerfile, "void comp_opcode_%s(const cpu_history* history, struct comptbl* props) REGPARAM;\n", opcodename[i]);
	}

	fprintf(headerfile, "\n\n");
}

unsigned long generatecfile(void)
{
	unsigned long count = 0;
	struct opcode* act;
	char c1[20];
	char c2[10];
	char c3[10];
	char c4[10];
	char c;
	int sr, de;
	int i, j;
	char specstr[200];

	fprintf(cfile, "// Generated instruction property table and addressing function offsets\n// Do not edit manually!\n\n");

	//Start with includes
	fprintf(cfile, "#include \"sysconfig.h\"\n");
	fprintf(cfile, "#include \"sysdeps.h\"\n");
	fprintf(cfile, "#include \"options.h\"\n");
	fprintf(cfile, "#include \"include/memory.h\"\n");
	fprintf(cfile, "#include \"custom.h\"\n");
	fprintf(cfile, "#include \"newcpu.h\"\n");
	fprintf(cfile, "#include \"compemu.h\"\n");
	fprintf(cfile, "#include \"compemu_macroblocks.h\"\n");
	fprintf(cfile, "#include \"comptbl.h\"\n\n");

	//Generate compile properties table
	fprintf(cfile, "struct comptbl compprops[65536] = {\n");
	for (i = 0; i < 65536; i++)
	{
		decbin(c1, i);
		act = ophead;
		while (act)
		{
			de = sr = 0;
			for (j = 0; j < 16; j++)
			{
				c = act->code[j];
				if (c == 's')
				{
					c2[sr] = c1[j];
					sr++;
				}
				else
				{
					if (c == 'd')
					{
						c3[de] = c1[j];
						de++;
					}
					else
					{
						if ((c1[j] != c) && (c != 'x')) break;
					}
				}
			}
			if (j == 16) break;
			act = act->next;
		}

		if (act)
		{
			strcpy(specstr, (act) && (act->implemented) ? "COMPTBL_SPEC_ISIMPLEMENTED" : "COMPTBL_SPEC_ISNOTIMPLEMENTED");

			if (act->jump) strcat(specstr, "|COMPTBL_SPEC_ISJUMP");
			if (act->constjump) strcat(specstr, "|COMPTBL_SPEC_ISCONSTJUMP");
		} else {
			strcpy(specstr, "COMPTBL_SPEC_ISNOTIMPLEMENTED|COMPTBL_SPEC_ISJUMP");
		}

		if ((act) && (act->implemented))
		{
			c2[sr] = '\0';
			c3[de] = '\0';

			fprintf(cfile, "\t{comp_opcode_%s, %d, %d, %d, %d, %d, %d, %s}",
					opcodename[act->opcode],
					act->ext,
					act->size,
					bindec(c2),
					bindec(c3),
					act->op1,
					act->op2,
					specstr);

			count++;
		}
		else
		{
			//Not implemented/illegal opcode: null filled

			fprintf(cfile, "\t{NULL, 0, 0, 0, 0, 0, 0, %s}", specstr);
		}

		if (i != 65535) fprintf(cfile, ",");

		fprintf(cfile, " // %04x\n", i);
	}

	fprintf(cfile, "};\n\n");

	//Generate function jump tables for addressing

	//Pre source addressing jump table
	dumpaddresstable(1, 1);

	//Post source addressing jump table
	dumpaddresstable(1, 0);

	//Pre dest addressing jump table
	dumpaddresstable(0, 1);

	//Post source addressing jump table
	dumpaddresstable(0, 0);

	return count;
}

//Dump addressing mode jump table into the C source file
void dumpaddresstable(int src, int pre)
{
	struct address* adact;
	int i;

	fprintf(cfile, "compop_func* comp%s_%s_func[] = {", (src ? "src" : "dest"), (pre ? "pre" : "post"));
	for (i = 0;; i++)
	{
		for (adact = adhead; (adact) && (adact->number != i); adact = adact->next);

		if (!adact) break;

		if ((src && adact->src_used) || (!src && adact->dest_used))
		{
			//Is this a condition code?
			if (((adact->name[0] == 'F') && (adact->name[1] == 'C') && (adact->name[2] == 'C'))
					|| ((adact->name[0] == 'C') && (adact->name[1] == 'C')))
			{
				//Yes, put condition code into the table

				//Is this source and pre?
				if ((src) && (pre))
				{
					//Yes, we need it, CC codes are ignored for post or destination
					fprintf(cfile, "\n\tcomp_cond_%s_%s_%s,", (pre ? "pre" : "post"), adact->name, (src ? "src" : "dest"));
				} else {
					//This addressing mode is not in use
					fprintf(cfile, "\n\tNULL,");
				}
			}
			else
			{
				//This is a normal addressing mode
				fprintf(cfile, "\n\tcomp_addr_%s_%s_%s,", (pre ? "pre" : "post"), adact->name, (src ? "src" : "dest"));
			}
		}
		else
		{
			//This addressing mode is not in use
			fprintf(cfile, "\n\tNULL,");
		}
	}
	fprintf(cfile, "\n\tNULL };\n\n");
}
//*** EOF
