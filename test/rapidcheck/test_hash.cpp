#include <gtest/gtest.h>
#include "Sha256Hash.h"

TEST(Sha256HashTest, HelloWorldHash) {
    Sha256Hash hasher;
    std::string data = "hello world";
    hasher.update(data.data(), data.size());
    std::string result = hasher.finalize();
    
    // The known SHA-256 hash for "hello world"
    EXPECT_EQ(result, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}

TEST(Sha256HashTest, EmptyStringHash) {
    Sha256Hash hasher;
    std::string result = hasher.finalize();
    
    // The known SHA-256 hash for an empty string
    EXPECT_EQ(result, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256HashTest, ChunkedUpdateHash) {
    Sha256Hash hasher;
    std::string data1 = "hello";
    std::string data2 = " ";
    std::string data3 = "world";
    
    hasher.update(data1.data(), data1.size());
    hasher.update(data2.data(), data2.size());
    hasher.update(data3.data(), data3.size());
    std::string result = hasher.finalize();
    
    EXPECT_EQ(result, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}
