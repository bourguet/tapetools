/* $Id: buildTape.c,v 1.3 2010/09/19 16:37:28 jm Exp $
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
 *   * Neither the name of Jean-Marc Bourguet nor the names of the other
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
 * A program to build TPC, TPS and TPE tape images from files.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum formatKind { fmtGuess, fmtTPS, fmtTPE, fmtTPC } formatKind;

char const* progname;
char const* outputFileName = 0;
int blockSize = 2590;
formatKind format;

void help()
{
  fprintf(stderr,
          "Usage: %s [-b size][-S|-E|-C] -o file files....\n"
          "       -o file  file containing tape dump\n"
          "       -b block block size\n"
          "       -S       TPS (TAP, simh) format\n"
          "       -E       TPE (E11) format\n"
          "       -C       TPC format\n"
          "       files... files to be copied\n",
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

  while ((c = getopt(argc, argv, "hSECo:")) != EOF) {
    switch((char)c) {
    case 'S':
      format = fmtTPS;
      break;
    case 'E':
      format = fmtTPE;
      break;
    case 'C':
      format = fmtTPC;
      break;
    case 'o':
      outputFileName = optarg;
      break;
    case 'h':
      help();
      exit(EXIT_SUCCESS);
    case '?':
      exit(EXIT_FAILURE);
    }
  }

  if (format == fmtGuess && outputFileName == 0) {
    fatal("Format needed when outputting to from standard output.\n");
  }

  if (format == fmtGuess) {
    char* suffix = strrchr(outputFileName, '.');
    if (suffix == 0) {
      fatal("No suffix in '%s', unable to guess format.\n", outputFileName);
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
      fatal("'%s' is not a known suffix, unable to guess format.\n", suffix);
    }
  }
}

FILE* openInput(char const* fileName)
{
  FILE* result;
  
  result = fopen(fileName, "rb");
  if (result == 0) {
    fatal("Unable to open '%s'.\n", fileName);
  }
  
  return result;
}

FILE* openOutput(char const* fileName)
{
  FILE* result;

  if (fileName == 0)
    return stdout;
  result = fopen(fileName, "wb");
  if (result == 0) {
    fatal("Unable to open '%s'.\n", fileName);
  }
  
  return result;
}

void writeToFile(char* buffer, size_t length, FILE* output)
{
  size_t offset = 0;
  while (offset != length) {
    size_t sizeWritten;

    sizeWritten = fwrite(buffer+offset, 1, length-offset, output);
    if (sizeWritten == 0) {
      fatal("Error while writing (written %d asked for %d).\n",
            offset+sizeWritten, length);
    }
    offset += sizeWritten;
  } 
}

void putFile(FILE* input, FILE* output)
{
  static char* buffer = 0;
  size_t offset;
  
  if (buffer == 0) {
    buffer = malloc(blockSize);
  }
  
  do {
    size_t toCopy = blockSize;
    char signature[4];
    size_t sizeRead;
    
    offset = 0;
    do {
      sizeRead = fread(buffer+offset, 1, toCopy-offset, input);
      offset += sizeRead;
    } while ((offset != toCopy) && (sizeRead != 0));
    signature[0] = offset % 256;
    signature[1] = (offset / 256) % 256;
    signature[2] = (offset / 256 / 256) % 256;
    signature[3] = (offset / 256 / 256 / 256) % 256;

    writeToFile(signature, (format == fmtTPC ? 2 : 4), output);
    writeToFile(buffer, offset, output);
    if ((format != fmtTPE) && ((offset & 1) == 1)) {
      if (fputc(0, output) == EOF) {
        fatal("Unable to write padding byte.\n");
      }
    }
    if (format != fmtTPC) {
      writeToFile(signature, 4, output);
    }
  } while (offset != 0);
}

int main(int argc, char* argv[])
{
  FILE* outputFile;
  
  parseArgs(argc, argv);
  outputFile = openOutput(outputFileName);

  if (optind == argc) {
    fprintf(stderr, "Copying standard input.\n");
    putFile(stdin, outputFile);
  } else {
    while (optind < argc) {
      FILE* inputFile = openInput(argv[optind]);
      
      fprintf(stderr, "Copying '%s'.\n", argv[optind]);
      putFile(inputFile, outputFile);
      fclose(inputFile);
      ++optind;
    }
  }
  if (outputFileName != 0) {
    if (fclose(outputFile) == EOF) {
      fatal("Unable to close '%s' properly.\n", outputFileName);
    }
  }
  
  return EXIT_SUCCESS;
}
