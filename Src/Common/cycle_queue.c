#include "cmsis_os.h"
#include "cycle_queue.h"
#include "common.h"
SeqCQueue   seqCQueue;
SeqCQueue   seqCQueueCache;
/*-------------------public function----------------------------------*/
void QueueInitiate( SeqCQueue* Q ){
	Q->rear = 0;
	Q->front = 0;
	Q->count = 0;
    Q->leftram = 0;
    Q->reConnectState = false;
    
	uint8_t *cache = &UsartType.usartDMA_rxBuf[0];
    
    Q->heapcache = cache;
    Q->currentCache = cache;
	
    for( uint8_t i = 0; i < MaxQueueSize; i++ ){
        Q->queue[i].index = NULL;
    }
}

int QueueNotEmpty( SeqCQueue *Q ){
    
	if( Q->count != 0 ){
		return 1;
	}else{
        if( Q->rear != Q->front ){
            taskENTER_CRITICAL();
            if( Q->count == 0 ){
                Q->rear = Q->front;
            }
            taskEXIT_CRITICAL();
        }
		return 0;
	}
}

int QueueAppend( SeqCQueue* Q, DataType x ){

    if( Q->count > 0 && Q->rear == Q->front ){
        return 0;
    }else{
        if( Q->count == 0 ){
            Q->rear = Q->front;
        }
        Q->queue[ Q->rear ] = x;
        Q->rear =  ( Q->rear + 1 ) % MaxQueueSize;
        Q->count++;
        return 1;
    }
}

int QueueDelete( SeqCQueue *Q, DataType *d ){
	if( Q->count == 0 ){
		return 0;
	}else{
        taskENTER_CRITICAL();//进入临界区
		*d = Q->queue[Q->front];
		Q->front = ( Q->front + 1 ) % MaxQueueSize;
		Q->count --;
        taskEXIT_CRITICAL();//退出临界区
		return 1;
	}
}

bool isAck( bool isdata ){
    extern SeqCQueue seqCQueue;
    int16_t waitCount = 0;
    while( QueueNotEmpty( isdata ? &seqCQueue : &seqCQueueCache ) == 0 && waitCount < ( isdata ? 1000:2000 ) ){
        waitCount ++;
        osDelay(1);
    }
    return ( QueueNotEmpty( isdata ? &seqCQueue : &seqCQueueCache ) == 1 ) ;
}




