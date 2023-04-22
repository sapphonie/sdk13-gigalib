#ifndef BYTEPATCH_H
#define BYTEPATCH_H

#ifdef _WIN32
#pragma once
#endif
// #include <functional>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <sys/mman.h>
#endif

#include <vector>

class BytePatch
{
    void *addr{ 0 };
    size_t size;
    std::vector<unsigned char> patch_bytes;
    std::vector<unsigned char> original;
    bool patched{ false };

public:
    ~BytePatch()
    {
        Shutdown();
    }
    // BytePatch(std::function<uintptr_t(const char *)> SigScanFunc, const char *pattern, size_t offset, std::vector<unsigned char> patch) : patch_bytes{ patch }
    // {
    //     addr = (void *) SigScanFunc(pattern);
    //     if (!addr)
    //     {
    //         printf("perish\n");
    //         return;
    //         //logging::Info("Signature not found");
    //         //throw std::runtime_error("Signature not found");
    //     }
    //     addr = static_cast<void *>(static_cast<char *>(addr) + offset);
    //     size = patch.size();
    //     original.resize(size);
    //     Copy();
    // }
    BytePatch(uintptr_t addr, std::vector<unsigned char> patch) : addr( reinterpret_cast<void *>(addr) ), patch_bytes( patch )
    {
        size = patch.size();
        original.resize(size);
        Copy();
    }
    BytePatch(void *addr, std::vector<unsigned char> patch) : addr( addr ), patch_bytes( patch )
    {
        size = patch.size();
        original.resize(size);
        Copy();
    }

    #ifdef _WIN32
    void Copy()
    {
        void *page          = (void*) ( (uint64_t) addr);
        void *end_page      = (void*) ( (uint64_t)(addr) + size);
        uintptr_t mprot_len = (uint64_t) end_page - (uint64_t) page;


        DWORD dummy;
        VirtualProtect(page, mprot_len, PAGE_EXECUTE_READWRITE, &dummy);
        memcpy(&original[0], addr, size);
        VirtualProtect(page, mprot_len, PAGE_EXECUTE_READ, &dummy);
    }
    void Patch()
    {
        if (!patched)
        {
            void *page          = (void*) ( (uint64_t) addr);
            void *end_page      = (void*) ( (uint64_t)(addr) + size);
            uintptr_t mprot_len = (uint64_t) end_page - (uint64_t) page;

            DWORD dummy;
            VirtualProtect(page, mprot_len, PAGE_EXECUTE_READWRITE, &dummy);
            memcpy(addr, &patch_bytes[0], size);
            VirtualProtect(page, mprot_len, PAGE_EXECUTE_READ, &dummy);

            patched = true;
        }
    }
    void Shutdown()
    {
        if (patched)
        {
            void *page          = (void*) ( (uint64_t) addr);
            void *end_page      = (void*) ( (uint64_t)(addr) + size);
            uintptr_t mprot_len = (uint64_t) end_page - (uint64_t) page;

            DWORD dummy;
            VirtualProtect(page, mprot_len, PAGE_EXECUTE_READWRITE, &dummy);
            memcpy(addr, &original[0], size);
            VirtualProtect(page, mprot_len, PAGE_EXECUTE_READ, &dummy);

            patched = false;
        }
    }
    #else

    // copy our 
    void Copy()
    {
        // linux needs to be aligned to page sizes
        void* page          = (void*)(   (uint64_t)addr          & ~0xFFF);
        void* end_page      = (void*)( ( (uint64_t)addr + size ) & ~0xFFF);
        uintptr_t mprot_len = (uint64_t)end_page - (uint64_t)page + 0xFFF;

        // rwx our mem
        mprotect(page, mprot_len, PROT_READ | PROT_WRITE | PROT_EXEC);
        // void* memcpy( void* dest, const void* src, std::size_t count );
        memcpy(&original[0], addr, size);
        mprotect(page, mprot_len, PROT_READ | PROT_EXEC);
    }
    void Patch()
    {
        if (!patched)
        {
            void* page              = (void*)( ( (uint64_t)(addr)        ) & ~0xFFF);
            void* end_page          = (void*)( ( (uint64_t)(addr) + size ) & ~0xFFF);
            uintptr_t mprot_len     = (uint64_t)end_page - (uint64_t)page + 0xFFF;

            // rwx our mem
            mprotect(page, mprot_len, PROT_READ | PROT_WRITE | PROT_EXEC);
            // copy our patch bytes to where the func is
            memcpy(addr, &patch_bytes[0], size);
            // clean up
            mprotect(page, mprot_len, PROT_READ | PROT_EXEC);
            patched = true;
        }
    }
    void Shutdown()
    {
        if (patched)
        {
            void* page              = (void*)( ( (uint64_t)(addr)        ) & ~0xFFF);
            void* end_page          = (void*)( ( (uint64_t)(addr) + size ) & ~0xFFF);
            uintptr_t mprot_len     = (uint64_t)end_page - (uint64_t)page + 0xFFF;

            // rwx our mem
            mprotect(page, mprot_len, PROT_READ | PROT_WRITE | PROT_EXEC);
            // copy the original bytes back to where they go
            memcpy(addr, &original[0], size);
            // clean up
            mprotect(page, mprot_len, PROT_READ | PROT_EXEC);
            patched = false;
        }
    }
#endif

};

#endif // BYTEPATCH_H