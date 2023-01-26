#include "bs3_asm.h"

const char * bs3_asm_message[]=
{
  [BS3_ASM_PASS1_PARSE_ERR_OK]             = "Parse succeeded",
  [BS3_ASM_PASS1_PARSE_ERR_NOPE]           = "Nothing to parse",
  [BS3_ASM_PASS1_PARSE_ERR_FAIL]           = "General error",
  [BS3_ASM_PASS1_PARSE_ERR_LINETOOLONG]    = "Line too long (max 72 characters)",
  [BS3_ASM_PASS1_PARSE_ERR_EMPTYLABEL]     = "Empty label",
  [BS3_ASM_PASS1_PARSE_ERR_BADLABEL]       = "Bad label",
  [BS3_ASM_PASS1_PARSE_ERR_BADOPE]         = "Bad operation",
  [BS3_ASM_PASS1_PARSE_ERR_BADPARAM]       = "Bad parameter",
  [BS3_ASM_PASS1_PARSE_ERR_BADSTRING]      = "Bad string parameter",
  [BS3_ASM_PASS1_PARSE_ERR_BADCHAR]        = "Bad one character parameter",
  [BS3_ASM_PASS1_PARSE_ERR_BADNUMBER]      = "Bad decimal parameter",
  [BS3_ASM_PASS1_PARSE_ERR_BADREGISTER]    = "Bad register parameter",
  [BS3_ASM_PASS1_PARSE_ERR_BADSYMBOL]      = "Bad symbol parameter",
  [BS3_ASM_PASS1_PARSE_ERR_KEYWORD]        = "Keyword used in parameter",
  [BS3_ASM_PASS1_PARSE_ERR_ADDRMODE]       = "Addressing mode error",
  [BS3_ASM_PASS1_PARSE_ERR_BIGVALUE]       = "Value too big (> 65535) or too small (< -32768)",
  [BS3_ASM_PASS1_PARSE_ERR_NOALIAS]        = "Missing alias identification",
  [BS3_ASM_PASS1_ERR_BADFILE]              = "Bad file name or file not found",
  [BS3_ASM_PASS1_PARSE_ILLEGALCHAR]        = "Illegal character",
  [BS3_ASM_PASS1_PARSE_ERR_SYMBOLNOTFOUND] = "Symbol not found",
  [BS3_ASM_PASS1_PARSE_ERR_SYMBOLTOOBIG]   = "Symbol value to large",
  [BS3_ASM_PASS1_PARSE_ERR_BADOPETYPE]     = "Internal error, bad operation type",
  [BS3_ASM_PASS1_PARSE_ERR_BADOPESYNTAX]   = "Incorrect instruction parameter",
  [BS3_ASM_PASS1_PARSE_ERR_UNEXPECTED]     = "Unexpected error",
  [BS3_ASM_PASS1_PARSE_ERR_BADBYTE]        = "Value not compatible with byte",
  [BS3_ASM_PASS1_PARSE_ERR_MEMORY]         = "Too big program",
  [BS3_ASM_PASS1_PARSE_ERR_NOLABEL]        = "Missing mandatory label",
  [BS3_ASM_PASS1_PARSE_ERR_INCLUDE]        = "Include directive failed",
  [BS3_ASM_PASS1_PARSE_ERR_BEYOND]         = "Instruction address beyond 64k",
  [BS3_ASM_PASS1_FAILURE]                  = "Pass 1 failure",
  [BS3_ASM_PASS1_PARSE_ERR_DUPLABEL]       = "Duplicate label",
  [BS3_ASM_PASS1_PARSE_ERR_EXPMACRO]       = "Unknown operation or macro",
  [BS3_ASM_PASS1_PARSE_ERR_MACROSYNTAX]    = "Bad macro parameter designation",
  [BS3_ASM_PASS1_PARSE_ERR_MACROREFPARAM]  = "Unknown macro parameter index",
  [BS3_ASM_PASS1_PARSE_ERR_MACROLINE2BIG]  = "Line generated by macro is too large",
  [BS3_ASM_PASS1_PARSE_ERR_ENDMNOTFOUND]   = "ENDM not found",
  [BS3_ASM_PASS1_PARSE_ERR_ASMOVERLAP]     = "Assembly over other assembly",
  [BS3_ASM_PASS2_FAILURE]                  = "Pass 2 failure",
  [BS3_ASM_PASS2_ERR_UNEXPECTED]           = "Pass 2 unexpected error",
  [BS3_ASM_PASS2_ERR_LABELNOTFOUND]        = "Pass 2 label not found",
  [BS3_ASM_PASS2_ERR_LABEL2FAR]            = "Pass 2 label too far",
  [BS3_ASM_PASS1_PARSE_ERR_BADALIGN]       = "Incorrect or not a power of two 'align' directive",
  [BS3_ASM_PASS1_PARSE_ERR_TOOBIGSPACE]    = "Incorrect value or not enough address for 'space' directive"
};
