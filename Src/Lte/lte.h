#ifndef __LTE_H__
#define __LTE_H__
#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os.h"

typedef enum{
		CYCLE=0, OPENNETWORK, TCPCONN, TCPSEND, TCPCLOSE, QUERYTCP, QUERYNETWORK
}INDEX;

typedef struct{
    char    *HANDLE;
    char    CMD[64];
    short   LEN;
}SEND_PARA;

typedef enum
{
  CMD_OK       = 0x00U,
  CMD_ERROR    = 0x01U,
  CMD_BUSY     = 0x02U,
  CMD_TIMEOUT  = 0x03U
} CMD_STAT;

typedef enum{
    NET_CONNECTED,
    NET_DISCONN,
}MQTT_CONNECT_STAT;

typedef union uStatus_t{
    uint64_t Value;//uint64_t
    struct sFields{
        uint64_t STATEOFEXE:4;//执行是否成功
		uint64_t OPENNETWORK:4;
		uint64_t TCPCONN:4;
		uint64_t TCPSEND:4;
		uint64_t TCPCLOSE:4;
		uint64_t CLOSENETWORK:4;
		uint64_t ATE:4;
		uint64_t QUERYTCP:4;
		uint64_t QUERYNETWORK:4;
    }Fields;
}Status_t;


#define EXCEPTION_THRESHOLD     5
#define LTE_BAUD_RATE           921600
/**********************************************************************************************************/
/*TCP OPEN STATUS*/
#define NETALREADYOPEN		    0x01
#define NETOPENSUCCESS		    0x02
#define NETOPENFAILED		    0x03
#define NETALREADYCLOSE		    0x04
#define NETCLOSESUCCESS		    0x05
/*TCP STATUS*/

#define TCPCONNSUCCESS		0x01
#define TCPALREADYCONN		0x02
#define TCPALREADYCLOSE		0x03
#define SERVENOTSTART		0x04
#define NETNOTSTART			0x05
#define TCPIPERROR          0x06

/*TCP SEND STATUS*/
#define SENDSUCCESS         0x01
#define SERRORBYNETOFF		0x02
#define SERRORBYNOCONN		0x03
#define SERRORBY

/*TCP CLOSE STATUS*/
#define TCPALREADYCLSOE		0x01
#define TCPCLOSESUCCESS		0x02
/**********************************************************************************************************/

#define LTE_CMD_TIMEOUT     20000

void LteChangeState( INDEX x_ctrl, INDEX x_currentctrl );
bool NetConnState( void );
void NetReSetState( void );

extern TimerHandle_t   LTE_STATE_CHECK_TIMER_HANDLE ;
extern MQTT_CONNECT_STAT   NET_STATE;
void MqttReEnterQueue( void );
void LteInitReSet( void );
void MqttQueueFreeMem( void );
#endif










