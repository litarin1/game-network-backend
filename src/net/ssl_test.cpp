#include <err.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdint.h>
#include <stdio.h>

// for SSL_SESSION Caching
const unsigned char session_cache_id[] = "SSL_TEST_01_VERSIYA";

int main(){
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == nullptr){
        ERR_print_errors_fp(stderr);
        errx(EXIT_FAILURE, "Failed to create server SSL_CTX");
    }
    // configure SSL_CTX
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION)){
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        errx(EXIT_FAILURE, "Failed to set the minimum TLS protocol version (hardcoded TLS1.3)");
    }
    uint64_t opts = 0;
    /*
     * Tolerate clients hanging up without a TLS "shutdown".
     * Appropriate in all application protocols which perform
     * their own message "framing", and don't rely on TLS to
     * defend against "truncation" attacks.
     */
    opts |= SSL_OP_IGNORE_UNEXPECTED_EOF;

    /*
     * Block potential CPU-exhaustion attacks by clients that request frequent
     * renegotiation.  This is of course only effective if there are existing
     * limits on initial full TLS handshake or connection rates.
     */
    opts |= SSL_OP_NO_RENEGOTIATION;

    /*
     * Most servers elect to use their own cipher preference rather than that of
     * the client.
     */
    opts |= SSL_OP_CIPHER_SERVER_PREFERENCE;
    SSL_CTX_set_options(ctx, opts);

    /*
     * Load the server's certificate *chain* file (PEM format), which includes
     * not only the leaf (end-entity) server certificate, but also any
     * intermediate issuer-CA certificates.  The leaf certificate must be the
     * first certificate in the file.
     *
     * In advanced use-cases this can be called multiple times, once per public
     * key algorithm for which the server has a corresponding certificate.
     * However, the corresponding private key (see below) must be loaded first,
     * *before* moving on to the next chain file.
     */
    if (SSL_CTX_use_certificate_chain_file(ctx, "chain.pem") <= 0){
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        errx(EXIT_FAILURE, "Failed to load the server certificate chain file");
    }

    /*
     * Load the corresponding private key, this also checks that the private
     * key matches the just loaded end-entity certificate.  It does not check
     * whether the certificate chain is valid, the certificates could be
     * expired, or may otherwise fail to form a chain that a client can
     * validate.
     */
    if (SSL_CTX_use_PrivateKey_file(ctx, "pkey.pem", SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        errx(EXIT_FAILURE,"Error loading the server private key file, "
             "possible key/cert mismatch???");
    }

    // Enable SSL_SESSION caching
    SSL_CTX_set_session_id_context(ctx, session_cache_id, sizeof(session_cache_id));
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
    // How many client TLS sessions to cache.
    // The default is 20k (SSL_SESSION_CACHE_MAX_SIZE_DEFAULT)
    SSL_CTX_sess_set_cache_size(ctx, 64);
    // t in seconds. the default is two hours
    SSL_CTX_set_timeout(ctx, 3600);
    
    // Explicitly disable clients sertificate store
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);


    // bind BIO

    // This actually does not creates the socket.
    // The first call to BIO_do_accept() do
    BIO* acceptor_bio = BIO_new_accept("20202");
    if (acceptor_bio == nullptr){
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        errx(EXIT_FAILURE, "Error creating acceptor BIO");
    }
    // create socket
    BIO_set_bind_mode(acceptor_bio, BIO_BIND_REUSEADDR);
    if (BIO_do_accept(acceptor_bio) <= 0) {
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        errx(EXIT_FAILURE, "Error setting up acceptor socket (BIO is created, but socker error)");
    }

    for (;;){
        BIO* client_bio;
        SSL* ssl;
        unsigned char buf[8192];
        size_t nread;
        size_t nwrite;
        size_t total = 0;

        ERR_clear_error();

        if (BIO_do_accept(acceptor_bio) <= 0){
            // Client went away before we accepted the connection
            continue;
        }
        client_bio = BIO_pop(acceptor_bio);
        printf("New client accepted!!!\n");
        ssl = SSL_new(ctx);
        if (ssl == nullptr){
            ERR_print_errors_fp(stderr);
            warnx("Error creating SSL handle for new connection");
            BIO_free(client_bio);
            continue;
        }
        SSL_set_bio(ssl, client_bio, client_bio);

        // SSL handshake
        if (SSL_accept(ssl) <= 0){
            ERR_print_errors_fp(stderr);
            warnx("Error performing SSL handshake with client");
            SSL_free(ssl);
            continue;
        }

        // Run echo
        while (SSL_read_ex(ssl, buf, sizeof(buf), &nread) > 0){
            printf("%.*s", nread, buf);
            if (SSL_write_ex(ssl, buf, nread, &nwrite) > 0 && nwrite == nread){
                total += nwrite;
                continue;
            }
            warnx("Error echoing client input");
            break;
        }
        printf("\nConnection closed; %zu bytes sent\n", total);
        SSL_free(ssl);
        // do not free BIO because it will be freed by SSL_free()
    }
    SSL_CTX_free(ctx);
    return EXIT_SUCCESS;
}