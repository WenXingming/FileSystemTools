#ifndef RAPIDCHECK_SHA256_HASH_H
#define RAPIDCHECK_SHA256_HASH_H

#include <string>
#include <vector>
#include <cstdint>
#include <openssl/evp.h>

class Sha256Hash {
public:
    Sha256Hash();
    ~Sha256Hash();

    // Prevent copy/move for simplicity
    Sha256Hash(const Sha256Hash&) = delete;
    Sha256Hash& operator=(const Sha256Hash&) = delete;

    void update(const void* data, size_t size);
    std::string finalize();
    void reset();

private:
    EVP_MD_CTX* ctx_;
    bool finalized_ = false;
};

#endif // RAPIDCHECK_SHA256_HASH_H
