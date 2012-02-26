/**
 * AmigaOS Kickstart ROM checksum calculator
 *
 * This tool calculates the correct checksum value for a binary file that is required
 * by AmigaOS and most of the Amiga emulators and appends the correction value at the end
 * of the file. If the file has the correct size then it can be loaded as Kickstart ROM into
 * the Amiga emulators.
 *
 * Created by Almos Rajnai
 * 06/09/2011
 *
 * This source code was derived from the checksum calculator routine
 * E-UAE Amiga emulation software.
 * The AmigaOS Kickstart ROM checksum calculator is open-source software and is made
 * available under the terms of the GNU Public License version 2.
 * For the details please visit this web page:
 * http://www.gnu.org/licenses/gpl-2.0.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

FILE* infile = NULL;
FILE* outfile = NULL;

void init(char* infilename, char* outfilename);
void done();
void error(char* errorstr, ...);
int readlong(FILE* file, unsigned int* data);
void writelong(FILE* file, unsigned int data);

int main(int argc, char **argv)
{
	int i;

	if (argc != 3)
	{
		error("Specify input and output file\nUsage: romchecksum <input file> <output file>");
	}

	init(argv[1], argv[2]);

	unsigned int cksum = 0;
	unsigned int prevck = 0;
	unsigned int data = 0;

	while (readlong(infile, &data))
	{
		writelong(outfile, data);
		cksum += data;
		if (cksum < prevck) cksum++;
		prevck = cksum;
	}

	if (ferror(infile))
	{
		error("Wrong input file, please check length");
	}

	data = 0xFFFFFFFFul  - cksum;
	printf("Calculated checksum: %08x\n",cksum);
	printf("Correction value: %08x\n",data);
	fwrite(&data, sizeof(char), 4, outfile);

	done();
}

void init(char* infilename, char* outfilename)
{
	infile = fopen(infilename, "rb");
	outfile = fopen(outfilename, "wb");

	if (infile == NULL)
	{
		error("Input file cannot be opened: %s", infilename);
	}

	if (outfile == NULL)
	{
		error("Output file cannot be opened: %s", outfilename);
	}
}

void done()
{
	if (infile)
	{
		fflush(infile);
		fclose(infile);
		infile = NULL;
	}

	if (outfile)
	{
		fflush(outfile);
		fclose(outfile);
		outfile = NULL;
	}
}

void error(char* errorstr, ...)
{
	char buffer[500];
    va_list parms;

	va_start (parms, errorstr);
	sprintf(buffer, "ERROR: %s\n", errorstr);
	vprintf(buffer, parms);
	done();

	exit(EXIT_FAILURE);
}

int readlong(FILE* file, unsigned int* data)
{
	unsigned char c;
	int i;

	c = fgetc(file);
	if (feof(file)) return 0;

	*data = 0;
	*data |= c;

	for(i = 0; i < 3; i++)
	{
		c = fgetc(file);
		if (feof(file)) error("Invalid input file, check size");

		*data <<= 8;
		*data |= c;
	}
	return 1;
}

void writelong(FILE* file, unsigned int data)
{
	unsigned char c[4];
	int i;

	for(i = 0; i < 4; i++)
	{
		c[3-i] = (unsigned char)(data & 255);
		data >>= 8;
	}

	for(i = 0; i < 4; i++)
	{
		fputc(c[i], file);
	}
}

