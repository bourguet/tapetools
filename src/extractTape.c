/* $Id: extractTape.c,v 1.3 2010/09/19 16:37:28 jm Exp $
 * Copyright (C) 2004, 2009  Jean-Marc Bourguet
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 *   * Neither the name of Jean-Marc Bourguet nor the names of other
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * A program to extract files from TCP, TPS and TPE tape image formats
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum formatKind { fmtGuess, fmtTPS, fmtTPE, fmtTPC } formatKind;

char const* progname;
char const* inputFileName = 0;
int startFile = 1;
formatKind format;

void help()
{
  fprintf(stderr,
          "Usage: %s [-s n] [-S|-E|-C] -f file files....\n"
          "       -f file  file containing tape dump\n"
          "       -s n     start at file n (default 1)\n"
          "       -S       TPS (TAP, simh) format\n"
          "       -E       TPE (E11) format\n"
          "       -C       TPC format\n"
          "       files... names for files to be saved\n",
          progname);
}

void fatal(char const* fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}


int parseInt(char const* val)
{
  int result;
  char* end;
  
  result = strtol(val, &end, 0);
  if (*end != '\0') {
    fatal("'%s' is not a valid number.\n", val);
  }
  return result;
}

void parseArgs(int argc, char* argv[])
{
  int c;

  progname = argv[0] != 0 ? argv[0] : "extractTape";

  while ((c = getopt(argc, argv, "hsSECf:")) != EOF) {
    switch((char)c) {
    case 's':
      startFile = parseInt(optarg);
      break;
    case 'S':
      format = fmtTPS;
      break;
    case 'E':
      format = fmtTPE;
      break;
    case 'C':
      format = fmtTPC;
      break;
    case 'f':
      inputFileName = optarg;
      break;
    case 'h':
      help();
      exit(EXIT_SUCCESS);
    case '?':
      exit(EXIT_FAILURE);
    }
  }

  if (format == fmtGuess && inputFileName == 0) {
    fatal("Format needed when taking input from standard input.\n");
  }

  if (format == fmtGuess) {
    char* suffix = strrchr(inputFileName, '.');
    if (suffix == 0) {
      fatal("No suffix in '%s', unable to guess format.\n", inputFileName);
    } 
    if (strcmp(suffix, ".tps") == 0) {
      format = fmtTPS;
    } else if (strcmp(suffix, ".tap") == 0) {
      format = fmtTPS;
    } else if (strcmp(suffix, ".tpe") == 0) {
      format = fmtTPE;
    } else if (strcmp(suffix, ".tpc") == 0) {
      format = fmtTPC;
    } else {
      fatal("'%s' is not a known suffix.\n", suffix);
    }
  }
}

FILE* openInput(char const* fileName)
{
  FILE* result;
  
  if (fileName == 0)
    return stdin;
  result = fopen(fileName, "rb");
  if (result == 0) {
    fatal("Unable to open '%s'.\n", fileName);
  }
  
  return result;
}

FILE* openOutput(char const* fileName)
{
  FILE* result;
  
  result = fopen(fileName, "wb");
  if (result == 0) {
    fatal("Unable to open '%s'.\n", fileName);
  }
  
  return result;
}

size_t readOffset(FILE* input)
{
  if (format == fmtTPC) {
    unsigned char buffer[2];
    size_t sizeRead = fread(buffer, 1, 2, input);
    
    if (sizeRead == 0) {
      fatal("EOF while reading record length.\n");
    }
    if (sizeRead != 2) {
      fatal("Incomplete record length.\n");
    }

    return buffer[0] + 256 * buffer[1];
  } else {
    unsigned char buffer[4];
    size_t sizeRead = fread(buffer, 1, 4, input);

    if (sizeRead == 0) {
      fatal("EOF while reading record length.\n");
    }
    if (sizeRead != 4) {
      fatal("Incomplete record length.\n");
    }
    
    return buffer[0] + 256 * (buffer[1] + 256 * (buffer[2] + 256 * buffer[3]));
  }
}

void skipFile(FILE* input)
{
  size_t offset;
  
  fprintf(stderr, "Skipping file.\n");
  do {
    offset = readOffset(input) & 0x7FFFFFFF;

    if ((format != fmtTPE) && ((offset & 1) == 1)) {
      ++offset;
    }
    if (fseek(input, offset, SEEK_CUR) == EOF) {
      fatal("Unable to fseek over record.\n");
    }
    if (format != fmtTPC) {
      size_t backOffset = readOffset(input) & 0x7FFFFFFF;
      if ((format != fmtTPE) && ((backOffset & 1) == 1)) {
        ++backOffset;
      }
      if (offset != backOffset)
      {
        fatal("End record size different for start record size.\n");
      }
    }
  } while (offset != 0);
}

void copyFile(FILE* input, FILE* output, size_t length)
{
  char buffer[BUFSIZ];

  while (length != 0) {
    size_t toCopy = (length > BUFSIZ ? BUFSIZ : length);
    size_t offset = 0;

    offset = 0;
    do {
      size_t sizeRead;
      sizeRead = fread(buffer+offset, 1, toCopy-offset, input);
      if (sizeRead == 0) {
        fatal("Reaching EOF when copying file.\n");
      }
      offset += sizeRead;
    } while(offset != toCopy);
    offset = 0;
    do {
      size_t sizeWritten;
      sizeWritten = fwrite(buffer+offset, 1, toCopy-offset, output);
      if (sizeWritten == 0) {
        fatal("Error while writing.\n");
      }
      offset += sizeWritten;
    } while (offset != toCopy);
    length -= toCopy;
  }
}

void extractFile(FILE* input, FILE* output)
{
  size_t offset;

  do {
    offset = readOffset(input);
    if ((offset & 0x80000000) != 0) {
      fprintf(stderr, "Warning: erroneous record read.\n");
      offset &= 0x7FFFFFFF;
    }

    copyFile(input, output, offset);

    if ((format != fmtTPS) && ((offset & 1) == 1)) {
      if (fseek(input, 1, SEEK_CUR) == EOF) {
        fatal("Unable to seek over padding byte.\n");
      }
    }
    if (format != fmtTPC) {
      size_t backOffset = readOffset(input);
      if ((backOffset & 0x7FFFFFFF) != offset) {
        fatal("End record size different from start of record size.\n");
      }
    }
  } while (offset != 0);
}

int main(int argc, char* argv[])
{
  FILE* inputFile;
  
  parseArgs(argc, argv);
  inputFile = openInput(inputFileName);

  while (startFile != 1) {
    skipFile(inputFile);
    --startFile;
  }

  if (optind == argc) {
    fprintf(stderr, "Saving to standard output.\n");
    extractFile(inputFile, stdout);
  } else {
    while (optind < argc) {
      FILE* outputFile = openOutput(argv[optind]);
      
      fprintf(stderr, "Saving to '%s'.\n", argv[optind]);
      extractFile(inputFile, outputFile);
      if (fclose(outputFile) == EOF) {
        fprintf(stderr, "Unable to close '%s' properly.\n", argv[optind]);
        exit(EXIT_FAILURE);
      }
      ++optind;
    }
  }
  return EXIT_SUCCESS;
}
