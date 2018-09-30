#include <stdint.h>
#include <string.h>
#include "cmsis_os.h"
#include <stdio.h>
#include "common.h"


struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;

int fputc(int ch, FILE *f) {
  if (DEMCR & TRCENA) {
    while (ITM_Port32(0) == 0);
    ITM_Port8(0) = ch;
  }
  return(ch);
}

char* findstring(const char* full_data, const char* substr, int full_data_len){  
    if (full_data == NULL || full_data_len <= 0 || substr == NULL) {  
        return NULL;
    }
  
    if (*substr == '\0') {
        return NULL;
    }  
  
    int sublen = strlen(substr);
  
    int i;
    char* cur = (char *)full_data;  
    int last_possible = full_data_len - sublen + 1;
    for (i = 0; i < last_possible && last_possible <= 2048u ; i++) {
        if (*cur == *substr) {
            if (memcmp(cur, substr, sublen) == 0) {
                return cur;
            }
        }
        cur++;
    }
    
    return NULL;
}

uint16_t LIB_HexTo2Asc(uint8_t hex)
{
	uint16_t dat;
	if(((hex&0xF0)>>4)<=9)  dat=((((hex&0xF0)>>4)+0x30)<<8);
		else dat=((((hex&0xF0)>>4)+0x41-10)<<8);
	if((hex&0x0F)<=9)	dat+=((hex&0x0F)+0x30);
		else dat+=((hex&0x0F)+0x41-10);	
	return dat;
}

uint16_t LIB_BigLittleEndian_16BitConvert(uint16_t tdata)           
{
  return ((tdata>>8) + (tdata<<8));
}

void LIB_nByteHexTo2Asc(char *asc, uint8_t *hex, uint16_t len)
{
  uint16_t i;

  for(i = 0; i<len; i++)
  {
    *((uint16_t *)(asc + 2*i)) = LIB_BigLittleEndian_16BitConvert(LIB_HexTo2Asc(hex[i]));
  }
}













