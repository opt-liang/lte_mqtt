#ifndef __COMMOM__
#define __COMMOM__

#define ITM_Port8(n)    (*((volatile unsigned char *)(0xE0000000+4*n)))
#define ITM_Port16(n)   (*((volatile unsigned short*)(0xE0000000+4*n)))
#define ITM_Port32(n)   (*((volatile unsigned long *)(0xE0000000+4*n)))

#define DEMCR           (*((volatile unsigned long *)(0xE000EDFC)))
#define TRCENA          0x01000000

extern char* memstr(const char* full_data, const char* substr, int full_data_len);
//#define findstring(x,y,z)   memstr(x,y,z)
char* findstring(const char* full_data, const char* substr, int full_data_len);
void LIB_nByteHexTo2Asc(char *asc, uint8_t *hex, uint16_t len);
#endif


