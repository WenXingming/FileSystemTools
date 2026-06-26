#include "Sha256Hash.h"
#include <stdexcept>
#include <iomanip>
#include <sstream>

Sha256Hash::Sha256Hash() {
    ctx_ = EVP_MD_CTX_new();
    if (!ctx_) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }
    if (EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx_);
        throw std::runtime_error("Failed to initialize SHA256 context");
    }
}

Sha256Hash::~Sha256Hash() {
    if (ctx_) {
        EVP_MD_CTX_free(ctx_);
    }
}

void Sha256Hash::reset() {
    if (EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("Failed to re-initialize SHA256 context");
    }
    finalized_ = false;
}

void Sha256Hash::update(const void* data, size_t size) {
    if (finalized_) throw std::runtime_error("Cannot update finalized SHA256 context");
    if (EVP_DigestUpdate(ctx_, data, size) != 1) {
        throw std::runtime_error("Failed to update SHA256 context");
    }
}

std::string Sha256Hash::finalize() {
    if (finalized_) throw std::runtime_error("SHA256 context already finalized");
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    if (EVP_DigestFinal_ex(ctx_, hash, &lengthOfHash) != 1) {
        throw std::runtime_error("Failed to finalize SHA256 context");
    }

    finalized_ = true;
    std::stringstream ss;
    for (unsigned int i = 0; i < lengthOfHash; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}
