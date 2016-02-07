/*
 * cryptpe -- Encryption tool for PE binaries
 * (C) 2012-2016 Martin Wolters
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#include <stdint.h>
#include <stdio.h>
#include <Windows.h>

#include "huffman_enc.h"

#include "..\shared\salsa20.h"

static void getrandom(uint8_t *dst, const int len) {
	HCRYPTPROV provider;
	
	CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	CryptGenRandom(provider, len, dst);
	CryptReleaseContext(provider, 0);	
}

static void printtable(char *name, const uint8_t *bin, const size_t len, salsa20_ctx_t *ctx) {
	size_t i;
	uint8_t *salsa_buf;

	printf("static uint8_t %s[] = {", name);
	for(i = 0; i < len; i++) {
		if(!(i % 32))
			printf("\n\t");
		if(ctx && !(i % SALSA20_BLOCK_SIZE))
			salsa_buf = salsa20_gen_block(ctx);

		printf("0x%02x", bin[i] ^ (ctx ? salsa_buf[i % SALSA20_BLOCK_SIZE] : 0));
		if(i < (len - 1))
			printf(", ");
	}
	printf("\n};\n\n");	
}

int main(int argc, char **argv) {
	salsa20_ctx_t salsa20_ctx;
	uint8_t salsa20_key[SALSA20_KEY_SIZE];
	uint8_t salsa20_nonce[SALSA20_NONCE_SIZE];

	uint8_t *file_buf, *enc_tree, *encoded;
	size_t pos = 0, file_size, tree_size;
	hfm_node_t *root;
	hfm_cdb_t *codebook;
	FILE *fp;
	
	if(argc == 1) {
		fprintf(stderr, "USAGE: %s <FILENAME>\n", argv[0]);
		return EXIT_FAILURE;
	}

	if((fp = fopen(argv[1], "rb")) == NULL) {
		fprintf(stderr, "ERROR: Could not open '%s'.\n", argv[1]);
		return EXIT_FAILURE;
	}
	
	printf("/* THIS FILE HAS BEEN AUTOMATICALLY GENERATED BY MKBINTABLE */\n\n");
	printf("#include <stdint.h>\n\n#ifndef BINTABLE_H_\n#define BINTABLE_H_\n\n");

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	file_buf = malloc(file_size);
	fseek(fp, 0, SEEK_SET);
	fread(file_buf, file_size, 1, fp);
	printf("static size_t file_size = %d;\n\n", file_size);

	getrandom(salsa20_key, SALSA20_KEY_SIZE);
	getrandom(salsa20_nonce, SALSA20_NONCE_SIZE);
	salsa20_ctx = salsa20_init(salsa20_key, salsa20_nonce, SALSA20_KEY_SIZE);

	printtable("key", salsa20_key, SALSA20_KEY_SIZE, NULL);
	printtable("nonce", salsa20_nonce, SALSA20_NONCE_SIZE, NULL);

	root = maketree(file_buf, file_size);
	enc_tree = encode_tree(root, &tree_size);
	printtable("tree", enc_tree, tree_size, &salsa20_ctx);

	codebook = make_codebook(root);
	encoded = encode(file_buf, file_size, codebook, &file_size);
	printtable("binary", encoded, file_size, &salsa20_ctx);
	printf("#endif\n");

	free(codebook);
	free(enc_tree);
	free(file_buf);
	free_tree(root);
	fclose(fp);

	return EXIT_SUCCESS;
} 
