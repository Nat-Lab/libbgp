#ifndef BGP_ERROR_H_
#define BGP_ERROR_H_
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#warning "bgp_error is deprecated. use bgp_log_handler."

namespace libbgp {

void _bgp_error (const char *format_str, ...);

const char *get_bgp_errors();
void clear_bgp_errors();

}

#endif // BGP_ERROR_H_