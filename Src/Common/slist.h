#ifndef __LIST__H_
#define __LIST__H_
#include "mqtt_app.h"

typedef struct Node{
    MqttSend_t data;
    uint16_t    times;
    uint8_t    delay;
    struct Node *next;
}SLNode;

void ListInitiate( SLNode **head );
int ListInsert( SLNode *head, MqttSend_t x );
void ListCheck(  SLNode *head );
void ListConfirm( SLNode *head, MQTT_TYPE type, short sign  );
void ListDelete( SLNode *head, int16_t count );
#endif
