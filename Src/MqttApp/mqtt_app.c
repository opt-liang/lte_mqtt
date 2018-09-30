#include "MQTTPacket.h"
#include "transport.h"
#include "cycle_queue.h"
#include "mqtt_app.h"
#include "common.h"
#include "slist.h"

#define PING_TIME_INTERVAL      50
#define PING_TIME_MULTIPLE      (1000/PING_TIME_INTERVAL)

/**********************************************************************************************************/
#define MQTT_PING_TIMEOUT(T) (10+(T))

SLNode *MQTT_QUEUE_LIST;
static int MQTT_SOCKET_HANDLE = 0;

unsigned char MQTT_BUF[ LEFTRAMSIZE ];
short MQTT_BUF_LEN = LEFTRAMSIZE;

uint8_t MQTT_ID[ MAC_LEN ] = "ffffffffff";

static uint16_t PING_TIMEOUT_COUNT = 0;
static int8_t MQTT_SEND_QUEUE_COUNT = 0;
bool MQTT_SUB_TOPIC_STATE = false;
bool MQTT_SEND_FLAG = false;

MQTT_CONNECT_STAT   NET_STATE = NET_DISCONN;
osThreadId      MQTT_TASK_HANDLE = NULL;
QueueHandle_t   MQTT_SEND_QUEUE_HANDLE = NULL;
TimerHandle_t   MQTT_STAT_TIMER_HANDLE = NULL;

SEND_PARA   LTE_SEND_PARA;
MqttSend_t  MqttSend;
/**********************************************************************************************************/

void MqttTestSend( void ){
    uint8_t temp_string[ 16 ];
    static uint32_t count = 0;
    memset( temp_string, 0, 16 );
    count ++;
    if( sprintf( ( char *)temp_string, "%d", count ) > 0 ){
        MqttPublish( "UP", ( uint8_t *)temp_string, strlen( (char *)temp_string ), MQTT_SEND_CONFIRM );
    }
}

void MqttThread( void ){
    
    extern void MqttCycle( void const * argument );
    extern void MqttCheckPingState( TimerHandle_t xTimer );
    
    MQTT_STAT_TIMER_HANDLE = xTimerCreate((const char* )"mqttstat", (TickType_t )PING_TIME_INTERVAL,(UBaseType_t )pdFALSE,(void* )2,(TimerCallbackFunction_t)MqttCheckPingState);
    if( MQTT_STAT_TIMER_HANDLE == NULL ){
      MQTT_INFO("MQTT_STAT_TIMER_HANDLE is NULL\r\n");
      while( true );
    }
    
    osThreadDef( mqtt, MqttCycle, osPriorityNormal, 1, 512);
    MQTT_TASK_HANDLE = osThreadCreate(osThread(mqtt), NULL);
    if( MQTT_TASK_HANDLE == NULL ){
      MQTT_INFO("create lte thread failed\r\n");
      while( true );
    }
    
    MQTT_SEND_QUEUE_HANDLE = xQueueCreate( MQTT_SEND_QUEUE_MAX_COUNT, sizeof( MqttSend_t ) );
    if( MQTT_SEND_QUEUE_HANDLE == NULL ){
      MQTT_INFO("create MQTT_SEND_QUEUE_HANDLE QUEUE failed\r\n");
      while( true );
    }
}

void MqttQueueDec( void ){
    --MQTT_SEND_QUEUE_COUNT;
}

void MqttQueueInc( void ){
    ++MQTT_SEND_QUEUE_COUNT;
}

bool MqttSendFlag( void ){
    return ( MQTT_SEND_FLAG && ( MQTT_SEND_QUEUE_COUNT > 0 ) );
}

void NetReSetState( void ){
    NET_STATE = NET_DISCONN;
}

void NetSetState( void ){
    NET_STATE = NET_CONNECTED;
}

bool NetConnState( void ){
    if( NET_STATE == NET_CONNECTED ){
        return true;
    }else{
        return false;
    }
}

void MqttReInit( void ){
    
    xTimerStop( MQTT_STAT_TIMER_HANDLE, 0 );

    NetReSetState();
    
	MQTT_SUB_TOPIC_STATE = false;
    
	PING_TIMEOUT_COUNT = 0;
    
    MQTT_SEND_FLAG = false;
}

bool MqttNetOffState( void ){

    bool stat = false;
    if( !NetConnState() || PING_TIMEOUT_COUNT >= ( MQTT_PING_TIMEOUT(5) * PING_TIME_MULTIPLE) ){
        NetReSetState();
        stat = true;
    }
    if( stat ){
        xTimerStop( MQTT_STAT_TIMER_HANDLE, 0 );
    }
    return stat;
}

void MqttSubTopicState( void ){
	if( MQTT_SUB_TOPIC_STATE == false ){
        if( ( PING_TIMEOUT_COUNT % ( 5 * PING_TIME_MULTIPLE ) ) == 0x00 ){
            MqttSubTopic( (char*)MQTT_ID );
        }
	}else{
        if( PING_TIMEOUT_COUNT < MQTT_PING_TIMEOUT(5) * PING_TIME_MULTIPLE &&\
            MQTT_SEND_QUEUE_COUNT < MQTT_SEND_QUEUE_MAX_COUNT ){
            ListCheck( MQTT_QUEUE_LIST );
        }
    }
}

void MqttCheckPingState( TimerHandle_t xTimer ){
    
    if( NetConnState() ){
        
        xTimerStart( MQTT_STAT_TIMER_HANDLE, 100 );
        
        PING_TIMEOUT_COUNT++;
        
        if( PING_TIMEOUT_COUNT < MQTT_PING_TIMEOUT(5) * PING_TIME_MULTIPLE && MQTT_SEND_QUEUE_COUNT < MQTT_SEND_QUEUE_MAX_COUNT ){
            MqttTestSend();
        }
        
        if( ( PING_TIMEOUT_COUNT % ( PING_TIME_MULTIPLE )  == 0x00 ) ){
            MqttSubTopicState();
            if( PING_TIMEOUT_COUNT >= MQTT_PING_TIMEOUT(0) * PING_TIME_MULTIPLE ){
                if( PING_TIMEOUT_COUNT == MQTT_PING_TIMEOUT(0) * PING_TIME_MULTIPLE || PING_TIMEOUT_COUNT == MQTT_PING_TIMEOUT(5) * PING_TIME_MULTIPLE ){
                    if( PING_TIMEOUT_COUNT >= MQTT_PING_TIMEOUT(5) * PING_TIME_MULTIPLE ){
                        NetReSetState();
                        MQTT_INFO("MQTT PING TIMEOUT retransmission %d\r\n", PING_TIMEOUT_COUNT);
                        return;
                    }
                    MqttPingReq();
                }
            }
        }
    }
}

void MqttSetUniqueId( uint8_t *mqtt_id ){
    uint8_t ID[ 8 ] = { 0 };
    ID[7] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) ) >> 24;
    ID[6] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) ) >> 16;
    ID[5] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) ) >> 8;
    ID[4] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) );
    ID[3] = ( ( *( uint32_t* )ID2 ) ) >> 24;
    ID[2] = ( ( *( uint32_t* )ID2 ) ) >> 16;
    ID[1] = ( ( *( uint32_t* )ID2 ) ) >> 8;
    ID[0] = ( ( *( uint32_t* )ID2 ) );
    LIB_nByteHexTo2Asc( (char *)mqtt_id, ID, 6 );
    mqtt_id[ 12 ] = '\0';
}

/**********************************************************************************************/

void MqttQueueDelete( uint8_t delete_number ){
    taskENTER_CRITICAL();
    MqttSend_t MqttSend;
    while( MQTT_SEND_QUEUE_COUNT > delete_number ){
        if( xQueueReceive( MQTT_SEND_QUEUE_HANDLE, &( MqttSend ), ( TickType_t ) 0 ) ){
            --MQTT_SEND_QUEUE_COUNT;
            if( (MqttSend.confirm ? 0:1) ){
                vPortFree( MqttSend.topic );
            }
            if( (MqttSend.confirm ? 0:1) ){
                vPortFree( MqttSend.payload );
            }
        }else{
            break;
        }
    }
    taskEXIT_CRITICAL();
}


void MqttQueueSend( char *topic, unsigned char * payload, uint16_t payloadlen, MQTT_TYPE prio_confirm ){

    if( xPortGetFreeHeapSize() <= ( configTOTAL_HEAP_SIZE / 10 ) ){
        ListDelete( MQTT_QUEUE_LIST, MQTT_SEND_QUEUE_MAX_COUNT * 10 );
        MqttQueueDelete( 0 );
    }
    
	if( MQTT_SEND_QUEUE_COUNT < MQTT_SEND_QUEUE_MAX_COUNT ){
        MqttSend_t MqttSend;
        
        char *ptopic = NULL;
        MqttSend.confirm = ( MQTT_TYPE )(prio_confirm & 0x0f);
        
        if( topic ){
            ptopic = pvPortMalloc( strlen( topic ) + 1 );
        }
        char *ppayload = pvPortMalloc( payloadlen );
        
        if( ( MqttSend.confirm ? ptopic != NULL : true ) && ppayload ){
            if( ptopic ){
                memcpy( (char *)ptopic, topic, strlen( topic ) );
                ptopic[ strlen( topic ) ] = '\0';
            }
            memcpy( ppayload, payload, payloadlen );
            
            MqttSend.identification = 0;
            MqttSend.payloadlen = payloadlen;
            MqttSend.payload    = ppayload;
            MqttSend.topic      = ptopic;
            
            if( MqttSend.confirm ){
                if( MqttSend.confirm == MQTT_SEND_CONFIRM ){
                    static short frame = 0;
                    MqttSend.identification = frame++ ? frame : (frame = 1);
                }else if( MqttSend.confirm == MQTT_SEND_CMD_CONFIRM ){
                    MqttSend.identification  = *(( uint16_t *)(payload+4));//cmd id
                }
                ListInsert( MQTT_QUEUE_LIST, MqttSend );
            }

            if( prio_confirm  == MQTT_SEND_UNCONFIRM_HIGHPRIORITY || \
                prio_confirm  == MQTT_SEND_CONFIRM_HIGHPRIORITY ){
                if(  xQueueSendToFront( MQTT_SEND_QUEUE_HANDLE, ( void * ) &MqttSend, ( TickType_t ) 0 ) == pdTRUE ){
                    MQTT_SEND_QUEUE_COUNT ++;
                }else{
                    if( (MqttSend.confirm ? 0:1) ){
                        vPortFree( MqttSend.topic );
                    }
                    if( (MqttSend.confirm ? 0:1) ){
                        vPortFree( MqttSend.payload );
                    }
                }
            }else{
                if( MQTT_SUB_TOPIC_STATE ){
                    if( xQueueSend( MQTT_SEND_QUEUE_HANDLE, ( void * ) &MqttSend, ( TickType_t ) 0 ) == pdTRUE ){
                        MQTT_SEND_QUEUE_COUNT ++;
                    }else{
                        if( (MqttSend.confirm ? 0:1) ){
                            vPortFree( MqttSend.topic );
                        }
                        if( (MqttSend.confirm ? 0:1) ){
                            vPortFree( MqttSend.payload );
                        }
                    }
                }else{
                    if( (MqttSend.confirm ? 0:1) ){
                        vPortFree( MqttSend.topic );
                    }
                    if( (MqttSend.confirm ? 0:1) ){
                        vPortFree( MqttSend.payload );
                    }
                }
            }
            return;
        }else{
            if( ptopic ){
                vPortFree( ptopic );
            }
            if( ppayload ){
                vPortFree( ppayload );
            }
            ListDelete( MQTT_QUEUE_LIST, MQTT_SEND_QUEUE_MAX_COUNT * 10 );
            MqttQueueDelete( 0 );
            return;
        }
	}else{
        MQTT_INFO("queue is full\r\n");
        MqttQueueDelete( MQTT_SEND_QUEUE_MAX_COUNT - 10 );
        MQTT_INFO("mqtt send malloc ram error, send failed, queue count %d\r\n", MQTT_SEND_QUEUE_COUNT );
        return;
    }
}

bool MqttEncryption( MqttSend_t *Ori, SEND_PARA* DestPayload ){

    if( DestPayload->HANDLE ){
        vPortFree( DestPayload->HANDLE );
        DestPayload->HANDLE = NULL;
    }
    short realLen = 0;
    if( Ori->topic ){
        realLen = strlen( Ori->topic ) + 5 + (Ori->confirm == 1 ? 2:0) + Ori->payloadlen;
        DestPayload->HANDLE = pvPortMalloc( realLen );
    }else{
        DestPayload->HANDLE = pvPortMalloc( Ori->payloadlen );
    }
    
    if( DestPayload->HANDLE ){
        if( Ori->topic ){
            //encryption payload
            MQTTString MQTT_TOPIC_STRING = MQTTString_initializer;
            MQTT_TOPIC_STRING.cstring = Ori->topic;
            DestPayload->LEN = MQTTSerialize_publish( (unsigned char *)(DestPayload->HANDLE), realLen , 0, (Ori->confirm) & 0x01, 0,\
            Ori->identification, MQTT_TOPIC_STRING, (unsigned char*)Ori->payload, Ori->payloadlen );
        }else{
            DestPayload->LEN = Ori->payloadlen;
            memcpy( DestPayload->HANDLE, Ori->payload, Ori->payloadlen );
        }
        return true;
    }else{
        return false;
    }
}

/**********************************************************************************************/

void MqttPublish( char *topic, unsigned char * payload, uint16_t payloadlen, MQTT_TYPE type ){
    MqttQueueSend( topic, payload, payloadlen, type );
}

void MqttSubTopic( char *topic ){

    int32_t msgid = SUB_INDEXES_TOPIC;
    if( strcmp( topic, (char *) MQTT_ID) == 0 ){
        msgid = SUB_MQTT_ID;
    }

    int32_t req_qos = 0;
    uint16_t lensub    = 0;
    uint8_t bufsub[32] = {0};
    MQTTString MQTT_TOPIC_STRING = MQTTString_initializer;
    MQTT_TOPIC_STRING.cstring = topic;
    lensub = MQTTSerialize_subscribe(bufsub, sizeof bufsub, 0, msgid, 1, &MQTT_TOPIC_STRING, &req_qos );
    MqttQueueSend( NULL, bufsub, lensub, MQTT_SEND_UNCONFIRM_HIGHPRIORITY );
}

void MqttCancelSubTopic( char *topic ){
    uint8_t bufsub[32] = {0};
	int32_t msgid = SUB_INDEXES_TOPIC;
    MQTTString MQTT_TOPIC_STRING = MQTTString_initializer;
    MQTT_TOPIC_STRING.cstring = topic;
    uint16_t lensub = MQTTSerialize_unsubscribe( bufsub, sizeof bufsub, 0, msgid, 1, &MQTT_TOPIC_STRING );
    MqttQueueSend( NULL, bufsub, lensub, MQTT_SEND_UNCONFIRM_HIGHPRIORITY );
}

void MqttPingReq( void ){
	uint8_t bufsub[2] = {0xc0, 0x00};//2byte change from 8byte
//    uint8_t lensub = MQTTSerialize_pingreq( bufsub, sizeof bufsub );
    MqttQueueSend( NULL, bufsub, 2, MQTT_SEND_UNCONFIRM_HIGHPRIORITY );
}

void MqttQueueFreeMem( void ){
    if( MqttSend.topic ){
		if( MqttSend.confirm ? 0:1 ){
			vPortFree( MqttSend.topic );
		}
        MqttSend.topic = NULL;
    }
    if( MqttSend.payload ){
		if( MqttSend.confirm ? 0:1 ){
			vPortFree( MqttSend.payload );
		}
        MqttSend.payload = NULL;
    }
}

void MqttReEnterQueue( void ){
    
    if( MqttSend.topic && MqttSend.payload ){
        if( xQueueSend( MQTT_SEND_QUEUE_HANDLE, ( void * ) &MqttSend, ( TickType_t ) 0 ) == pdTRUE ){
            MqttQueueInc();
            MqttSend.topic = NULL;
            MqttSend.payload = NULL;
            return;
        }
    }
    MqttQueueFreeMem();
}
/**********************************************************************************************/

void MqttCycle( void const * argument )
{
	enum msgTypes mqttmsgType ;
	char *host = "39.108.231.83";
	int port = 1885;
	
    ListInitiate( &MQTT_QUEUE_LIST );
	MqttSetUniqueId( MQTT_ID );

MQTTREINIT:
    
    MqttReInit();
    
	MQTT_SOCKET_HANDLE = transport_open(host, port);
    
	if ( MQTT_SOCKET_HANDLE < 0) {
		osDelay(500);
		goto MQTTREINIT;
	}else{
        
        if( MQTT_SEND_QUEUE_COUNT == MQTT_SEND_QUEUE_MAX_COUNT ){
            MqttQueueDelete( MQTT_SEND_QUEUE_MAX_COUNT - 2 );
        }
        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.clientID.cstring = (char *)MQTT_ID;
        data.keepAliveInterval = 60;
        data.cleansession = 1;
        data.username.cstring = "gateway";
        data.password.cstring = "passwd";
        int len = MQTTSerialize_connect(MQTT_BUF, MQTT_BUF_LEN, &data);
        MqttQueueSend( NULL, MQTT_BUF, len, MQTT_SEND_UNCONFIRM_HIGHPRIORITY );
        MQTT_SEND_FLAG = true;
    }

    MQTT_INFO("Sending to hostname %s port %d\n", host, port);
    
 	/* wait for connack */
    if( isAck( true ) == false ){
        MQTT_INFO("Queue is Empty\r\n");
        goto MQTTREINIT;
    }
    
	while ( true ) {

		mqttmsgType = (enum msgTypes) MQTTPacket_read( MQTT_BUF, MQTT_BUF_LEN, transport_getdata );

		switch ( mqttmsgType ) {

			case CONNACK:
				{
					unsigned char sessionPresent, connack_rc;
					int rc = MQTTDeserialize_connack(&sessionPresent, &connack_rc, MQTT_BUF, MQTT_BUF_LEN);
					if ( rc == 1) {
                        
						MQTT_INFO("Device: %s Successful connect mqtt broker , just do it\r\n", MQTT_ID );
						
						MqttSubTopic((char *) MQTT_ID );
                        
                        xTimerStart( MQTT_STAT_TIMER_HANDLE, 0 );
                        
					}else if (rc != 1){
						goto EXIT;
					}
				}
				break;

			case PUBLISH:
				{
					unsigned char dup;
					int qos;
					unsigned char retained;
					unsigned short msgid;
					int payloadlen_in;
					unsigned char *payload_in;
					MQTTString receivedTopic;

					int rc = MQTTDeserialize_publish( &dup, &qos, &retained, &msgid, &receivedTopic, &payload_in, &payloadlen_in, MQTT_BUF, MQTT_BUF_LEN);
					if (rc == 1) {
                        printf( "%s\r\n", payload_in );
//						commandProcessing( payload_in, payloadlen_in );
					}else{
                        printf( "\r\n@@@@@@@@@@@@@@@@@@@@ PUBLISH @@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n" );
                    }
				}
				break;

			case PUBACK:
				{
					unsigned char packettype = 0;
					unsigned char dup_ack = 0;
					unsigned short packetid = 0;
					if( MQTTDeserialize_ack( &packettype, &dup_ack, &packetid, MQTT_BUF, MQTT_BUF_LEN) ){
                        ListConfirm( MQTT_QUEUE_LIST, MQTT_SEND_CONFIRM, packetid );
                        printf( "\r\n@@@@@@@@@@@@@@@@@@@@ PUBACK PACKET ID %d @@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n", packetid );
                    }
				}

				break;

			case PINGRESP:
				{
					PING_TIMEOUT_COUNT = 0;
					MQTT_INFO("###################### Pong success ######################\r\n");
				}
				break;

			case SUBACK:
				{
					int subcount = 0;
					int granted_qos = 0;
                    unsigned short submsgid = 0;
                    
					int rc = MQTTDeserialize_suback(&submsgid, 1, &subcount,&granted_qos, MQTT_BUF, MQTT_BUF_LEN);
					if (rc == 1){
						if ( granted_qos != 0 && granted_qos != 1 && granted_qos != 2 && (submsgid == SUB_INDEXES_TOPIC || submsgid == SUB_MQTT_ID )) {
							MQTT_INFO("granted qos != 0,1,2 %d--submsgid=%d\r\n", granted_qos, submsgid);
							MQTT_INFO("CYCLE SUBSTOPIC ERROR,Delay 5 seconds,reinit mqtt\r\n");
							goto EXIT;
						} else {
							if ( submsgid == SUB_MQTT_ID ){
								MQTT_SUB_TOPIC_STATE = true;
								MQTT_INFO("######################success sub the topic################\r\n");
							}else{
                                printf( "\r\n@@@@@@@@@@@@@@@@@@@@ SUBACK @@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n" );
                            }
                            
						}
					}
				}
				break;

			case UNSUBACK:
				{
					unsigned short submsgid = 0;

					int rc = MQTTDeserialize_unsuback( &submsgid, MQTT_BUF, MQTT_BUF_LEN );
					if (rc == 1) {
						if ( submsgid == SUB_INDEXES_TOPIC) {
							MQTT_INFO("######################UNSUBACK success######################\r\n");
						}
					}
				}
				break;

			default:

				break;
		}
        
        if( !QueueNotEmpty( &seqCQueue ) ){
            if( MqttNetOffState() ){
                goto EXIT;
            }
        }
	}

EXIT:
	MQTT_INFO("disconnecting\n");
	transport_close( MQTT_SOCKET_HANDLE );
	goto MQTTREINIT;

}





