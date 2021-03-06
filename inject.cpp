#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <windows.h>

int is_injected = 0;
// this module base. this will be properly set even in the injected process, inject() takes care of that
HMODULE hmodule;

// PID to the original process we were injected from
DWORD parent_pid;

// this is just a buffer to keep an image of the executable at point of entry; before the crt is initialized and before main is called
char*image_mem = 0;
DWORD image_size = 0;

// take an image of the process
// do not use any CRT functions here, since it's not initialized yet
void take_image() {
	char*p = (char*)hmodule;
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)p;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(p + dos->e_lfanew);

	// find first section...
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
	int n_sections = nt->FileHeader.NumberOfSections;

	// ...to find last section
	DWORD begin = (DWORD)p;
	DWORD end = begin + section[n_sections - 1].VirtualAddress + section[n_sections - 1].Misc.VirtualSize;
	image_size = end - begin;

	// any memory allocation function that does not use the CRT will do (that excludes malloc)
	image_mem = (char*)VirtualAlloc(0, image_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	// copy all the memory from the beginning of the module to the end of the last section
	memcpy(image_mem, (void*)begin, image_size);
}

// copy from src in this process to dst in the image
// use to set a variable or memory area in the image before injection

void image_set(const void*dst, const void*src, size_t size) {
	DWORD offset = (DWORD)dst - ((DWORD)hmodule);
	memcpy(image_mem + offset, src, size);
}

// easy way to "copy" a variable or memory area over to the image
void image_copy(const void*p, size_t size) {
	image_set(p, p, size);
}

// this goes through the Import Address Table and loads all the imports
// essentially the same as Windows does upon loading a module
// note that it just silently ignores errors, so if it fails to load a module, the process might crash later
// this function is provided only for completeness, and I do not recommend using it
void do_iat() {
	const char*p = (const char*)hmodule;
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)p;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(p + dos->e_lfanew);
	PIMAGE_IMPORT_DESCRIPTOR import = (PIMAGE_IMPORT_DESCRIPTOR)(p + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	while (import->Characteristics) {
		HMODULE hm = LoadLibraryA(p + import->Name);
		if (hm) {
			DWORD*dw = (DWORD*)(p + import->OriginalFirstThunk);
			int i;
			for (i = 0; *dw; i++) {
				FARPROC proc;
				if (*dw & 0x80000000) proc = GetProcAddress(hm, (LPCSTR)(*dw && 0xFFFF)); // load by ordinal
				else proc = GetProcAddress(hm, p + *dw + 2); // load by name
				if (proc) {
					*((FARPROC*)(p + import->FirstThunk) + i) = proc; // set the value in the bound IAT
				} else {
					// failed to load proc
				}
				++dw;
			}
		} else {
			// failed to load module
		}
		++import;
	}
}

// just to be compatible with C, we wrap these in ifdefs
// mainCRTStartup and start need to have C symbol names

#ifdef __cplusplus
extern "C"
#endif
void mainCRTStartup();
#ifdef __cplusplus
extern "C"
#endif
void start() {
	hmodule = GetModuleHandle(0);
	// take image before CRT is initialized
	take_image();
	mainCRTStartup();
}

void injected_start() {
	is_injected = 1;
	// uncomment next line if you want to be able to inject into further processes
	take_image();

	// fix up the IAT
	// I would actually recommend not doing this, instead only use functions from kernel32, which is guaranteed to be loaded in every process (and at the same address, to boot)
	// but, for completeness, I've included it here
	//do_iat();

	mainCRTStartup();
}

// This function resolves imports from kernel32 in to_hm by locating
// GetModuleHandleA and GetProcAddress from the imports of from_hm.
// Usually unnecessary, but under Wine the address of kernel32 is
// variable.
void import_kernel32(HMODULE from_hm, HMODULE to_hm) {
	const char* p = (const char*)from_hm;
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)p;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(p + dos->e_lfanew);
	PIMAGE_IMPORT_DESCRIPTOR import = (PIMAGE_IMPORT_DESCRIPTOR)(p + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	HMODULE (__stdcall*local_GetModuleHandleA)(const char*) = nullptr;
	FARPROC (__stdcall*local_GetProcAddress)(HMODULE, const char*) = nullptr;
	for (; import->Characteristics; ++import) {
		if (!_stricmp(p + import->Name, "kernel32") || !_stricmp(p + import->Name, "kernel32.dll")) {
			DWORD*dw = (DWORD*)(p + import->OriginalFirstThunk);
			int i;
			for (i = 0; *dw; ++i, ++dw) {
				if (~*dw & 0x80000000) {
					const char* name = p + *dw + 2;
					FARPROC proc = *((FARPROC*)(p + import->FirstThunk) + i);
					if (!strcmp(name, "GetModuleHandleA")) local_GetModuleHandleA = (HMODULE(__stdcall*)(const char*))proc;
					else if (!strcmp(name, "GetProcAddress")) local_GetProcAddress = (FARPROC(__stdcall*)(HMODULE, const char*))proc;
				}
			}
		}
	}

	HMODULE kernel32 = local_GetModuleHandleA("kernel32.dll");

	p = (const char*)to_hm;
	dos = (PIMAGE_DOS_HEADER)p;
	nt = (PIMAGE_NT_HEADERS)(p + dos->e_lfanew);
	import = (PIMAGE_IMPORT_DESCRIPTOR)(p + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	for (; import->Characteristics; ++import) {
		if (!_stricmp(p + import->Name, "kernel32") || !_stricmp(p + import->Name, "kernel32.dll")) {
			DWORD*dw = (DWORD*)(p + import->OriginalFirstThunk);
			int i;
			for (i = 0; *dw; ++i, ++dw) {
				FARPROC proc;
				if (*dw & 0x80000000) proc = local_GetProcAddress(kernel32, (LPCSTR)(*dw && 0xFFFF)); // load by ordinal
				else proc = local_GetProcAddress(kernel32, p + *dw + 2); // load by name
				if (proc) {
					*((FARPROC*)(p + import->FirstThunk) + i) = proc; // set the value in the bound IAT
				} else {
					// failed to load proc
				}
			}
		}
	}
}

struct inject_info {
	void* base = nullptr;
	void* entry = nullptr;
};

inject_info inject(HANDLE h_proc, bool create_remote_thread) {

	char*p = (char*)hmodule;
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)p;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(p + dos->e_lfanew);

	DWORD begin = (DWORD)p;
	DWORD end = begin + image_size;
	int start_offset = (ptrdiff_t)&injected_start - begin; // offset of the entry point for the injected code
														   // allocate memory in the target process for the image
	char*mem = (char*)VirtualAllocEx(h_proc, 0, image_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!mem) {
		printf("VirtualAllocEx failed; error %d\n", GetLastError());
		fflush(stdout);
		return {};
	}

	// calling GetModuleHandle(0) from the injected code would return the module of the target process,
	// so we set hmodule in the image to the target memory here
	image_set(&hmodule, &mem, sizeof(hmodule));

	// now we must do base relocation, since we are probably loading the code in a different memory area then where we took the image from :)
	// this is essentially the same thing Windows does whenever loading a module in a different location than it's desired base address
	// the executable must be linked with a relocation section, otherwise it will crash bad after injecting

	char*tmp_mem = (char*)malloc(image_size);
	memcpy(tmp_mem, image_mem, image_size);

	{
		// the relocation section is basically a list of IMAGE_BASE_RELOCATION entries
		// each entry has a virtual address and then a list of WORDs
		// the top 4 bits of each WORD specify a relocation type, and the bottom 12 specify an offset
		// (for x86, all base relocations are of type IMAGE_REL_BASED_HIGHLOW)
		// each offset should be added to the virtual address of the IMAGE_BASE_RELOCATION to get the address of a DWORD
		// subtract begin from that DWORD and add mem, and the relocation is done
		PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)(p + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		while (reloc->VirtualAddress) {
			DWORD d = (DWORD)(p + reloc->VirtualAddress);
			PIMAGE_BASE_RELOCATION next = (PIMAGE_BASE_RELOCATION)((char*)reloc + reloc->SizeOfBlock);
			WORD*w = (WORD*)(reloc + 1);
			while ((char*)w < (char*)next) {
				if (*w >> 12 == IMAGE_REL_BASED_HIGHLOW) {
					DWORD d2 = d + (*w & 0xFFF);
					if (d2 >= begin&&d2 < end) {
						DWORD*d = (DWORD*)(d2 - begin + (DWORD)tmp_mem);
						*d -= begin - (DWORD)mem;
					}
				}
				++w;
			}
			reloc = next;
		}
	}

	// write it into the allocated memory in the target process!
	if (!WriteProcessMemory(h_proc, mem, tmp_mem, image_size, nullptr)) {
		printf("WriteProcessMemory failed; %d", GetLastError());
		fflush(stdout);
		free(tmp_mem);
		return {};
	}
	free(tmp_mem);

	if (create_remote_thread) {
		// create the remote thread...
		HANDLE h = CreateRemoteThread(h_proc, NULL, 0, (LPTHREAD_START_ROUTINE)(mem + start_offset), 0, 0, 0);
		if (!h) {
			printf("CreateRemoteThread failed; error %d", GetLastError());
			fflush(stdout);
			return {};
		}
		CloseHandle(h);
		// ...and the rest is up to fate
	}
	inject_info r;
	r.base = mem;
	r.entry = mem + start_offset;
	return r;
}



