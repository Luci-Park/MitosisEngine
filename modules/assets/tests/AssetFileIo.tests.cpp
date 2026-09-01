#include <assets/AssetFileIo.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstring>
#include <string>

namespace
{
    std::filesystem::path MakeScratchFile()
    {
        static std::atomic<int> counter{0};
        return std::filesystem::temp_directory_path() /
               ("mitosis_assetfileio_test_" + std::to_string(counter.fetch_add(1)));
    }

    std::vector<std::byte> MakeContent(std::string_view text)
    {
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    }
}

TEST_CASE("WriteFileBytes and ReadFileBytes round trip", "[assets][fileio]")
{
    const std::filesystem::path path = MakeScratchFile();
    const std::vector<std::byte> content = MakeContent("round trip payload");

    REQUIRE(mts::WriteFileBytes(path, content));

    const std::optional<std::vector<std::byte>> read = mts::ReadFileBytes(path);
    REQUIRE(read.has_value());
    REQUIRE(read->size() == content.size());
    CHECK(std::memcmp(read->data(), content.data(), content.size()) == 0);

    std::filesystem::remove(path);
}

TEST_CASE("WriteFileBytes and ReadFileBytes round trip empty content", "[assets][fileio]")
{
    const std::filesystem::path path = MakeScratchFile();

    REQUIRE(mts::WriteFileBytes(path, {}));

    const std::optional<std::vector<std::byte>> read = mts::ReadFileBytes(path);
    REQUIRE(read.has_value());
    CHECK(read->empty());

    std::filesystem::remove(path);
}

TEST_CASE("ReadFileBytes returns nullopt for a missing file", "[assets][fileio]")
{
    const std::filesystem::path path = MakeScratchFile();
    CHECK_FALSE(mts::ReadFileBytes(path).has_value());
}

TEST_CASE("WriteFileBytes returns false when the directory does not exist", "[assets][fileio]")
{
    const std::filesystem::path path = MakeScratchFile() / "no_such_dir" / "file.blob";
    CHECK_FALSE(mts::WriteFileBytes(path, MakeContent("data")));
}
