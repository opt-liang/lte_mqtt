#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cycle_queue.h"
#include "uart_driver.h"
#include "lte_driver.h"
#include "mqtt_app.h"
#include "common.h"
#include "lte.h"
#include "timers.h"

#define LTE_DEBUG true
#if LTE_DEBUG
    #define LTE_INFO(fmt,args...)  printf(fmt, ##args)
#else
    #define LTE_INFO(fmt,args...)
#endif

/**********************************************************************************************************/

#define LTE_TIMER_WAIT 100

Status_t    LTE_STATE;
extern SEND_PARA   LTE_SEND_PARA;

uint8_t CMD_NO_ACK = 0;
bool LTE_INITED_STATE = false;
INDEX       LTE_CTRL  =  CYCLE;
bool LTE_ABNORMAL_STATE = false;
CMD_STAT    CMD_STATE    = CMD_OK;
INDEX       LTE_CURR_CTRL  =  CYCLE;
extern MQTT_CONNECT_STAT   NET_STATE;
char LTE_CONN_SERVER_CMD[ 64 ] = { 0 };

extern MqttSend_t  MqttSend;
osThreadId LTE_TASK_HANDLE = NULL;
TimerHandle_t LTE_STATE_CHECK_TIMER_HANDLE = NULL;

/**********************************************************************************************************/

void LteThread( void ){
    
    extern void LteCycle( void const * argument );
    extern void LteStateCheckCallback( TimerHandle_t xTimer );
    
    LTE_STATE_CHECK_TIMER_HANDLE = xTimerCreate((const char* )"checkstat", (TickType_t )LTE_CMD_TIMEOUT,(UBaseType_t )pdFALSE,(void* )2,(TimerCallbackFunction_t)LteStateCheckCallback);
    if( LTE_STATE_CHECK_TIMER_HANDLE == NULL ){
      LTE_INFO("LTE_STATE_CHECK_TIMER_HANDLE is NULL\r\n");
      while( true );
    }
    
    osThreadDef( checkLte, LteCycle, osPriorityNormal, 1, 512);
    LTE_TASK_HANDLE = osThreadCreate(osThread(checkLte), NULL);
    if( LTE_TASK_HANDLE == NULL ){
      LTE_INFO("create lte thread failed\r\n");
      while( true );
    }
}

void LteInit( void ){
    LteChangeState( CYCLE, CYCLE );
    xTimerStop( LTE_STATE_CHECK_TIMER_HANDLE, LTE_TIMER_WAIT );
    LTEINIT:
    set_lte_baud( LTE_BAUD_RATE );
    if( judge_sim_card() ){
        if( judge_lte_rssi() ){
            LTE_INITED_STATE = true;
        }else{
            goto LTEINIT;
        }
    }else{
        LTE_INFO("No SIM card\r\n");
        osDelay( 1000 );
        LteReboot();
        goto LTEINIT;
    }
}

bool ServerAddrConfig( char *ip_address, int serverport, short localport ){
    
    int a, b, c, d;
    if( sscanf( (const char*)ip_address,"%d.%d.%d.%d",&a,&b,&c,&d) != 0x04 ){
        return false;
    }
    if( islegalIP( a, b, c, d  ) == false ){
        return false;
    }
RECONFIG:
    LTE_CONN_SERVER_CMD[0] = '\0';
    if( sprintf( LTE_CONN_SERVER_CMD, "AT+IPOPEN=1,\"TCP\",\"%s\",%d,%d\r\n", ip_address, serverport, localport ) < 0 ){
        LTE_INFO( "Configuration server cmd executes an error\r\n" );
        osDelay( 1000 );
        goto RECONFIG;
    }
    return true;
}

void LteInitReSet( void ){
    LTE_INITED_STATE = false;
    LteChangeState( CYCLE, CYCLE );
}

/**********************************************************************************************************/

void LteStateCheckCallback( TimerHandle_t xTimer ){
    
    LTE_INFO("\r\n@@@@@@@@@@status timeous@@@@@@@@@@\r\n");
    
    LTE_ABNORMAL_STATE = false;

    if( LTE_CURR_CTRL != TCPSEND ){
        CMD_STATE = CMD_TIMEOUT;
        LteChangeState( LTE_CURR_CTRL, CYCLE );
    }else{
        MqttReEnterQueue();
    }
}

void LteChangeState( INDEX x_ctrl, INDEX x_currentctrl ){
    LTE_CURR_CTRL = x_currentctrl;
    LTE_CTRL = x_ctrl;
}

void LteChangeStateExapi( void ){
    if( LTE_INITED_STATE ){
        xTimerStart( LTE_STATE_CHECK_TIMER_HANDLE, LTE_TIMER_WAIT );
        LteChangeState( CYCLE, LTE_CTRL );
        CMD_STATE = CMD_BUSY;
        CMD_NO_ACK ++;
    }
}

void LteSendData( const char *pdata , short len ){
    UartSendData( &huart3, (uint8_t *)pdata, len );
    if( LTE_SEND_PARA.HANDLE ){
        vPortFree( LTE_SEND_PARA.HANDLE );
    }
    LTE_SEND_PARA.HANDLE = NULL;
    LTE_SEND_PARA.LEN = 0;
}

bool LteRecvData( const char* msg,uint16_t length ){

    const char *find = findstring( msg, "RECV", length );

    if( find == NULL ){
        return false;
    }else if( *( find - 1 ) == '+' && *( find + 4 ) == ':' && *( find + 7 ) == ',' ){
        find -= 1;
    }else{
        find = findstring( find+4, "+RECV: 1,", length - ( find - msg ) - 4 );
        if( find == NULL ){
            return true;
        }
    }

    find += 9;

    int len = 0;

    if( sscanf( find, "%d", &len ) == 0x01 ){

        if( len == 0 || len > 1024 ){
            LTE_INFO("cache value is error len is %d\r\n", len );
            return true;
        }

        int8_t cycle_count = 5;

        while( *find != 0x0D && --cycle_count ) find++;

        if( cycle_count == 0 ){
            return true;
        }

        find += 2;

        DataType cache;

        cache.size = len;

        cache.index = ( uint8_t *)find;

        taskENTER_CRITICAL();
        QueueAppend( &seqCQueue, cache );
        taskEXIT_CRITICAL();
    }
    return true;
}

void LteDealState( char * msg ,uint16_t len ){
    LTE_STATE.Value = 0;
    switch( LTE_CURR_CTRL ){

/***************************IPNETOPEN************************************/												
        case QUERYNETWORK:
        case OPENNETWORK:
            if( LTE_CURR_CTRL == OPENNETWORK ){
               if( findstring( msg, "+IPNETOPEN: 0" , len ) ){
                    LTE_STATE.Fields.OPENNETWORK = NETOPENSUCCESS;
                    return;
                }else if( findstring( msg, "Network is already opened" , len ) ){
                    LTE_STATE.Fields.OPENNETWORK = NETALREADYOPEN;
                    return;
                }else if( findstring( msg, "+IPNETOPEN: 1" , len ) ){       //很少出现这种情况没有SIM卡的时候出现？
                    LTE_STATE.Fields.OPENNETWORK = NETOPENFAILED;
                    return;
                }else{
                    LTE_INFO("\r\n@@@@@@@@@@@@@@@@@@IPNETOPEN OPENNETWORK Never met: %s@@@@@@@@@@@@@@@@@@\r\n", msg );
                }
            }else if( LTE_CURR_CTRL == QUERYNETWORK ){                            //查询命令是否马上返回来？
                if( findstring( msg, "+IPNETOPEN: 1" , len ) ){
                    LTE_STATE.Fields.QUERYNETWORK = NETALREADYOPEN;
                    return;
                }else if( findstring( msg, "+IPNETOPEN: 0" , len ) ){	
                    LTE_STATE.Fields.QUERYNETWORK = NETALREADYCLOSE;
                    return;
                }else{
                    LTE_INFO("\r\n@@@@@@@@@@@@@@@@@@IPNETOPEN QUERYNETWORK Never met: %s@@@@@@@@@@@@@@@@@@\r\n", msg );
                }
            }

        break;
/****************************IPOPEN**************************************/						
        case QUERYTCP:
        case TCPCONN:
            if( findstring( msg,"IPOPEN", len ) ){
                if( LTE_CURR_CTRL == TCPCONN ){
                    if( findstring( msg, "+IPOPEN: 1,0" , len ) ){
                        LTE_STATE.Fields.TCPCONN = TCPCONNSUCCESS;
                    }else if( findstring( msg, "+IPOPEN: 1,1" , len ) ){        //服务器没启动，可以作为错误报告
                        LTE_STATE.Fields.TCPCONN = SERVENOTSTART;				//服务器没启动，可以作为错误报告
                    }else if( findstring( msg, "+IPOPEN: 1,2" , len ) ){
                        LTE_STATE.Fields.TCPCONN = NETNOTSTART;				//网络没打开，得执行IPNETOPEN
                    }else if( findstring( msg, "+IPOPEN: 1,4" , len ) ){        //操作无效，不知道哪里影响了
//                        LTE_ABNORMAL_STATE = true;
                    }else{
                        LTE_INFO("\r\n@@@@@@@@@@@@@@@@@@IPOPEN TCPCONN Never met: %s@@@@@@@@@@@@@@@@@@\r\n", msg );
                    }
                }else if( LTE_CURR_CTRL == QUERYTCP ){
                    if( findstring( msg, "+IPOPEN: 1" , len ) ){
                        if( findstring( msg, "TCP" , len ) ){
                            LTE_STATE.Fields.QUERYTCP = TCPIPERROR;
                        }else{
                            LTE_STATE.Fields.QUERYTCP = TCPALREADYCLOSE;
                            LTE_INFO("\r\n@@@@@@@@@@@@@@@@@@IPOPEN QUERYTCP Never met: %s@@@@@@@@@@@@@@@@@@\r\n", msg );
                        }
                    }
                }else{
                    LTE_INFO("\r\n@@@@@@@@@@@@@@@@@@IPOPEN TOTAL Never met: %s@@@@@@@@@@@@@@@@@@\r\n", msg );
                }
                return;
            }
        break;
/*****************************IPSEND**********************************/
        case TCPSEND:
            if( findstring( msg, "\r\n>", len ) ){
                LteSendData( (char *)LTE_SEND_PARA.HANDLE, LTE_SEND_PARA.LEN );
                if( len == 4 ){
                    return;
                }
            }

            if( findstring( msg,"+IP" , len ) ){
                if( findstring( msg,"+IPSEND: 1" , len ) ){
                    LTE_STATE.Fields.TCPSEND = SENDSUCCESS;
                    return;
                }else if( findstring( msg, "+IPERROR: 2" , len ) ){
                    LTE_STATE.Fields.TCPSEND = SERRORBYNETOFF;				//网络没打开
                    return;
                }else if( findstring( msg, "+IPERROR: 4" , len ) ){         //需要验证结果
                    LTE_ABNORMAL_STATE = true;
                    return;
                }else if( findstring( msg, "+IPERROR: 8" , len ) ){         //lte状态忙busy
                    LTE_ABNORMAL_STATE = true;
                    return;
                }else{
                    LTE_INFO( "IPSEND: %s\r\n", msg );
                }
            }

        break;
/****************************IPCLOSE***********************************/				
        case TCPCLOSE:
            if( findstring( msg,"IPCLOSE", len ) ){
                if( findstring( msg, "+IPCLOSE: 1,0" , len ) ){
                    LTE_STATE.Fields.TCPCLOSE = TCPCLOSESUCCESS;
                }else if( findstring( msg, "+IPCLOSE: 1,1", len  ) ){
                    LTE_STATE.Fields.TCPCLOSE = TCPALREADYCLSOE;
                }else if( findstring( msg, "+IPCLOSE: 1,2", len  ) ){
                    LTE_STATE.Fields.TCPCLOSE = TCPALREADYCLSOE;
                }else	if( findstring( msg, "+IPCLOSE: 1,4" , len ) ){
                    LTE_STATE.Fields.TCPCLOSE = TCPALREADYCLSOE;
                }else{
                    LTE_INFO("\r\n@@@@@@@@@@@@@@@@@@IPCLOSE Never met: %s@@@@@@@@@@@@@@@@@@\r\n", msg );
                }
                return;
            }
        break;

/*****************************QUERYPARA**********************************/							
        default:

        break;

    }
    
    if( len == 0x07 || len == 0x0a || LteRecvData( msg , len) ){//OK ERROR
        return;
    }else if( findstring( msg, "+IPCLOSE: 1,1", len ) || findstring( msg, "+IPCLOSE: 1,2", len ) ||\
              findstring( msg, "+IPCLOSE: 1,0", len ) ){
        xTimerStop(LTE_STATE_CHECK_TIMER_HANDLE, LTE_TIMER_WAIT );
        extern void MQTT_CONNECT_ABNORMAL_FLAG( void );
        MQTT_CONNECT_ABNORMAL_FLAG();
        LTE_INFO("ENTER %s\r\n", msg );
        NET_STATE = NET_DISCONN;
    }else if( findstring( msg, "MODE: 0" , len ) ){
        extern void MQTT_CONNECT_ABNORMAL_FLAG( void );
        MQTT_CONNECT_ABNORMAL_FLAG();
        LTE_INFO("LTE stop service\r\n");
        NET_STATE = NET_DISCONN;
    }else if( findstring( msg, "RING" , len ) ){
        LTE_INFO("Hang up the phone\r\n");
        INDEX TEMP = LTE_CURR_CTRL;
        CmdSend( "AT+CHUP\r\n" );
        LTE_CURR_CTRL = TEMP;
    }else{
        LTE_INFO("\r\n\r\n\r\n###########################enter");
        for( uint16_t i = 0; i < len ; i ++ ){
            LTE_INFO("%c", msg[i] );
        }
        LTE_INFO("out###########################\r\n\r\n\r\n");
    }
}

void LteJudgeState( void ){

    if ( LTE_STATE.Value == 0 ){
        return;
    }
    
    CMD_NO_ACK = 0;
    LTE_ABNORMAL_STATE = false;
    
    switch( LTE_CURR_CTRL ){
        
        case OPENNETWORK:
            if( LTE_STATE.Fields.OPENNETWORK == NETALREADYOPEN || LTE_STATE.Fields.OPENNETWORK == NETOPENSUCCESS ){
                LteChangeState( TCPCONN, CYCLE );
            }else if( LTE_STATE.Fields.OPENNETWORK == NETOPENFAILED ){
                LteChangeState( OPENNETWORK, CYCLE );
                LTE_INFO("The phone card may have no money\r\n");//reset
                osDelay( 1000 );
            }
        break;

        case TCPCONN:
            if( LTE_STATE.Fields.TCPCONN == TCPCONNSUCCESS ){
                xTimerStop( LTE_STATE_CHECK_TIMER_HANDLE, LTE_TIMER_WAIT );
                NET_STATE = NET_CONNECTED;
                CMD_STATE = CMD_OK;
                LteChangeState( CYCLE, CYCLE );
            }else if( LTE_STATE.Fields.TCPCONN == SERVENOTSTART ){
                LteChangeState( TCPCONN, CYCLE );
                osDelay( 1000 );
            }else if( LTE_STATE.Fields.TCPCONN == NETNOTSTART ){
                LteChangeState( OPENNETWORK, CYCLE );
                LTE_INFO("The phone card may have no money\r\n");//reset
                osDelay( 1000 );
            }
        break;

        case TCPSEND:
            if( LTE_STATE.Fields.TCPSEND == SENDSUCCESS ){
                xTimerStop(LTE_STATE_CHECK_TIMER_HANDLE, LTE_TIMER_WAIT );
                LteChangeState( CYCLE, CYCLE );
                CMD_STATE = CMD_OK;
            }else if( LTE_STATE.Fields.TCPSEND == SERRORBYNETOFF ){
                NET_STATE = NET_DISCONN;
            }
        break;

        case TCPCLOSE:
            if( LTE_STATE.Fields.TCPCLOSE == TCPALREADYCLSOE || LTE_STATE.Fields.TCPCLOSE == TCPCLOSESUCCESS ){
                LteChangeState( QUERYNETWORK, CYCLE );
                osDelay(500);
            }
        break;

        case QUERYTCP:
            if( LTE_STATE.Fields.QUERYTCP == TCPALREADYCONN || LTE_STATE.Fields.QUERYTCP == TCPIPERROR ){
                LteChangeState( TCPCLOSE, CYCLE );
            }else if( LTE_STATE.Fields.QUERYTCP == TCPALREADYCLOSE ){
                LteChangeState( TCPCONN, CYCLE );
            }
        break;

        case QUERYNETWORK:
            if( LTE_STATE.Fields.QUERYNETWORK == NETALREADYOPEN ){
                LteChangeState( QUERYTCP, CYCLE );
            }else if( LTE_STATE.Fields.QUERYNETWORK == NETALREADYCLOSE ){
                LteChangeState( OPENNETWORK, CYCLE );
            }
        break;

        default:
            LteChangeState( CYCLE, CYCLE );
        break;
    }

    LTE_STATE.Value = 0;

}

void LteReadySendMsg( void ){
    if( NET_STATE == NET_CONNECTED ){
        if(  MqttSendFlag() ){
            static uint16_t send_timeout = 0;
            if( CMD_STATE == CMD_OK ){
                send_timeout = 0;
                MqttQueueFreeMem();
                if( xQueueReceive( MQTT_SEND_QUEUE_HANDLE, &( MqttSend ), ( TickType_t ) 0 ) ){
                    MqttQueueDec();
                    if( MqttEncryption( &MqttSend, &(LTE_SEND_PARA) )){
                        LteChangeState( TCPSEND, CYCLE );
                    }
                }
            }else{
                send_timeout ++;
                if( send_timeout > 500u ){
                    if( CMD_NO_ACK < EXCEPTION_THRESHOLD ){
                        if( LTE_CURR_CTRL == TCPSEND ){
                            if( LTE_ABNORMAL_STATE == false && LTE_SEND_PARA.HANDLE ){
                                LteSendData( (char *)LTE_SEND_PARA.HANDLE, LTE_SEND_PARA.LEN );
                                LTE_INFO( "recv > overtime\r\n" );
                            }else{
                                CMD_STATE = CMD_OK;
                                MqttReEnterQueue();
                                LTE_INFO( "send msg overtime\r\n" );
                            }
                        }
                    }
                    send_timeout = 0;
                }
            }
        }

        if( CMD_NO_ACK >= EXCEPTION_THRESHOLD ){
            CMD_NO_ACK = 0;
            NetReSetState();
        }
    }else{
        MqttReEnterQueue();
    }
}

void LteCycle( void const * argument ){

    DataType queuetemp;
    
    for(;;){
        
        while( LTE_INITED_STATE == true && QueueNotEmpty( &seqCQueueCache ) ){
            if( QueueDelete( &seqCQueueCache, &queuetemp ) ){
                LteDealState( (char *)queuetemp.index, queuetemp.size );
                LteJudgeState();
            }
        }
        
        switch( LTE_CTRL ){
            
            case CYCLE:
                LteReadySendMsg();
                osDelay( 1 );
            break;
            
            case QUERYNETWORK:
                CmdSend( "AT+IPNETOPEN?\r\n" );
            break;
            
            case OPENNETWORK:
                CmdSend( "AT+IPNETOPEN\r\n" );
            break;
            
            case QUERYTCP:
                CmdSend( "AT+IPOPEN?\r\n" );
            break;
            
            case TCPCONN:
                CmdSend( LTE_CONN_SERVER_CMD );
                osDelay( 500 );
            break;
            
            case TCPCLOSE:
                CmdSend( "AT+IPCLOSE=1\r\n" );
            break;
            
            case TCPSEND:
                LTE_SEND_PARA.CMD[ 0 ] = '\0';
                if( sprintf( LTE_SEND_PARA.CMD, "AT+IPSEND=1,%d\r\n", LTE_SEND_PARA.LEN ) > 0 ){
                    CmdSend( LTE_SEND_PARA.CMD );
                }
            break;

            default:
                LTE_INFO("raise an exception\r\n");
            break;
        }
    }
}


