#include <stdlib.h>
#include <stdio.h>

#if defined(BS3RUNTIME)
/* RUNTIME main */
#include "bs3.h"
int main(int argc, char *argv[]) 
{
  int i, err;
  struct bs3_asm_code_map codemap;
  codemap.dynamic =0;
  codemap.next = NULL;
  bs3_asm_code_map_reset(&codemap);
  for (i = 0 ; i < argc; i++)
  {
    if ((err = bs3_asm_code_map_load(argv[i],&codemap,0)) != BS3_ASM_CODE_MAP_ERR_OK)
    {
      if (err == BS3_ASM_CODE_MAP_ERR_LOADBADFILE && i == 0  && argc > 1) continue;
      fprintf(stderr,"Error on loading %s : %s\n",argv[i],bs3_asm_code_map_message[err]);
      return 1;
    }
  }
  bs3_hyper_main(&codemap,((void *)0)); /* Execute code with no debugger handler */
  bs3_asm_code_map_free(&codemap);
  return 0;
}

#else

/* ASSEMBLER / DEBUGGER  main */
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#include "bs3_asm.h"
#include "bs3_debug.h"

void bs3_usage(const char * prgname )
  {
    printf("Usage: %s [OPTION]... FILE \n", prgname);
    printf("Assembler / Debugger of BackSlashThree (BS3) CPU program.                      \n");
    printf("                                                                               \n");
    printf("  -b, --output-type-bin      BS3 result file in binary format (default)        \n");
    printf("  -d, --debug-stop           Debug result program after successful assembling  \n");
    printf("                             program is loaded stopped at first operation      \n");
    printf("  -D, --debug-run            Debug result program after successful assembling  \n");
    printf("                             load and run program                              \n");
    printf("  -e, --output-elf ELFFILE   Assembling result executable in ELFFILE           \n");
    printf("                             by default it is FILE without extension           \n");
    printf("  -h, --help                 This current help message.                        \n");
    printf("  -i, --include-path  PATH   Path to search for include directive              \n");
    printf("                             multiple -i argument is allowed                   \n");
    printf("  -k, --keep-tmpfile         Keep temporary generated file. Usefull to         \n");
    printf("                             troubleshoot deep nested macro compilation.       \n");
    printf("  -n, --debug-bindip         IP address to bind the debug listening socket,    \n");
    printf("                             127.0.0.1 is by default.                          \n");
    printf("  -o, --output-bs3 BS3FILE   Assembling BS3 result in BS3FILE                  \n");
    printf("                             by default it is FILE with .bs3 extension         \n");
    printf("  -p, --debug-port PORT      Remote debug listening port (default: 35853)      \n");
    printf("  -r, --output-report RFILE  Assembling report in RFILE                        \n");
    printf("                             by default it is FILE with .rpt extension         \n");
    printf("  -R, --nodebug-run          no debug, only run if possible.                   \n");
    printf("  -t, --temp-path TPATH      set temp path to TPATH. (default '/tmp')          \n");
    printf("  -x, --output-type-hexa     BS3 result file in hexadecimal format, human      \n");
    printf("                             readable binary with a text viewer.               \n");
    printf("                                                                               \n");
    printf("If FILE has an .s, .asm, .S or .ASM extension, it assembles FILE, otherwise it \n");
    printf("load it for debug.                                                             \n");
    printf("After successful assembling, you can debug it right away by using argument -d. \n");
    printf("Debug service is reachable at localhost (127.0.0.1) with port 35853 by default.\n");
    printf("Use a telnet alike program to interract with the debugger:                     \n");
    printf("       >telnet localhost 35853                                                 \n");
    printf("                                                                               \n");
    printf("Examples:                                                                      \n");
    printf("   >bs3asm myprog.asm                                                          \n");
    printf("     Generates 'myprog.bs3' binary file, 'myprog.rpt' text report,             \n");
    printf("     and 'myprog' ELF executable file.                                         \n");
    printf("                                                                               \n");
    printf("   >bs3asm -d -p 2300 myprog.asm                                               \n");
    printf("     If assembling completed successfully,                                     \n");
    printf("     then generates 'myprog.bs3' binary file, and 'myprog.rpt' text report.    \n");
    printf("     Starts debug on port 2300 of execution of 'myprog.bs3'                    \n");
    printf("                                                                               \n");
    printf("   >bs3asm --debug-run -p 2300 myprog.bs3                                      \n");
    printf("     Starts debug on port 2300 of execution of 'myprog.bs3'                    \n");
    printf("     Program is runnning (not stopped at first instruction)                    \n");
    printf("                                                                               \n");
    printf("   >bs3asm --debug-stop -p 2300 myprog                                         \n");
    printf("     Starts debug on port 2300 of execution of 'myprog' ELF program            \n");
    printf("     Program is stopped at first instruction                                   \n");
    printf("                                                                               \n");
  }


int main(int argc, char *argv[]) 
{
  struct bs3_asm_include_paths inc;
  int nbIncludePath;
  int toDebug;
  int noDebug;
  int debugStop;
  int toCompile;
  int keepTmpfile;
  char outputBS3[BS3_MAX_FILENAME_SIZE];
  char bindaddr[BS3_MAX_FILENAME_SIZE];
  char outputReport[BS3_MAX_FILENAME_SIZE];
  char tmppath[BS3_MAX_FILENAME_SIZE];
  char outputELF[BS3_MAX_FILENAME_SIZE];
  char inputFile[BS3_MAX_FILENAME_SIZE];
  char bs3rtFile[BS3_MAX_FILENAME_SIZE];
  char buffer[BS3_BUFFER_SIZE];
  int port;
  int c;
  int i;
  struct stat fs;
  FILE * targetELF;
  FILE * sourceFile;
  int bs3type = BS3_ASM_CODE_MAP_TYPE_BINARY;
  static const struct option long_options[] =
  {
    {"include-path",      1, 0, 'i'},
    {"output-type-hexa",  0, 0, 'x'},
    {"output-type-bin",   0, 0, 'b'},
    {"output-bs3",        1, 0, 'o'},
    {"output-report",     1, 0, 'r'},
    {"output-elf",        1, 0, 'e'},
    {"debug-bindip",      1, 0, 'n'},
    {"debug-stop",        0, 0, 'd'},
    {"debug-run",         0, 0, 'D'},
    {"keep-tmpfile",      0, 0, 'k'},
    {"nodebug-run",       0, 0, 'R'},
    {"debug-port",        1, 0, 'p'},
    {"temp-path",         1, 0, 't'},
    {"help",              0, 0, 'h'},
    {0, 0, 0, 0}
  };
  
  strcpy(outputBS3,"");
  strcpy(outputReport,"");
  strcpy(outputELF,"");
  strcpy(bindaddr,"127.0.0.1");
  strcpy(tmppath, "/tmp");
  port = 0; /* 0 = use default 35853 */
  nbIncludePath = 1;
  toDebug = 0;
  toCompile = 0;
  noDebug = 0;
  keepTmpfile = 0;
  inc.includePath[0][0] = 0; /* empty path */
  inc.includePath[1][0] = 0xFF; /* -1 is end of list of include */
  /* getopt_long stores the option index here. */
  int option_index;
  while (1)
  {
    option_index = 0;
    c = getopt_long (argc, argv, ":i:o:r:n:e:p:t:hbxdDRk",
                      long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c)
      {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if (long_options[option_index].flag != 0)
          break;

      case 'd':
        toDebug = 1;
        debugStop = 1;
        break;

      case 'D':
        toDebug = 1;
        debugStop = 0;
        break;

      case 'k':
        keepTmpfile = 1;
        break;

      case 'R':
        noDebug = 1;
        break;

      case 'h':
        bs3_usage(argv[0]);
        return 0;
        break;

      case 'b':
        bs3type = BS3_ASM_CODE_MAP_TYPE_BINARY;
        break;

      case 'x':
        bs3type = BS3_ASM_CODE_MAP_TYPE_HEXA;
        break;

      case 'i':
        if (strlen(optarg) > BS3_MAX_FILENAME_SIZE-32)
        {
          fprintf(stderr, "Too large include path %s\n", optarg);
          return 1;
        }
        if (nbIncludePath >= (BS3_MAX_INCLUDEPATH - 4) )
        {
          fprintf(stderr, "Too many include path\n");
          return 1;
        }
        strncpy(inc.includePath[nbIncludePath++], optarg, BS3_MAX_FILENAME_SIZE-1);
        inc.includePath[nbIncludePath-1][BS3_MAX_FILENAME_SIZE-1] = 0;
        inc.includePath[nbIncludePath][0] = 0xFF;
        inc.includePath[nbIncludePath][1] = 0;
        break;

      case 't':
        if (strlen(optarg) > BS3_MAX_FILENAME_SIZE-32)
        {
          fprintf(stderr, "Too large temporary path %s\n", optarg);
          return 1;
        }
        struct stat s = {0};
        if (!stat(optarg, &s))
        {
          // (s.st_mode & S_IFDIR) can be replaced with S_ISDIR(s.st_mode)
          if (! s.st_mode & S_IFDIR)
          {
            fprintf(stderr, "can't use %s for temporary path: not a directory.\n",optarg);
            return 1;
          }
        }
        else
        {
            fprintf(stderr, "can't use %s for temporary path: not found or can't access it\n", optarg);
            return 1;
        }
        if(access(optarg, W_OK) != 0)
        {
            fprintf(stderr,"Can't use %s for temporary path: not writable\n", optarg);
            return 1;
        }
        strcpy(tmppath, optarg);
        break;

      case 'o':
        strncpy(outputBS3, optarg,  BS3_MAX_FILENAME_SIZE-1);
        outputBS3[BS3_MAX_FILENAME_SIZE-1] = 0;
        break;

      case 'n':
        strncpy(bindaddr, optarg, BS3_MAX_FILENAME_SIZE-1);
        bindaddr[BS3_MAX_FILENAME_SIZE-1] = 0;
        break;

      case 'r':
        strncpy(outputReport, optarg,  BS3_MAX_FILENAME_SIZE-1);
        outputReport[BS3_MAX_FILENAME_SIZE-1] = 0;
        break;

      case 'e':
        strncpy(outputELF, optarg,  BS3_MAX_FILENAME_SIZE-1);
        outputELF[BS3_MAX_FILENAME_SIZE-1] = 0;
        break;

      case 'p':
        port  = atoi(optarg);
        break;

      case '?':
      case ':':
        /* getopt_long already printed an error message. */
        fprintf(stderr, "Incorrect provided parameter\n");
        bs3_usage(argv[0]);
        return 1;
        break;

      default:
        exit(1);
      }
  }

  if ((argc - optind) > 1)
  {
    fprintf(stderr, "Too many parameters\n");
    bs3_usage(argv[0]);
    return 1;
  }
  if ((argc - optind) == 1)
  {
      if (strlen(argv[optind]) > BS3_MAX_FILENAME_SIZE-1)
      {
        fprintf(stderr, "Too large file path %s\n",argv[optind]);
        return 1;
      }

      strncpy(inputFile, argv[optind], BS3_MAX_FILENAME_SIZE-1);
      inputFile[BS3_MAX_FILENAME_SIZE-1] = 0;
      i = strlen(inputFile);
      if (i>=2) /* possible .s or .S file, if yes then assemble input file */
      {
        if (inputFile[i-2] == '.' &&
            (inputFile[i-1] == 's' || inputFile[i-1] == 'S'))
        {
          toCompile = 1;
          i = i - 2;
        }
      }
      if (i>=4) /* possible .asm or .ASM file, if yes then assemble input file */
      {
        if (inputFile[i-4] == '.' && 
            (inputFile[i-3] == 'a' || inputFile[i-3] == 'A') &&
            (inputFile[i-2] == 's' || inputFile[i-2] == 'S') &&
            (inputFile[i-1] == 'm' || inputFile[i-1] == 'M')
        )
        {
          toCompile = 1;
          i = i - 4;
        }
      }
      if (!toCompile) 
      {
        toDebug = 1;
        strcpy(outputBS3, inputFile);
      }
      if (toCompile && outputBS3[0] == 0 ) /* manage default parameter if '-o' argument is not provided */
      {
          strcpy(outputBS3, inputFile);
          outputBS3[i] = 0;
          strcat(outputBS3,".bs3");
      }
      if (toCompile && outputReport[0] == 0 ) /* manage default parameter if '-r' argument is not provided */
      {
          strcpy(outputReport, inputFile);
          outputReport[i] = 0;
          strcat(outputReport,".rpt");
      }
      if (toCompile && outputELF[0] == 0 ) /* manage default parameter if '-r' argument is not provided */
      {
          strcpy(outputELF, inputFile);
          outputELF[i] = 0;
          strcpy(buffer,argv[0]);
          strcpy(bs3rtFile,dirname(buffer));
          strcat(bs3rtFile, "/bs3rt");
      }
      if (toCompile)   /* add include path where source file reside and where this binary path reside concatenated with "/include" */
      {
        strcpy(buffer, inputFile);
        strncpy(inc.includePath[nbIncludePath++], dirname(buffer), BS3_MAX_FILENAME_SIZE-32);
        inc.includePath[nbIncludePath-1][BS3_MAX_FILENAME_SIZE-32] = 0;
        strcpy(buffer,argv[0]);
        strncpy(inc.includePath[nbIncludePath++], dirname(buffer) , BS3_MAX_FILENAME_SIZE-32);
        strcat(inc.includePath[nbIncludePath - 1],"/include");
        inc.includePath[nbIncludePath-1][BS3_MAX_FILENAME_SIZE-32] = 0;
        inc.includePath[nbIncludePath][0] = 0xFF;
        inc.includePath[nbIncludePath][1] = 0;
        inc.size = nbIncludePath;
        /* be sure we have a trailing '/' at the end of each includePath */
        for (i =0; i < BS3_MAX_INCLUDEPATH; i++ )
        {
          if (inc.includePath[i][0] == (signed char)0xFF) break;
          if (inc.includePath[i][0] == 0) continue;
          if (inc.includePath[i][strlen(inc.includePath[i]) - 1] != '/') strcat(inc.includePath[i],"/");
        }
      } 
  }
  else
  {
    fprintf(stderr, "Missing parameter of BS3 source to assemble or binary file to debug\n");
    bs3_usage(argv[0]);
    return 1;
  }

  struct bs3_asm_code_map codemap;
  codemap.dynamic =0;
  codemap.next = NULL;
  if (toCompile)
  {
    bs3_asm_includepaths = &inc; /* provides the include paths */
    if (bs3_asm_file(inputFile, outputBS3, outputReport, bs3type, tmppath, keepTmpfile ) != BS3_ASM_PASS1_PARSE_ERR_OK) return 1;
    if (outputELF[0]!= 0) 
    {
      /* Generate the ELF file */
      targetELF = fopen(outputELF,"wb");
      if (targetELF)
      {
        i = 0;
        sourceFile = fopen(bs3rtFile,"rb");
        if (sourceFile)
        {
          while (!feof(sourceFile))
          {
            i = fread(buffer, 1, BS3_BUFFER_SIZE,sourceFile);
            fwrite(buffer, 1, i, targetELF);
          }
          fclose(sourceFile);
          sourceFile = fopen(outputBS3,"rb");
          if (sourceFile)
          {
            while (!feof(sourceFile))
            {
              i = fread(buffer, 1, BS3_BUFFER_SIZE, sourceFile);
              fwrite(buffer, 1, i, targetELF);
            }
            fclose (sourceFile);
            i = 1;
          } else i = 0;
        } else i = 0;
        fclose(targetELF);
        /* make outputELF file executable by everybody */
        if (i)
        {
          i = stat(outputELF,&fs);
          if( i==-1)
          {
              fprintf(stderr,"Can't get permission info of '%s'\n",outputELF);
              exit(1);
          }
          i = chmod( outputELF, fs.st_mode | S_IXUSR | S_IXGRP | S_IXOTH );
          if( i!=0)
          {
              fprintf(stderr,"Can't set executable permission on '%s'\n",outputELF);
              exit(1);
          }
        }

      }
    }
  }
  if (toDebug || noDebug)
  {
    if (!noDebug) bs3_asm_code_map_reset(&codemap);
    i = bs3_asm_code_map_load(outputBS3, &codemap,0);
    if ( i != BS3_ASM_CODE_MAP_ERR_OK) 
    {
      fprintf(stderr,"Error on loading %s : %s\n",outputBS3,bs3_asm_code_map_message[i]);
      return 1;
    }
    if (!noDebug) bs3_debug_prepare(bindaddr, port, debugStop); /* 0 for default port 35853*/
    bs3_hyper_main(&codemap,(noDebug)?((void *)0):&bs3_debug);
    bs3_asm_code_map_free(&codemap);
    if (!noDebug) bs3_debug_end();
  }
  return 0;
 
}
#endif