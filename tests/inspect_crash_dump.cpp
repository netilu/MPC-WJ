// Offline x64 minidump inspection. Reads dump memory only; never attaches to a process.
#include <windows.h>
#include <dbghelp.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#pragma comment(lib, "dbghelp.lib")

struct Range { ULONG64 base, size, offset; };
static std::vector<Range> ranges;
static std::vector<unsigned char> dump;
static MINIDUMP_MODULE_LIST* modules;
struct Image { ULONG64 base; std::vector<unsigned char> bytes; };
static std::vector<Image> images;

static PVOID CALLBACK FunctionTable(HANDLE process, DWORD64 address)
{
    for (auto& image : images) {
        if (address < image.base || address - image.base >= image.bytes.size()) continue;
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.bytes.data());
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image.bytes.data() + dos->e_lfanew);
        const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (directory.VirtualAddress + directory.Size > image.bytes.size()) return nullptr;
        auto table = reinterpret_cast<RUNTIME_FUNCTION*>(image.bytes.data() + directory.VirtualAddress);
        for (DWORD i = 0; i < directory.Size / sizeof(RUNTIME_FUNCTION); ++i) {
            if (address - image.base >= table[i].BeginAddress && address - image.base < table[i].EndAddress) return &table[i];
        }
    }
    return SymFunctionTableAccess64(process, address);
}

static BOOL CALLBACK ReadDump(HANDLE, DWORD64 address, PVOID buffer, DWORD size, LPDWORD read)
{
    *read = 0;
    for (const auto& r : ranges) {
        if (address >= r.base && address - r.base < r.size) {
            const auto count = std::min<ULONG64>(size, r.size - (address - r.base));
            const auto offset = r.offset + address - r.base;
            if (offset > dump.size() || count > dump.size() - offset) return FALSE;
            memcpy(buffer, dump.data() + offset, static_cast<size_t>(count));
            *read = static_cast<DWORD>(count);
            return TRUE;
        }
    }
    for (const auto& image : images) {
        if (address >= image.base && address - image.base < image.bytes.size()) {
            const auto offset = static_cast<size_t>(address - image.base);
            const auto count = std::min<size_t>(size, image.bytes.size() - offset);
            memcpy(buffer, image.bytes.data() + offset, count);
            *read = static_cast<DWORD>(count);
            return TRUE;
        }
    }
    return FALSE;
}

static void* Stream(MINIDUMP_STREAM_TYPE type)
{
    PMINIDUMP_DIRECTORY directory = nullptr;
    void* data = nullptr;
    ULONG size = 0;
    return MiniDumpReadDumpStream(dump.data(), type, &directory, &data, &size) ? data : nullptr;
}

static void PrintAddress(HANDLE process, DWORD64 address)
{
    for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
        const auto& m = modules->Modules[i];
        if (address >= m.BaseOfImage && address - m.BaseOfImage < m.SizeOfImage) {
            auto name = reinterpret_cast<MINIDUMP_STRING*>(dump.data() + m.ModuleNameRva);
            std::wstring path(name->Buffer, name->Length / sizeof(wchar_t));
            wprintf(L"%ls+0x%llx", path.substr(path.find_last_of(L"\\/") + 1).c_str(), address - m.BaseOfImage);
            alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
            auto symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            if (SymFromAddr(process, address, &displacement, symbol)) {
                printf(" (%s+0x%llx)", symbol->Name, displacement);
            }
            return;
        }
    }
    printf("0x%llx", address);
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) return 2;
    FILE* file = nullptr;
    if (_wfopen_s(&file, argv[1], L"rb") || !file) return 3;
    _fseeki64(file, 0, SEEK_END);
    const auto size = _ftelli64(file);
    if (size < sizeof(MINIDUMP_HEADER)) { fclose(file); return 4; }
    dump.resize(static_cast<size_t>(size));
    _fseeki64(file, 0, SEEK_SET);
    const auto got = fread(dump.data(), 1, dump.size(), file);
    fclose(file);
    if (got != dump.size()) return 4;
    if (auto list = static_cast<MINIDUMP_MEMORY_LIST*>(Stream(MemoryListStream))) {
        for (ULONG i = 0; i < list->NumberOfMemoryRanges; ++i) {
            auto& m = list->MemoryRanges[i];
            ranges.push_back({ m.StartOfMemoryRange, m.Memory.DataSize, m.Memory.Rva });
        }
    }
    if (auto list = static_cast<MINIDUMP_MEMORY64_LIST*>(Stream(Memory64ListStream))) {
        ULONG64 offset = list->BaseRva;
        for (ULONG64 i = 0; i < list->NumberOfMemoryRanges; ++i) {
            auto& m = list->MemoryRanges[i];
            ranges.push_back({ m.StartOfMemoryRange, m.DataSize, offset });
            offset += m.DataSize;
        }
    }
    auto threads = static_cast<MINIDUMP_THREAD_LIST*>(Stream(ThreadListStream));
    auto names = static_cast<MINIDUMP_THREAD_NAME_LIST*>(Stream(ThreadNamesStream));
    modules = static_cast<MINIDUMP_MODULE_LIST*>(Stream(ModuleListStream));
    auto exception = static_cast<MINIDUMP_EXCEPTION_STREAM*>(Stream(ExceptionStream));
    if (!threads || !modules || !exception) return 5;
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_EXACT_SYMBOLS);
    // Explicit local search path: no symbol server or network requests.
    if (!SymInitializeW(process, L".", FALSE)) return 6;
    for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
        auto& m = modules->Modules[i];
        auto name = reinterpret_cast<MINIDUMP_STRING*>(dump.data() + m.ModuleNameRva);
        std::wstring path(name->Buffer, name->Length / sizeof(wchar_t));
        // Supply matching image unwind data when the minidump omits code pages.
        HMODULE mapped = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (mapped) {
            auto base = reinterpret_cast<unsigned char*>(reinterpret_cast<ULONG_PTR>(mapped) & ~3ull);
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            if (nt->FileHeader.TimeDateStamp == m.TimeDateStamp && nt->OptionalHeader.SizeOfImage == m.SizeOfImage) {
                Image image{m.BaseOfImage, std::vector<unsigned char>(m.SizeOfImage)};
                memcpy(image.bytes.data(), base, nt->OptionalHeader.SizeOfHeaders);
                auto section = IMAGE_FIRST_SECTION(nt);
                for (unsigned j = 0; j < nt->FileHeader.NumberOfSections; ++j) {
                    if (section[j].VirtualAddress + section[j].SizeOfRawData <= m.SizeOfImage) {
                        memcpy(image.bytes.data() + section[j].VirtualAddress, base + section[j].VirtualAddress, section[j].SizeOfRawData);
                    }
                }
                images.push_back(std::move(image));
            }
            FreeLibrary(mapped);
        }
        SymLoadModuleExW(process, nullptr, path.c_str(), nullptr, m.BaseOfImage, m.SizeOfImage, nullptr, 0);
    }
    printf("Exception 0x%08lx, thread %lu, address ", exception->ExceptionRecord.ExceptionCode, exception->ThreadId);
    PrintAddress(process, exception->ExceptionRecord.ExceptionAddress);
    printf("\n");
    for (ULONG i = 0; i < threads->NumberOfThreads; ++i) {
        const auto& thread = threads->Threads[i];
        const bool crashed = thread.ThreadId == exception->ThreadId;
        const auto& location = crashed ? exception->ThreadContext : thread.ThreadContext;
        if (location.DataSize < sizeof(CONTEXT)) continue;
        CONTEXT context;
        memcpy(&context, dump.data() + location.Rva, sizeof(context));
        STACKFRAME64 frame = {};
        frame.AddrPC = { context.Rip, 0, AddrModeFlat };
        frame.AddrStack = { context.Rsp, 0, AddrModeFlat };
        frame.AddrFrame = { context.Rbp, 0, AddrModeFlat };
        printf("\nThread %lu%s\n", thread.ThreadId, crashed ? " CRASHED" : "");
        if (names) {
            for (ULONG j = 0; j < names->NumberOfThreadNames; ++j) {
                if (names->ThreadNames[j].ThreadId == thread.ThreadId) {
                    auto name = reinterpret_cast<MINIDUMP_STRING*>(dump.data() + names->ThreadNames[j].RvaOfThreadName);
                    wprintf(L"Name: %.*ls\n", name->Length / sizeof(wchar_t), name->Buffer);
                }
            }
        }
        PrintAddress(process, context.Rip);
        printf("\n");
        for (int depth = 0; depth < 32; ++depth) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(thread.ThreadId)),
                &frame, &context, ReadDump, FunctionTable, SymGetModuleBase64, nullptr) || !frame.AddrPC.Offset) break;
            if (!SymGetModuleBase64(process, frame.AddrPC.Offset)) {
                printf("[Unwind stopped: return address is outside known modules]\n");
                break;
            }
            PrintAddress(process, frame.AddrPC.Offset);
            printf("\n");
        }
    }
    SymCleanup(process);
}
