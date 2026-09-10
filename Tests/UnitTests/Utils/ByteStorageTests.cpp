#include <array>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <Utils/ByteStorage.h>

namespace
{

    TEST(ByteStorageTest, ValueCanBeWrittenAndRead) {
        std::array<std::byte, sizeof(std::uint32_t)> storage{};

        ASSERT_TRUE(byte_storage::write(storage, 0, std::uint32_t{0x12345678}));
        const auto value = byte_storage::read<std::uint32_t>(storage, 0);

        ASSERT_TRUE(value.hasValue());
        EXPECT_EQ(value.value(), 0x12345678);
    }

    TEST(ByteStorageTest, ValueCanBeReadAndWrittenAtEndOfStorage) {
        std::array<std::byte, 8> storage{};
        constexpr std::size_t offset{storage.size() - sizeof(std::uint32_t)};

        ASSERT_TRUE(byte_storage::write(storage, offset, std::uint32_t{0x12345678}));
        const auto value = byte_storage::read<std::uint32_t>(storage, offset);

        ASSERT_TRUE(value.hasValue());
        EXPECT_EQ(value.value(), 0x12345678);
    }

    TEST(ByteStorageTest, ReadFailsForOutOfRangeOrTruncatedOffset) {
        std::array<std::byte, sizeof(std::uint32_t)> storage{};

        EXPECT_FALSE(byte_storage::read<std::uint32_t>(storage, storage.size() + 1).hasValue());
        EXPECT_FALSE(byte_storage::read<std::uint32_t>(storage, storage.size() - 1).hasValue());
    }

    TEST(ByteStorageTest, FailedWriteDoesNotModifyStorage) {
        std::array<std::byte, sizeof(std::uint32_t)> storage;
        storage.fill(std::byte{0xA5});
        const auto originalStorage = storage;

        EXPECT_FALSE(byte_storage::write(storage, 1, std::uint32_t{0x12345678}));
        EXPECT_EQ(storage, originalStorage);
    }

}
