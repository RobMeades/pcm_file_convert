// pcm_file_convert.cpp : Defines the entry point for the console application.

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Things to help with parsing filenames.
#define DIR_SEPARATORS "\\/"
#define EXT_SEPARATOR "."
#define OUTPUT_FILE_EXTENSION "csv"

// Print the usage text
static void printUsage(char *pExeName) {
    printf("\n%s: take a binary audio file containing PCM audio data of a given word-width/endianness and produce\n", pExeName);
    printf("a CSV file of the numbers in it.  Usage:\n");
    printf("    %s input_file <-e endianness> <-w word_width> <-o output_file>\n", pExeName);
    printf("where:\n");
    printf("    input_filename is the PCM input file,\n");
    printf("    -e optionally specifies the endianness, b for big, l for little (l is the default),\n");
    printf("    -w optionally specifies the width of a word in bytes, 1 to 4 are the allowed values (4 by default),\n");
    printf("    -o optionally specifies the output file (if not specified the output file is input_file with any\n");
    printf("       extension replaced with %s%s); if the output file exists it will be overwritten,\n", EXT_SEPARATOR, OUTPUT_FILE_EXTENSION);
    printf("For example:\n");
    printf("    %s input.pcm -e l -w 4 -o output.csv\n\n", pExeName);
}

// Parse the input file and write to the output file
static int parse(FILE *pInputFile, FILE *pOutputFile, int wordWidth, bool isLittleEndian)
{
    unsigned char wordArray[4]; // Must be unsigned or the casts can produce negative numbers
    char buf[32];
    unsigned int unsignedNumber;
    char c;
    int itemsWritten = 0;

    // Read a word at a time until we get none
    while (fread(wordArray, wordWidth, 1, pInputFile) == 1) {
        if (!isLittleEndian) {
            // Make sure byte 0 is the little end
            for (int x = 0; x < wordWidth; x++ ) {
                c = wordArray[x];
                wordArray[wordWidth - x - 1] = c;
            }
        }
        // Calculate the number
        unsignedNumber = 0;
        for (int x = 0; x < wordWidth; x++) {
            unsignedNumber += ((unsigned int) (wordArray[x])) << (x << 3);
        }
        // Deal with sign
        if (unsignedNumber & (1 << ((wordWidth << 3) - 1))) {
            // Sign extend
            for (int x = (wordWidth << 3); x < sizeof(wordArray) << 3; x++) {
                unsignedNumber |= 1 << x;
            }
        }
        if (itemsWritten > 0) {
            fwrite(", ", 2, 1, pOutputFile);
        }
        // Write the number to the file as a string
        sprintf(buf, "%d", (int) unsignedNumber);
        fwrite(buf, strlen(buf), 1, pOutputFile);
        itemsWritten++;
    }

    return itemsWritten;
}

// Entry point
int main(int argc, char* argv[])
{
    int retValue = -1;
    bool success = false;
    int x = 0;
    int wordWidth = 4;
    char *pExeName = NULL;
    char *pInputFileName = NULL;
    FILE *pInputFile = NULL;
    char *pOutputFileName = NULL;
    bool outputFileNameMalloced = false;
    FILE *pOutputFile = NULL;
    char *pEndianness = "l";
    char *pChar;
    struct stat st = { 0 };

    // Find the exe name in the first argument
    pChar = strtok(argv[x], DIR_SEPARATORS);
    while (pChar != NULL) {
        pExeName = pChar;
        pChar = strtok(NULL, DIR_SEPARATORS);
    }
    if (pExeName != NULL) {
        // Remove the extension
        pChar = strtok(pExeName, EXT_SEPARATOR);
        if (pChar != NULL) {
            pExeName = pChar;
        }
    }
    x++;

    // Look for all the command line parameters
    while (x < argc) {
        // Test for input filename
        if (x == 1) {
            pInputFileName = argv[x];
        // Test for endianness option
        } else if (strcmp(argv[x], "-e") == 0) {
            x++;
            if (x < argc) {
                pEndianness = argv[x];
            }
        // Test for word width option
        } else if (strcmp(argv[x], "-w") == 0) {
            x++;
            if (x < argc) {
                wordWidth = atoi(argv[x]);
            }
        // Test for output file option
        } else if (strcmp(argv[x], "-o") == 0) {
            x++;
            if (x < argc) {
                pOutputFileName = argv[x];
            }
        }
        x++;
    }

    // Must have the mandatory command-line parameter
    if (pInputFileName != NULL) {
        success = true;
        // Check that endianness makes sense
        if ((strlen (pEndianness) != 1) ||
            ((*pEndianness != 'l') &&
             (*pEndianness != 'b'))) {
            success = false;
            printf("Endianness must be l for little or b for big (not %s).\n", pEndianness);
        }
        // Check that the word width makes sense
        if ((wordWidth <= 0) && (wordWidth > 4)) {
            success = false;
            printf("Word width must be 1, 2, 3, or 4 (not %d).\n", wordWidth);
        }
        // Open the input file
        pInputFile = fopen (pInputFileName, "rb");
        if (pInputFile == NULL) {
            success = false;
            printf("Cannot open input file %s (%s).\n", pInputFileName, strerror(errno));
        }
        // Create the output file name if we don't have one, by
        // removing the extension from the input file name and adding the desired extension
        if (pOutputFileName == NULL) {
            pChar = strtok(pInputFileName, EXT_SEPARATOR);
            pOutputFileName = (char *) malloc (strlen(pChar) + sizeof(OUTPUT_FILE_EXTENSION) - 1 + sizeof(EXT_SEPARATOR) - 1 + 1);
            if (pOutputFileName != NULL) {
                outputFileNameMalloced = true;
                strcpy(pOutputFileName, pInputFileName);
                strcat(pOutputFileName, EXT_SEPARATOR);
                strcat(pOutputFileName, OUTPUT_FILE_EXTENSION);
            } else {
                success = false;
                printf("Cannot allocate memory for output file name.\n");
            }
        }
        // Open the output file
        if (pOutputFileName != NULL) {
            pOutputFile = fopen(pOutputFileName, "w");
            if (pOutputFile == NULL) {
                success = false;
                printf("Cannot open output file %s (%s).\n", pOutputFileName, strerror(errno));
            }
        }
        if (success) {
            printf("Parsing of file %s starting, %s endian with %d byte words and writing output to %s.\n",
                   pInputFileName, *pEndianness == 'l' ? "little" : "big", wordWidth, pOutputFileName);
            x = parse(pInputFile, pOutputFile, wordWidth, *pEndianness == 'l' ? true : false);
            printf("Done: %d item(s) written to file.\n", x);
        } else {
            printUsage(pExeName);
        }
    } else {
        printUsage(pExeName);
    }

    if (success) {
        retValue = 0;
    }

    // Clean up
    if (pInputFile != NULL) {
        fclose(pInputFile);
    }
    if (pOutputFile != NULL) {
        fclose(pOutputFile);
    }
    if (outputFileNameMalloced) {
        free(pOutputFileName);
    }

    return retValue;
}