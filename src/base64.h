//
// Created by zhensheng on 9/14/17.
//

#ifndef ZTE_CLIENT_BASE64_H
#define ZTE_CLIENT_BASE64_H

char *base64_encode(const unsigned char *data,
                    size_t input_length,
                    size_t *output_length,
                    void *buffer,
                    size_t buffer_length
);

unsigned char *base64_decode(const char *data,
                             size_t input_length,
                             size_t *output_length,
                             void *buffer,
                             size_t buffer_length
);

size_t encoded_output_length(size_t input_length);
size_t decoded_output_length(size_t input_length);

void build_decoding_table();

void base64_cleanup();


#endif //ZTE_CLIENT_BASE64_H
