#ifndef __LTE_DRIVER__
#define __LTE_DRIVER__
#include <stdbool.h>
#include <stdint.h>
void LteReboot( void );
void LteRepowerOn( void );
void CmdSend(  char  *pdata );
void set_lte_baud( uint32_t baud );
bool judge_lte_rssi( void );
bool judge_sim_card( void );

#endif









