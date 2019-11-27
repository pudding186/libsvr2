#include "./lib_svr_common_def.h"
#include "../include/ssl_tcp.h"

#if OPENSSL_VERSION_NUMBER > 0x1000000fL

SSL_CTX* def_client_ssl_ctx = 0;
int ssl_locks_num = 0;

#ifdef _MSC_VER
struct CRYPTO_dynlock_value
{
	CRITICAL_SECTION lock;
};


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

    if (!def_client_ssl_ctx)
    {
        def_client_ssl_ctx = SSL_CTX_new(SSLv23_method());
        SSL_CTX_set_verify(def_client_ssl_ctx, SSL_VERIFY_NONE, 0);
    }
}
#elif __GNUC__
#include <pthread.h>
struct CRYPTO_dynlock_value
{
	pthread_mutex_t lock;
};

pthread_mutex_t* ssl_locks_arry = 0;

void ssl_lock_callback(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&ssl_locks_arry[n]);
	else
		pthread_mutex_unlock(&ssl_locks_arry[n]);
}

struct CRYPTO_dynlock_value* ssl_lock_dyn_create_callback(const char *file, int line)
{
	struct CRYPTO_dynlock_value *l = (struct CRYPTO_dynlock_value*)malloc(sizeof(struct CRYPTO_dynlock_value));
	pthread_mutex_init(&l->lock, 0);
	return l;
}

void ssl_lock_dyn_callback(int mode, struct CRYPTO_dynlock_value* l, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&l->lock);
	else
		pthread_mutex_unlock(&l->lock);
}

void ssl_lock_dyn_destroy_callback(struct CRYPTO_dynlock_value* l, const char *file, int line)
{
	pthread_mutex_destroy(&l->lock);
	free(l);
}

void ssl_tcp_init(void)
{

	ssl_locks_num = CRYPTO_num_locks();

	if (ssl_locks_num > 0)
	{
		ssl_locks_arry = (pthread_mutex_t*)malloc(ssl_locks_num * sizeof(pthread_mutex_t));
		for (int i = 0; i < ssl_locks_num; ++i)
		{
			pthread_mutex_init(&ssl_locks_arry[i], 0);
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

    if (!def_client_ssl_ctx)
    {
        def_client_ssl_ctx = SSL_CTX_new(SSLv23_method());
        SSL_CTX_set_verify(def_client_ssl_ctx, SSL_VERIFY_NONE, 0);
    }
}

#else
#error "unknown compiler"
#endif



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

bool init_server_ssl_data(net_ssl_core* core, SSL_CTX* ssl_ctx_data)
{
    core->ssl = SSL_new(ssl_ctx_data);
    if (!core->ssl)
    {
        return false;
    }

    core->bio[BIO_RECV] = BIO_new(BIO_s_mem());
    core->bio[BIO_SEND] = BIO_new(BIO_s_mem());

    if ((!core->bio[BIO_RECV]) ||
        (!core->bio[BIO_SEND]))
    {
        return false;
    }

    SSL_set_bio(core->ssl, core->bio[BIO_RECV], core->bio[BIO_SEND]);

    SSL_set_accept_state(core->ssl);

    return true;
}

bool init_client_ssl_data(net_ssl_core* core, SSL_CTX* ssl_ctx_data)
{
    core->ssl = SSL_new(ssl_ctx_data);
    if (!core->ssl)
    {
        return false;
    }

    core->bio[BIO_RECV] = BIO_new(BIO_s_mem());
    core->bio[BIO_SEND] = BIO_new(BIO_s_mem());

    if ((!core->bio[BIO_RECV]) ||
        (!core->bio[BIO_SEND]))
    {
        return false;
    }

    SSL_set_bio(core->ssl, core->bio[BIO_RECV], core->bio[BIO_SEND]);

    SSL_set_connect_state(core->ssl);

    return true;
}

void uninit_ssl_data(net_ssl_core* core)
{
    if (core->ssl)
    {
        SSL_free(core->ssl);
    }
    else
    {
        if (core->bio[BIO_RECV])
        {
            BIO_free(core->bio[BIO_RECV]);
        }

        if (core->bio[BIO_SEND])
        {
            BIO_free(core->bio[BIO_SEND]);
        }
    }
}

bool is_ssl_error(int ssl_error)
{
    switch (ssl_error)
    {
    case SSL_ERROR_NONE:
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_CONNECT:
    case SSL_ERROR_WANT_ACCEPT:
        return false;

    default: return true;
    }
}

#endif


