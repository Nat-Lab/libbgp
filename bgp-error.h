#ifndef BGP_ERROR_H_
#define BGP_ERROR_H_
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

namespace bgpfsm {

char __bgp_error[255];
uint8_t __bgp_err_offset = 0;
bool __bgp_err_buf_init = false;

void _bgp_error (const char *format_str, ...);

const char *get_bgp_errors();

}

#endif // BGP_ERROR_H_