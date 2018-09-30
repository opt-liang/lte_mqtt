#include "stm32f1xx_hal.h"
#include "uart_driver.h"
#include "cmsis_os.h"
#include <string.h>
#include "cycle_queue.h"
#include "common.h"
#include <stdlib.h>
#include "lte_driver.h"


#define LTED_DEBUG true
#if LTED_DEBUG
    #define LTED_INFO(fmt,args...)  printf(fmt, ##args)
#else
    #define LTED_INFO(fmt,args...)
#endif

#define CYCLE_CHECK_TIMES       10

void LteReboot( void ){
    LTED_INFO( "reboot lte module\r\n" );
    HAL_GPIO_WritePin(LTE_RESET_GPIO_Port, LTE_RESET_Pin, GPIO_PIN_SET);
    osDelay(500);
    HAL_GPIO_WritePin(LTE_RESET_GPIO_Port, LTE_RESET_Pin, GPIO_PIN_RESET);
    osDelay( 20000 );
}

void LteRepowerOn( void ){
    HAL_GPIO_WritePin(LTE_PWR_GPIO_Port, LTE_PWR_Pin, GPIO_PIN_RESET);
    osDelay(10);
    HAL_GPIO_WritePin(LTE_PWR_GPIO_Port, LTE_PWR_Pin, GPIO_PIN_SET);
    osDelay( 20000 );
}

void CmdSend(  char  *pdata ){
        LTED_INFO("CMD IS :%s",pdata );
        extern void LteChangeStateExapi( void );
        LteChangeStateExapi();
		UartSendData( &huart3, (uint8_t *)pdata, strlen( pdata ) );
}

bool confirm_baud( char *cmd, uint32_t baud ){
    
    if( cmd[0] = '\0', sprintf( cmd, "AT+IPREX=%d\r\n", baud ) > 0 ){
        osDelay( 15 );
        MX_UART_Config( &huart3, baud );
        CmdSend( cmd );
        int8_t cycle_read = CYCLE_CHECK_TIMES;
        DataType queuetemp;
        while( isAck( false ) && --cycle_read ){
            if( QueueDelete( &seqCQueueCache, &queuetemp ) ){
                if( findstring( (const char*)queuetemp.index, "OK", queuetemp.size ) ){
                    LTED_INFO( "baud is right\r\n" );
                    CmdSend( "ATE0\r\n" );
                    osDelay( 100 );
                    return true;
                }
            }
        }
        LTED_INFO( "##############  LTE Non response  ####################\r\n" );
    }else{
        LTED_INFO( "confirm_baud sprintf error\r\n" );
    }
    return false;
}

void set_lte_baud( uint32_t baud ){
    
    if( baud != 115200 && baud != 921600 ){
        LTED_INFO( "baud: %d error, Please recheck, baud = 921600 default \r\n", baud );
        baud = 921600;
    }

    char SET_BAUD_CMD[ 64 ] ;
    SET_BAUD_CMD[0] = '\0';
    while( sprintf( SET_BAUD_CMD, "AT+IPREX=%d\r\n", baud ) == -1 ){
        LTED_INFO( "set_lte_baud sprintf error\r\n" );
        osDelay( 1000 );
        SET_BAUD_CMD[ 0 ] = '\0';
    }
    
    uint8_t cycle_count = 0;
    
    SET_LTE_BAUD:
    osDelay( 100 );
    if( cycle_count == 0 || cycle_count >= 10 ){

        if( cycle_count >= 10 ){
            cycle_count = 0;
            LteReboot();
        }
        
        MX_UART_Config( &huart3, 15200 );//lte default
    }

    CmdSend( SET_BAUD_CMD );
    
    if( !confirm_baud( SET_BAUD_CMD, baud ) ){
        cycle_count ++;
        if( cycle_count % 2 != 0 ){
            MX_UART_Config( &huart3, 921600 );
        }else{
            MX_UART_Config( &huart3, 115200 );//lte default
        }
        
        goto SET_LTE_BAUD;
    }
}


bool judge_sim_card( void ){
    
    bool stat = false;
    uint8_t cycle_count = 0;
    DataType queuetemp;
    
    JUDGE_SIM_CARD:
    osDelay( 100 );
    cycle_count ++;
    if( cycle_count > 5 ){
        return false;
    }
    
    CmdSend( "AT+CPIN?\r\n" );
    
    int8_t cycle_read = CYCLE_CHECK_TIMES;
    
    while( isAck( false ) && --cycle_read ){
        if( QueueDelete( &seqCQueueCache, &queuetemp ) ){
            if( findstring( (const char*)queuetemp.index, "READY", queuetemp.size ) ){
                stat = true;
                LTED_INFO("SIM already inserted\r\n");
                break;
            }else if(\
                findstring( (const char*)queuetemp.index, "SIM not inserted", queuetemp.size ) ||\
                findstring( (const char*)queuetemp.index, "SIM failure", queuetemp.size ) ){
                LTED_INFO("SIM failure\r\n");
                return false;
            }
        }
    }
    
    if( !stat ){
        LTED_INFO( "judge_sim_card error\r\n" );
        goto JUDGE_SIM_CARD;
    }
    
    return true;
    
}

bool judge_lte_rssi( void ){
    
    bool stat = false;
    DataType queuetemp;
    uint8_t cycle_count = 0;
    int16_t rssi[1] = { 0 };

    JUDGE_LTE_RSSI:
    osDelay( 100 );
    cycle_count ++;
    
    if( cycle_count > 5 ){
        LTED_INFO( "judge_lte_rssi error\r\n" );
        return false;
    }
    
    CmdSend( "AT+CCSQ\r\n" );
    
    int8_t cycle_read = CYCLE_CHECK_TIMES;
    
    while( isAck( false ) && --cycle_read ){
        
        if( QueueDelete( &seqCQueueCache, &queuetemp ) ){
            
            if( findstring( (const char*)queuetemp.index, "+CCSQ:", queuetemp.size ) ){
                
                char * handle = findstring( (char *)queuetemp.index, " ", queuetemp.size );
                
                if( !handle ){
                    goto JUDGE_LTE_RSSI;
                }
                
                if( sscanf( handle, "%hu,", &rssi[0] ) == 1 ){
                    if( rssi[0] > 31 ){
                        LTED_INFO("A problem with the antenna\r\n" );
                    }else if( rssi[0] >= 0 && rssi[0] <= 31 ){
                        int16_t signal = -113 + 2 * rssi[0];
                        LTED_INFO("LTE RSSI is %d\r\n", signal );
                        if( rssi[0] >= 12 ){
                            stat  = true;
                            if( rssi[0] >= 20 ){
                                LTED_INFO("Strong signal\r\n" );
                            }else{
                                LTED_INFO("Weak signal\r\n" );
                            }
                            break;
                        }else{
                            LTED_INFO("The signal is very weak\r\n" );
                        }
                    }
                }
            }
        }
    }
    
    if( !stat ){
        goto JUDGE_LTE_RSSI;
    }
    
    return true;
}

