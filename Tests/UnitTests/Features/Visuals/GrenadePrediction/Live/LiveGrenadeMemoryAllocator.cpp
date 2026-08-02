#include <MemoryAllocation/MemoryAllocatorBase.h>

std::byte* MemoryAllocatorBase::allocate(std::size_t size) noexcept
{
    return new std::byte[size];
}

void MemoryAllocatorBase::deallocate(std::byte* memory, std::size_t) noexcept
{
    delete[] memory;
}
