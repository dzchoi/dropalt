## Dynamic memory allocation

#### malloc() is used even if you don't explicitly call malloc() or new.
The malloc() function is often utilized behind the scenes by various standard library functions. Even if you don't directly call malloc() or new, dynamic memory allocation may occur in the background to support certain operations. For example, malloc() is employed in the following functions:

- __libc_init_array(): This function is part of the C runtime initialization process. It calls constructors for global and static C++ objects and may use malloc() to allocate memory for the initialization of these objects.

- printf(): The printf() function itself can also indirectly use malloc() for buffering, especially when dealing with formatted output that requires dynamic memory.

- _dtoa_r(): When you call printf("%f", ...) to print floating-point numbers, the _dtoa_r() function is used to convert double values to strings. This conversion process often requires dynamic memory allocation, relying on malloc() to obtain the necessary memory.

By understanding that malloc() is used internally by these and other standard library functions, you can better anticipate and manage memory allocation in your applications.

#### NUM_HEAPS
The file riot/cpu/cortexm_common/Makefile.include sets `CFLAGS += -DCPU_HAS_BACKUP_RAM=1`. Additionally, riot/cpu/sam0_common/include/cpu_conf.h defines `#define NUM_HEAPS (2)`

The _sbrk_r() function in riot/sys/newlib_syscalls_default/syscalls.c can allocate memory from these two non-contiguous heaps.

#### _sbrk_r()
The _sbrk_r() function is a reentrant version of the _sbrk() function, commonly used in embedded systems for dynamic memory allocation. It's part of the Newlib C library and often used with embedded operating systems like RIOT.

This function reserves memory from the heap at runtime. Typically, it is called by functions like malloc(), calloc(), and realloc() when they need more memory. When these functions run out of memory to allocate, they call _sbrk_r() to request more heap memory. _sbrk_r() consumes the heap and never frees it; freed memory is handled within the free() and realloc() functions in the library.

When using Newlib or Newlib-nano, you must provide your own implementation of the _sbrk_r() function, which RIOT implements in riot/sys/newlib_syscalls_default/syscalls.c. However, by redefining _malloc_r(), _calloc_r(), and _realloc_r(), which are defined by Newlib, you can avoid calling _sbrk_r().

This mechanism might mislead the mallinfo() reading for heap information, as it will report no available heap memory even though more memory can still be allocated using malloc(). It will continue to report no available memory until some memory is explicitly freed using free().

#### Thread safety
The _malloc_r() function in Newlib-nano is not inherently thread-safe. Newlib and Newlib-nano require additional platform-specific support to ensure thread safety.

Newlib provides hooks for thread safety, such as __malloc_lock() and __malloc_unlock(), which can be defined to use platform-specific synchronization primitives like mutexes. By implementing these hooks, you can make the memory allocation functions thread-safe in a multi-threaded environment.

#### __wrap_malloc()
Instead, RIOT selects the malloc_thread_safe module for Cortex-M, which offers thread-safe wrappers for malloc(), calloc(), realloc(), and free(). These wrappers ensure safe dynamic memory allocation in multi-threaded environments, preventing race conditions and ensuring proper memory management.

To achieve this, the malloc_thread_safe module uses the linker option `-Wl,--wrap=malloc` to replace malloc() with __wrap_malloc(), and similarly for calloc(), realloc(), and free(). The wrapper functions are defined in riot/sys/malloc_thread_safe/malloc_wrappers.c.

Note that only the malloc() functions resolved by the linker are replaced. The functions within the Newlib library are not affected and already use the existing malloc().

#### new in C++
When you select the cpp module in RIOT, the new and delete operators are overridden with RIOT's own implementations located in riot/sys/cpp_new_delete/new_delete.cpp. These custom implementations internally call malloc() and free(), ensuring the compatibility with RIOT's memory management and thread safety mechanisms.

#### TLSF
RIOT offers the TLSF memory allocator as an alternative to the existing malloc() and related functions. See https://doc.riot-os.org/group__pkg__tlsf__malloc.html and https://deepwiki.com/mattconte/tlsf/3-tlsf-api-reference.

- Real-Time Suitability: TLSF is better suited for real-time and embedded systems.
- Predictable Performance: TLSF offers constant-time allocation and deallocation, ensuring predictable performance.
- Minimized Fragmentation: TLSF reduces fragmentation more effectively than Newlib-nano's malloc().
- Lower Overhead: TLSF incurs less overhead compared to Newlib-nano's malloc().

Although the TLSF allocator itself is not inherently thread-safe, the overriding functions in riot/pkg/tlsf/contrib/newlib.c are wrapped in irq_disable()/irq_restore() to ensure thread safety.

You can replace the default malloc() and related memory allocation functions with TLSF-based allocators using the implementation provided in riot/pkg/tlsf/contrib/newlib.c. To do so, apply the following changes:
```
[Makefile]
# Substitute newlib_nano's malloc() with tlsf_malloc() for global dynamic memory
# allocation. This also applies to C++'s new operator, as implemented in the RIOT
# cpp_new_delete module.
USEPKG += tlsf
USEMODULE += tlsf-malloc
LINKFLAGS += -Wl,--allow-multiple-definition

[board.c]
#ifndef RIOTBOOT
#include "tlsf-malloc.h"        // for tlsf_add_global_pool()
#endif

void post_startup(void)
{
    ...
    tlsf_add_global_pool(&_sheap, &_eheap - &_sheap);
}

[lua.cpp]
// LUA_MEM_SIZE is maximized, accounting for a ~3.75KB reservation in the main heap
// (&_eheap - &_sheap). This reserved space must exceed:
//  * tlsf_size() (= 3320), required by tlsf_create()
//  * + ~320 bytes, estimated usage by fputs() and stdin_init().
static constexpr size_t LUA_MEM_SIZE = 105 *1024;

[main.cpp]
#include "tlsf.h"               // for tlsf_size()

// tlsf_size() returns the size of the metadata and internal structures (= 3320 bytes)
// for TLSF allocator.
LOG_DEBUG("Main: max heap size is %d bytes", &_eheap - &_sheap - tlsf_size());
```

This modification will reduce the binary size by ~500 bytes, while increasing RAM usage by 3320 bytes (=tlsf_size()) to accommodate metadata and internal structures required by the TLSF allocator.
