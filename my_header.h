#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>



extern "C" {

const char *c_append(const char *ip_addr, const char *key, const char *value);

const char *c_delete_in_tmp(const char *ip_addr, const char *key);

const char *c_delete_tmp(const char *ip_addr);

const uint8_t *c_execute_binary(const char *ip_addr,
                                const char *filename,
                                const char *const *args,
                                size_t count,
                                const uint8_t *stdin_input,
                                size_t stdin_len,
                                size_t *out_len);

const char *c_get(const char *ip_addr, const char *key);

const char *c_hello(const char *ip_addr);

const char *c_put(const char *ip_addr, const char *key, const char *value);

const char *c_remove_binary(const char *ip_addr, const char *filename);

const char *c_send_binary(const char *ip_addr, const uint8_t *binary, size_t len);

bool c_some_binary(const char *ip_addr, const char *name);

bool c_stop_execution(const char *ip_addr);

const char *c_store_in_tmp(const char *ip_addr, const char *key);

void free_string(const char *ptr);

void free_vec(void *ptr, size_t length, size_t capacity);

}  // extern "C"
