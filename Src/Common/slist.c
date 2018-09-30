#include "slist.h"

void ListInitiate( SLNode **head ){
    *head = (SLNode *) pvPortMalloc( sizeof( SLNode ) );
    (*head)->next = NULL;
}

int ListInsert( SLNode *head, MqttSend_t x ){
    SLNode *p = NULL, *q = NULL;
    p = head;
    q = (SLNode *) pvPortMalloc( sizeof( SLNode ) );
    if( q ){
        taskENTER_CRITICAL();
        q->data = x;
        q->delay = 0;
        q->times = 0;
        q->next = p->next;
        p->next = q;
        taskEXIT_CRITICAL();
        return 1;
    }else{
        return 0;
    }
}

void ListCheck(  SLNode *head ){
    
    SLNode *p = NULL, *q = NULL;
    p = head;
    
    taskENTER_CRITICAL();
    while( p->next != NULL ){
        q = p;
        p = p->next;
        p->delay ++;
        if( p->delay >= 5 ){
            p->delay = 0;
            p->times ++;
            if( p->times < 10 ){
                if( xQueueSend( MQTT_SEND_QUEUE_HANDLE, ( void * ) &( p->data ), ( TickType_t ) 0 ) == pdTRUE ){
                    MqttQueueInc();
                }else{
                    p->delay = 4;
                    p->times --;
                    break;
                }
            }
        }
        
        if( p->times >= 10 ){
            vPortFree( p->data.payload );
            vPortFree( p->data.topic );
            q->next = p->next;
            vPortFree( p );
            p = q;
        }
    }
    taskEXIT_CRITICAL();
}

void ListConfirm( SLNode *head, MQTT_TYPE confirm, short identification  ){
    
    SLNode *p = NULL, *q = NULL;
    p = head;
    
    taskENTER_CRITICAL();
    while( p->next != NULL ){
        q = p;
        p = p->next;
        if( p->data.identification == identification && p->data.confirm == confirm ){
            vPortFree( p->data.payload );
            vPortFree( p->data.topic );
            q->next = p->next;
            vPortFree( p );
            break;
        }
    }
    taskEXIT_CRITICAL();
}

void ListDelete( SLNode *head, int16_t number ){
    
    SLNode *p = NULL, *q = NULL;
    p = head;
    taskENTER_CRITICAL();
    while( p->next != NULL && number-- ){
        q = p;
        p = p->next;
        vPortFree( p->data.payload );
        vPortFree( p->data.topic );
        q->next = p->next;
        vPortFree( p );
        p = q;
    }
    taskEXIT_CRITICAL();
}
