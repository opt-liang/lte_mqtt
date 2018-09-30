#include "FIFO.h"

/* 初始化队列参数，使用队列前必须先初始化 */
void FifoInit( Fifo_Typedef *fifo, uint8_t *buffer, uint16_t size )
{
    fifo->Begin = CLEAR;
    fifo->End = CLEAR;
    fifo->Buf = buffer;
    fifo->Size = size;
}

/* 指向下一个位置 */
uint16_t FifoNext( Fifo_Typedef *fifo, uint16_t index )
{
    return (( index + 1 ) % fifo->Size);
}

uint16_t FifoBefore( Fifo_Typedef *fifo, uint16_t index )
{
	return (( index + fifo->Size - 1 ) % fifo->Size);
}

/* 数据向队列里填充 */
bool FifoPush( Fifo_Typedef *fifo, uint8_t data )
{
    if(IsFifoFull(fifo)==true) return false;
    
    fifo->End = FifoNext( fifo, fifo->End );
    fifo->Buf[fifo->End] = data;

    return true;
}

/* 读出队列要读出的一个数据 */
uint8_t FifoPop( Fifo_Typedef *fifo )
{
    if(fifo->Begin==fifo->End) return 0;
    
    uint8_t data = fifo->Buf[FifoNext( fifo, fifo->Begin )];

    fifo->Begin = FifoNext( fifo, fifo->Begin );
    return data;
}

/* 队列溢出清空，重新开始 */
void FifoFlush( Fifo_Typedef *fifo )
{
    fifo->Begin = 0;
    fifo->End = 0;
}

/* 判断状态是否空闲 1(true):空闲   0:false */
bool IsFifoEmpty( Fifo_Typedef *fifo )
{
    return ( fifo->Begin == fifo->End );
}

/* 判断状态是否满了 1(true):满了   0:false */
bool IsFifoFull( Fifo_Typedef *fifo )
{
    return ( FifoNext( fifo, fifo->End ) == fifo->Begin );
}

/* 计算缓存的数量*/
uint16_t FifoCacheNum( Fifo_Typedef *fifo )
{
  uint16_t num;
  
  if(fifo->End < fifo->Begin)
  {
    num = fifo->Size - fifo->Begin + fifo->End;
  } else num = fifo->End - fifo->Begin;
  
  return num;
}

void FifoSkipNum(Fifo_Typedef *fifo, uint16_t size)
{
  if(fifo->Size - fifo->Begin <= size)
  { 
    fifo->Begin = size - ( fifo->Size - fifo->Begin );
  } else fifo->Begin += size;
}
