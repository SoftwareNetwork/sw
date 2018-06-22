// SW C API

#ifndef _SW_SW_C_H_
#define _SW_SW_C_H_

typedef void *sw_context_t;

sw_context_t *sw_init(const char *config_file = 0);
void sw_finish(sw_context_t *ctx);

void *sw_alloc(int size);
void  sw_free(void *p);

#endif
