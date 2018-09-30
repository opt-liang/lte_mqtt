#include "putinfor.h"
#include <stdio.h>
#include <time.h>

#if defined(_FREERTOS_MODE_PRINTF_)
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
static TaskHandle_t           PrintfTaskHandle;
static SemaphoreHandle_t      printf_SemaphoreHandle; 
static PrintfCtrlTyp          PrintfCtrl = {NULL};
#endif

/////////////////// KEIL DEBUG ////////////////////
 /* definition for Debug (printf) view */
#define ITM_Port8(n)   (*((volatile unsigned char*)(0xE0000000+4*n)))
#define ITM_Port16(n)  (*((volatile unsigned short*)(0xE0000000+4*n)))
#define ITM_Port32(n)  (*((volatile unsigned long*)(0xE0000000+4*n)))
#define DEMCR          (*((volatile unsigned long*)(0xE000edfc)))
#define TRCENA         0x01000000
 struct __FILE{int handle;		/*Add whatever you need here*/};
 FILE __stdout;
 FILE __STDIN;
 
int fputc(int ch, FILE *f)
{
	/* printf for Debug (printf) view */
	if(PrintfCtrl.State==false)
	{
		if(DEMCR & TRCENA)
		{
			while(ITM_Port32(0) == 0);
			ITM_Port8(0)=ch;
		}
  } else
    {
      FifoPush(&PrintfCtrl.Fifo, (uint8_t)ch);
      if(PrintfCtrl.Semaphore==false) 
      {
        PrintfCtrl.Semaphore = true;
        xSemaphoreGive(printf_SemaphoreHandle);
      }
    }
    
	return ch;
}

static void _swoprintf(char ch)
{

	if(DEMCR & TRCENA)
	{
		while(ITM_Port32(0) == 0);
		ITM_Port8(0) = (uint8_t)ch;
	}
}

static void SWO_printf(void)
{
  uint8_t temp;


  do
  {
    while(IsFifoEmpty(&PrintfCtrl.Fifo)==false)
    {
      temp = FifoPop(&PrintfCtrl.Fifo);
      
      _swoprintf(temp);
    }

    vTaskDelay(10);
    
  } while( IsFifoEmpty(&PrintfCtrl.Fifo)==false );
}

//////////////////////// DATA FORMAT RTT INFOR //////////////////////////////

#if defined(_FREERTOS_MODE_PRINTF_)

static void uPrintfHandleTask(void * argument)
{
  for(;;)
  {
	  xSemaphoreTake(printf_SemaphoreHandle, portMAX_DELAY);
	  PrintfCtrl.Semaphore = false;
    SWO_printf();
  }
}

void PrintfInit(void)
{	
  PrintfCtrl.Buf = (uint8_t *)pvPortMalloc((size_t)PRINTF_QUEUE_LEN);
  if(PrintfCtrl.Buf==NULL)
  {
    printf("printf Malloc fail\r\n");
    return;
  }
  
  FifoInit(&PrintfCtrl.Fifo, PrintfCtrl.Buf, PRINTF_QUEUE_LEN);

  if(xTaskCreate(uPrintfHandleTask, "printf", 200, NULL, 1, PrintfTaskHandle)!=pdPASS)
  {
    printf("printf create fail\r\n");
    return;
  }

  printf_SemaphoreHandle = xSemaphoreCreateBinary();
  if(printf_SemaphoreHandle==NULL)
  {
    printf("printf Semaphore create fail\r\n");
    return;
  }
  
  PrintfCtrl.State = true;
}

#endif

void _Infor(char *s, uint8_t *tdata, uint16_t len, TpModeEnum mode)
{
  uint16_t i = 0,tsize = len;
  bool     type = true;
	
	switch(mode)
	{
	  case _AUTO_:
			{
				if(len!=0) 
				{
					while(i<len)
					{
						if(tdata[i]==0||tdata[i]>=0x7F) 
						{
							type = false;
							break;
						}
						i++;
					}
				} else
				  {
            while(tdata[i]!='\0')
            {
              if(tdata[i]>=0x7F) break;
              i++;
            }
				  }
			} break;
		case _HEX_:
			{ 
				type = false;
			} break;
		case _ASC_:
			{
				type = true;
				if(len==0)
				{
					i = 0;
					while(tdata[i]!='\0')
					{
						if(tdata[i]>=0x7F) break;
						i++;
					}
				} else i = len;
			} break;
		default: break;
	}
  
  if(type==false)
  { 
    tsize = len;
	  printf("%s[%u]<HEX>:",s,tsize);
	  for(i = 0; i<tsize; i++) printf("%02X ",tdata[i]);
  } else
    {
      tsize = i;
		  printf("%s[%u]<ASC>:",s,tsize);
		  for(i = 0; i<tsize; i++) printf("%c",tdata[i]);
    }

  printf("\r\n");
}


//////////////////////// J-LINK RTT DEBUG //////////////////////////////
#if defined(RTT_FREERTOS_ENABLE)
#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t    RTT_GetCmdHandle;

void TaskStatusInfor(char *name,TaskStatusEnum status)
{
  switch(status)
  {
    case TASK_CREATE_OK:
	    {
//				SEGGER_RTT_printf(0,"%sC-TASK: %s%s\r\n",RTT_CTRL_TEXT_CYAN,RTT_CTRL_RESET,name);
        break;
	    }
    case TASK_CREATE_FAIL:
      {
//				SEGGER_RTT_printf(0,"%sF-TASK: %s%s\r\n",RTT_CTRL_TEXT_BRIGHT_RED,RTT_CTRL_RESET,name);
        break;
      }
    case TASK_RUN:
      {
//				SEGGER_RTT_printf(0,"R-TASK: %s\r\n",name);
        break;
      }
    case TASK_DELETE:
      {
//				SEGGER_RTT_printf(0,"%sD-TASK: %s%s\r\n",RTT_CTRL_TEXT_BLUE,RTT_CTRL_RESET,name);
        break;
      }
    default: break;
  }
}

void Task_ErrorHandler(char *name,char * file, int line)
{
  TaskStatusInfor(name,TASK_CREATE_FAIL);
//  SEGGER_RTT_printf(0,"   %sErr-Location:[FILE]:%s [LINE]:%d\r\n%s",RTT_CTRL_TEXT_BRIGHT_YELLOW,file,line,RTT_CTRL_RESET);
}

__weak void Debug_API_CmdRTT(char *fdata, uint8_t len)
{

}

void uPrvRTT_ScanTask(void * pvParameters)
{
  int     c;
  char    buf[16];
  uint8_t i = 0,j;

  for(j=0;j<16;j++) buf[j] = 0;
  TaskStatusInfor("RTT-SCAN",TASK_RUN);

  while(1)
  {
    i = 0;
    while(SEGGER_RTT_HasKey())
    {
		  c = (char)SEGGER_RTT_GetKey();
      buf[i++] = c;
			if(c==0x0A||i>=15) 
			{
				// 处理
				Debug_API_CmdRTT(buf,i);
//				SEGGER_RTT_printf(0,"%s",buf);
//				for(j=0;j<16;j++) buf[j] = 0;
				break;
			}
    }
    vTaskDelay(1000);
  }
}


void appSEGGER_RTT_Init(void)
{
  SEGGER_RTT_ConfigUpBuffer(0, "debug", NULL, 0, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL); 
  SEGGER_RTT_printf(0,"startup\r\n");
 #if defined(RTT_FREERTOS_ENABLE)
  xTaskCreate(uPrvRTT_ScanTask, "RTT_CMD", 100, NULL, 1,&RTT_GetCmdHandle);
  if(RTT_GetCmdHandle==NULL) SEGGER_printf("RTT_GetCmdHandle create fail\r\n");
 #endif
}

#endif


