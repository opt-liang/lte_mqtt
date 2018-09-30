#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef CLEAR
#define CLEAR    0
#endif

typedef struct
{
  uint8_t* volatile Buf;  // 数据缓存
  volatile uint16_t Size;  // 缓存大小
  volatile uint16_t End;   // 需要处理位置
  volatile uint16_t Begin; // 正在处理位置
}Fifo_Typedef;

uint16_t FifoNext( Fifo_Typedef *fifo, uint16_t index );
uint16_t FifoBefore( Fifo_Typedef *fifo, uint16_t index );
void     FifoInit( Fifo_Typedef *fifo, uint8_t *buffer, uint16_t size );
bool     FifoPush( Fifo_Typedef *fifo, uint8_t data );
uint8_t  FifoPop( Fifo_Typedef *fifo );
void     FifoFlush( Fifo_Typedef *fifo );
bool     IsFifoEmpty( Fifo_Typedef *fifo);
bool     IsFifoFull( Fifo_Typedef *fifo );
uint16_t FifoCacheNum( Fifo_Typedef *fifo );
void     FifoSkipNum(Fifo_Typedef *fifo, uint16_t size);

#endif
