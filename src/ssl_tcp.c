#include "./lib_svr_common_def.h"
#include "../include/ssl_tcp.h"

#if OPENSSL_VERSION_NUMBER > 0x1000000fL

struct CRYPTO_dynlock_value
{
    CRITICAL_SECTION lock;
};

int ssl_locks_num = 0;
CRITICAL_SECTION* ssl_locks_arry = 0;

void ssl_lock_callback(int mode, int n, const char *file, int line)
{
    file;
    line;
    if (mode & CRYPTO_LOCK)
        EnterCriticalSection(&ssl_locks_arry[n]);
    else
        LeaveCriticalSection(&ssl_locks_arry[n]);
}

struct CRYPTO_dynlock_value* ssl_lock_dyn_create_callback(const char *file, int line)
{
    file;
    line;
    struct CRYPTO_dynlock_value *l = (struct CRYPTO_dynlock_value*)malloc(sizeof(struct CRYPTO_dynlock_value));
    InitializeCriticalSection(&l->lock);
    return l;
}

void ssl_lock_dyn_callback(int mode, struct CRYPTO_dynlock_value* l, const char *file, int line)
{
    file;
    line;
    if (mode & CRYPTO_LOCK)
        EnterCriticalSection(&l->lock);
    else
        LeaveCriticalSection(&l->lock);
}

void ssl_lock_dyn_destroy_callback(struct CRYPTO_dynlock_value* l, const char *file, int line)
{
    file;
    line;
    DeleteCriticalSection(&l->lock);
    free(l);
}

void ssl_tcp_init(void)
{

    ssl_locks_num = CRYPTO_num_locks();

    if (ssl_locks_num > 0)
    {
        ssl_locks_arry = (CRITICAL_SECTION*)malloc(ssl_locks_num * sizeof(CRITICAL_SECTION));
        for (int i = 0; i < ssl_locks_num; ++i)
        {
            InitializeCriticalSection(&ssl_locks_arry[i]);
        }
    }
#ifdef _DEBUG
    CRYPTO_malloc_debug_init();
    CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif

    CRYPTO_set_locking_callback(&ssl_lock_callback);
    CRYPTO_set_dynlock_create_callback(&ssl_lock_dyn_create_callback);
    CRYPTO_set_dynlock_lock_callback(&ssl_lock_dyn_callback);
    CRYPTO_set_dynlock_destroy_callback(&ssl_lock_dyn_destroy_callback);


    SSL_load_error_strings();
    SSL_library_init();
}

extern SSL_CTX* create_server_ssl_ctx(const char* certificate_file, const char* private_key_file)
{
    SSL_CTX* svr_ssl_ctx = SSL_CTX_new(SSLv23_server_method());

    if (SSL_CTX_use_certificate_file(svr_ssl_ctx, certificate_file, X509_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(svr_ssl_ctx);
        return 0;
    }

    if (SSL_CTX_use_PrivateKey_file(svr_ssl_ctx, private_key_file, SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(svr_ssl_ctx);
        return 0;
    }

    if (!SSL_CTX_check_private_key(svr_ssl_ctx))
    {
        SSL_CTX_free(svr_ssl_ctx);
        return 0;
    }

    return svr_ssl_ctx;
}

extern SSL_CTX* create_client_ssl_ctx(void)
{
    SSL_CTX* cli_ssl_ctx = SSL_CTX_new(SSLv23_method());
    SSL_CTX_set_verify(cli_ssl_ctx, SSL_VERIFY_NONE, 0);

    return cli_ssl_ctx;
}

void destroy_ssl_ctx(SSL_CTX* ssl_ctx)
{
    if (ssl_ctx)
    {
        SSL_CTX_free(ssl_ctx);
    }
}

#endif


