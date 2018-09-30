#ifndef __MQTT_APP__
#define __MQTT_APP__
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "lte.h"

#define MQTT_DEBUG true
#if MQTT_DEBUG
    #define MQTT_INFO(fmt,args...)  printf(fmt, ##args)
#else
    #define MQTT_INFO(fmt,args...)
#endif

#define MQTT_SEND_QUEUE_MAX_COUNT   20

#define         ID1                                   (0x1FFFF7E8)
#define         ID2                                   (0x1FFFF7E8 + 0x04)
#define         ID3                                   (0x1FFFF7E8 + 0x08)

#define MAC_LEN		            16
#define SUB_MQTT_ID             16384
#define SUB_INDEXES_TOPIC       1024

typedef enum{
    MQTT_SEND_UNCONFIRM                 = 0x00,
    MQTT_SEND_CONFIRM                   = 0x01,
    MQTT_SEND_CMD_CONFIRM               = 0x02,
    MQTT_SEND_UNCONFIRM_HIGHPRIORITY    = 0x80,
    MQTT_SEND_CONFIRM_HIGHPRIORITY      = 0x81,
}MQTT_TYPE;

typedef struct xMqttSend_t{
    MQTT_TYPE  confirm;
    short identification;
    short payloadlen;
    char *topic;
    char *payload;
}MqttSend_t;

bool MqttSendFlag( void );
void MqttQueueInc( void );
void MqttQueueDec( void );
void MqttPingReq( void );
void MqttSubTopic( char *topic );
void MqttPublish( char *topic, unsigned char * payload, uint16_t payloadlen, MQTT_TYPE type );
bool MqttEncryption( MqttSend_t *Ori, SEND_PARA* DestPayload );
extern QueueHandle_t MQTT_SEND_QUEUE_HANDLE;
#endif

