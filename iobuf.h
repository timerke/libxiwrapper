#ifndef _IOBUF_H
#define _IOBUF_H

#include <stdbool.h> // For bool
#include "types.h"
#include "macro.h"

// Firmware defines this as 256+1, but we extend it a little because of potential network-induced latency
#define IOBUFMSK 4000

#pragma pack(push, 1)

typedef struct
{
  INT16U length;
  INT8U data[IOBUFMSK];
  INT16U pp; // Put pointer
  INT16U gp; // Get pointer
  
} IO_BUF;

#pragma pack(pop)

void IOBuf_Init(IO_BUF *b);   // Empty buffer and fill it with zeroes
void IOBuf_ReInit(IO_BUF *b); // Empty buffer

bool IOBuf_PutC(IO_BUF *b, INT8U c);
bool IOBuf_PutBuf(IO_BUF *b, INT8U *cbuf, INT16U size);

bool IOBuf_PeekC(IO_BUF *b, INT8U *c);

bool IOBuf_GetC(IO_BUF *b, INT8U *c);
bool IOBuf_GetBuf(IO_BUF *b, INT8U *cbuf, INT16U size);

INT16U IOBuf_Size(IO_BUF *b);

#endif // _IOBUF_H
