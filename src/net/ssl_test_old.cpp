#include <memory.h>
#include <openssl/encoder.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdio.h>

// const unsigned char msg[] = "helo!";
const unsigned char msg[] = {0x01, 0x02, 0x03, 0x04, 0x05};

int main() {
    puts("todo");
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_MD *digest_alghoritm = EVP_MD_fetch(NULL, "SHA2-256", NULL);
    unsigned char* out = nullptr;

    if (ctx == nullptr){
        EVP_MD_CTX_free(ctx);
        return 1;
    }
    if (digest_alghoritm == nullptr) {
        EVP_MD_free(digest_alghoritm);
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    if (!EVP_DigestInit_ex(ctx, digest_alghoritm, NULL)) {
        EVP_MD_free(digest_alghoritm);
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    if (!EVP_DigestUpdate(ctx, msg, sizeof(msg))) {
        EVP_MD_free(digest_alghoritm);
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    out = (unsigned char*) OPENSSL_malloc(EVP_MD_get_size(digest_alghoritm));
    if (out == nullptr) {
        OPENSSL_free(out);
        EVP_MD_free(digest_alghoritm);
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    unsigned int len = 0;
    if (!EVP_DigestFinal_ex(ctx, out, &len)) {
        EVP_MD_free(digest_alghoritm);
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    // EVP_ENCODE_CTX *e_ctx = EVP_ENCODE_CTX_new();
    EVP_CIPHER_CTX *e_ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_init(e_ctx);
    EVP_CIPHER_CTX_set_padding(e_ctx, 0);
    EVP_CIPHER_CTX_set_flags(e_ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-192-CCM", NULL);
    printf("%d %d ", e_ctx, cipher);
    // EVP_EncodeInit(e_ctx);
    const unsigned char key[] = "1234";
    const unsigned char iv[] = "1";
    printf("%d ", EVP_EncryptInit_ex(e_ctx, cipher, NULL, key, iv));
    // according to man evp_encryptupdate
    // unsigned char e_out[3*sizeof(msg)*EVP_CIPHER_get_block_size(cipher)];
    // printf("s%d", EVP_CIPHER_get_block_size(cipher));
    unsigned char e_out[1024];
    bzero(e_out, sizeof(e_out));
    int e_len = 0;
    printf("%d-", EVP_EncryptUpdate(e_ctx, e_out, &e_len, msg, sizeof(msg)));
    printf("%d ", e_len);
    printf("%d-", EVP_EncryptFinal_ex(e_ctx, e_out, &e_len));
    printf("%d ", e_len);
    char err_buf[80];
    ERR_error_string(ERR_get_error(), err_buf);
    printf("\n[%s]\n", err_buf);

    printf("msg: %.*s\n", sizeof(msg), msg);
    printf("digest out (%d): %.*s\n", len, len, out);
    BIO_dump_fp(stdout, out, len);
    printf("encrypt out (%d): %.*s\n", e_len, e_len, e_out);
    BIO_dump_fp(stdout, e_out, e_len);

    EVP_MD_free(digest_alghoritm);
    EVP_MD_CTX_free(ctx);

    return 0;
}