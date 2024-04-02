#include  "iobuf.h"

void IOBuf_Init(IO_BUF *b)
{
  IOBuf_ReInit(b);
  
  for(INT16U i = 0; i < IOBUFMSK; i++)
  {
    b->data[i] = 0;
  }
}

void IOBuf_ReInit(IO_BUF *b)
{
  b->pp = 0;
  b->gp = 0;
  b->length = 0;
}

bool IOBuf_PutC(IO_BUF *b, INT8U c)
{
  if (b->length >= IOBUFMSK)
    return false;
  else
  {
    b->data[b->pp] = c;
    b->pp = increase(b->pp, 0, IOBUFMSK - 1);
    b->length++;
    
    return true;
  }
}

bool IOBuf_PutBuf(IO_BUF *b, INT8U *cbuf, INT16U size)
{
  INT16U i = 0;
  INT16U tmp = size;
  
  if (size == 0)
    return false;
  
  if ((b->length + size) > IOBUFMSK)
    return false;
  
  while (size--)
    if (IOBuf_PutC(b, *(cbuf++)))
      i++;
  
  if (i != tmp)
    return false;
  else
    return true;
}

bool IOBuf_GetC(IO_BUF *b, INT8U *c)
{
  if (b->length == 0)
    return false;
  else
  {
    *c = b->data[b->gp];
    b->gp = increase(b->gp, 0, IOBUFMSK - 1);
    b->length--;
    
    return true;
  }
}

bool IOBuf_PeekC(IO_BUF *b, INT8U *c)
{
  if(b->length == 0)
    return false;
  else
  {
    *c = b->data[b->gp];
    
    return true;
  }
}

bool IOBuf_GetBuf(IO_BUF *b, INT8U *cbuf, INT16U size)
{
  INT16U i = 0;
  INT16U tmp = size;
  
  if(size == 0)
    return false;
  
  if (b->length < size)
    return false;
  
  while (size--)
    if (IOBuf_GetC(b, cbuf++))
      i++;
  
  if (i != tmp)
    return false;
  else
    return true;
}

INT16U IOBuf_Size(IO_BUF *b)
{
  return b->length;
}
