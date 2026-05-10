/*
 * html_builder.h
 *
 *  Created on: May 3, 2026
 *      Author: viorel_serbu
 */

#ifndef HTTP_HTML_BUILDER_H_
#define HTTP_HTML_BUILDER_H_

#include "esp_http_server.h"

void html_send(httpd_req_t *req, const char *s);
void html_sendf(httpd_req_t *req, const char *fmt, ...);
void html_text(httpd_req_t *req, const char *s);
void html_attr(httpd_req_t *req, const char *s);
void html_id(char *dst, size_t dst_len, const char *src);
char *html_escape_chr(char c, char *output);
void html_escape_str(char *dst, size_t dst_len, const char *src);



#endif /* HTTP_HTML_BUILDER_H_ */
