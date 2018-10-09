/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - "commonalization" from prior samples and/or documentation extension
 *******************************************************************************/
#include "lte.h"
#include "lte_driver.h"
#include "cycle_queue.h"
#include <string.h>
#include <stdio.h>

#define MQTT_DEBUG  true
#if MQTT_DEBUG
    #define MQTT_INFO(fmt,args...)  printf(fmt, ##args)
#else
    #define MQTT_INFO(fmt,args...)
#endif

/*******************************************************************************/

uint8_t *recvindex = NULL;
DataType    queuetemp;

bool MQTT_CONNECT_ABNORMAL = false;

void MQTT_CONNECT_ABNORMAL_FLAG( void ){
    MQTT_CONNECT_ABNORMAL = true;
}

/*******************************************************************************/
int transport_open(char* host, int serverport )
{
    extern void LteInit( void );
    extern bool ServerAddrConfig( char *ip, int serverport, short localport );
    
    LteInitReSet();

    static uint8_t reboot_times = 0;

    if( reboot_times >= 3 ){
        reboot_times = 0;
        LteReboot();
    }
    
    LteInit();
    
    if( ServerAddrConfig( host, serverport, 1001 ) ){
        if( MQTT_CONNECT_ABNORMAL ){
            MQTT_CONNECT_ABNORMAL = false;
            LteChangeState( TCPCLOSE, CYCLE );
        }else{
            LteChangeState( QUERYNETWORK, CYCLE );
        }

        reboot_times++;

        uint16_t i = 0;
        while( NET_STATE != NET_CONNECTED && i <= 30000 && !MQTT_CONNECT_ABNORMAL ){
            osDelay( 1 );
            i ++ ;
        }
        if( NET_STATE == NET_CONNECTED ){
            reboot_times = 0;
            return 1;
        }
        LteChangeState( CYCLE, CYCLE );
        return -1;
    }else{
        MQTT_INFO("Server address error\r\n");
        return -1;
    }
}

/*******************************************************************************/
int transport_getdata(unsigned char* buf, int count){
     
    if( count >= 1024 ){
        return -1;
    }
    
    if( queuetemp.size == 0 && count != 0 ){
        int16_t delaycount = 5000;
        int8_t stat = QueueDelete(&seqCQueue, &queuetemp);
        while( NetConnState() && !stat && delaycount >= 0 ){
            stat = QueueDelete(&seqCQueue, &queuetemp);
            delaycount--;
            osDelay(1);
        }
        if( stat == 0 ){
            return -1;
        }
        recvindex = queuetemp.index;
    }
    
    if( count > queuetemp.size ){//这里存在bug，一般情况下是不会小于零的，实际运行却发现小于0
        recvindex = NULL;
        queuetemp.size = 0;
        return -1;
    }
    
    memcpy( buf, recvindex, count );
    recvindex = recvindex + count;
    queuetemp.size -= count;
    
    MQTT_INFO("bytes count %d\r\n", (int)count);
    
    return count;
}

/*******************************************************************************/

int transport_sendPacketBuffer(int sock, unsigned char* buf, int buflen)
{
    return 1;
}

/*******************************************************************************/

int transport_close(int sock)
{
    osDelay( 2500 );
    return 0;
}

/*******************************************************************************/
#if 0
static volatile int8_t front = 0;
static bool GroupPacketFlag = false;

bool MqttReadyToRead( void ){
    if( queuetemp.size != 0 || QueueNotEmpty( &seqCQueue ) ){
        return true;
    }
    return false;
}

bool isEmptyCache( void ){
    if( queuetemp.size == 0 ){
        GroupPacketFlag = true;
        return true;
    }else{
        GroupPacketFlag = false;
        return false;
    }
}

void ClearQueueCache( void ){
	queuetemp.size = 0;
}

void GetQueueFront( void ){
    front = seqCQueue.front;
}

void SetQueueFront( void ){

	taskENTER_CRITICAL();
    
	ClearQueueCache();
    
	if( front == seqCQueue.front ){
        taskEXIT_CRITICAL();
		return;
	}
    
    int16_t count = 0;
    if( front > seqCQueue.front ){
        count = MaxQueueSize - front + seqCQueue.front ;
    }else{
        count = seqCQueue.front - front ;
    }
    
    if( GroupPacketFlag ){
        if( (( seqCQueue.count + count - 1 ) > MaxQueueSize ) ){
            taskEXIT_CRITICAL();
            return;
        }
		seqCQueue.front = ( front + 1 ) % MaxQueueSize;
		seqCQueue.count += count - 1;
	}else{
        if( (( seqCQueue.count + count ) > MaxQueueSize ) ){
            taskEXIT_CRITICAL();
            return;
        }
		seqCQueue.front = front;
		seqCQueue.count += count;
	}
    taskEXIT_CRITICAL();
}
#endif
/*******************************************************************************/
