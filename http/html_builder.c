/*
 * html_builder.c
 *
 *  Created on: May 3, 2026
 *      Author: viorel_serbu
 */

#include "html_builder.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// NOTE:
// HTML escaping depends on context:
// - Text nodes (<td>...</td>): must escape &, <, >
// - Attributes (id="...", value="..."): must also escape " and '
// - Special case: JS inside attributes (e.g. onclick="..."):
//   ' must be escaped (&#39;) to avoid breaking JS strings
//
// Current implementation:
// - html_escape_text_*()  -> for visible text
// - html_escape_attr_*()  -> for attributes and JS contexts
//
// IMPORTANT:
// Do NOT reuse one for the other without reviewing context!

void html_send(httpd_req_t *req, const char *s)
	{
    httpd_resp_send_chunk(req, s, strlen(s));
	}
	
void html_sendf(httpd_req_t *req, const char *fmt, ...)
	{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    html_send(req, buf);
	}

void html_escape_str(char *dst, size_t dst_len, const char *src)
	{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_len - 6 - 1; i++)
	    {
        switch (src[i])
    	    {
            case '&':  strcpy(&dst[j], "&amp;");  j += 5; break;
            case '<':  strcpy(&dst[j], "&lt;");   j += 4; break;
            case '>':  strcpy(&dst[j], "&gt;");   j += 4; break;
            case '"':  strcpy(&dst[j], "&quot;"); j += 6; break;
            case '\'': strcpy(&dst[j], "&#39;");   j += 5; break;
            default:   dst[j++] = src[i]; break;
        	}
    	}
    dst[j] = 0;
	}
char *html_escape_chr(char c, char *output)
	{
	output[0] = 0;
	switch(c)
		{
		case '&':  
			strcpy(output, "&amp;");
			break;
		case '<':  
			strcpy(output, "&lt;");   
			break;
		case '>':  
			strcpy(output, "&gt;");   
			break;
		case '"':  
			strcpy(output, "&quot;");
			break;
        case '\'': 
        	strcpy(output, "&#39;");   
			break;
		default:
			output[0] = c;
			output[1] = 0;
			break;
		}
    return output;
	}	
	
void html_text(httpd_req_t *req, const char *s)
	{
    char buf[256];
    html_escape_str(buf, sizeof(buf), s);
    html_send(req, buf);
	}
	
static int is_safe_id_char(char c)
	{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
	}
void html_id(char *dst, size_t dst_len, const char *src)
	{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_len - 1; i++)
        dst[j++] = is_safe_id_char(src[i]) ? src[i] : '_';
    dst[j] = 0;
	}