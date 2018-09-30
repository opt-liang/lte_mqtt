#ifndef __PUTINFOR_H__
#define __PUTINFOR_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include "FIFO.h"

#define  PRINTF_QUEUE_LEN       1000   // unit: byte
#define  PRINTF_QUEUE_UNIT      1     // unit: byte

//#define  RTT_FREERTOS_ENABLE
#define  _FREERTOS_MODE_PRINTF_

#define  _printf                SYS_printf

typedef enum
{
  _AUTO_,
	_HEX_,
	_ASC_,
}TpModeEnum;

typedef enum
{
  TASK_CREATE_OK,
  TASK_CREATE_FAIL,
  TASK_RUN,
  TASK_DELETE
}TaskStatusEnum;
typedef struct 
{
  uint8_t      *Buf;
  Fifo_Typedef Fifo;
  bool         State;
  bool         Semaphore;
}PrintfCtrlTyp;

void PrintfInit(void);
void _Infor(char *s, uint8_t *tdata, uint16_t len, TpModeEnum mode);

#if defined(RTT_FREERTOS_ENABLE)
 void appSEGGER_RTT_Init(void);
 void TaskStatusInfor(char *name,TaskStatusEnum status);
 void Task_ErrorHandler(char *name,char * file, int line);
 void vPrvRTT_ScanTask(void * pvParameters);
#endif

#endif
