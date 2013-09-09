#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

int main() {
	RSA* rsa;
	EVP_PKEY *pkey;
	int ret, inlen, outlen;
	char data[100], out[500];
	EVP_MD_CTX md_ctx, md_ctx2;

	strcpy(data, "{\"uid\":\"xxx\"}");
	rsa = RSA_generate_key(1024, RSA_3, NULL, NULL);

	printf("%s\n", BN_bn2hex(rsa->n));
	printf("%s\n", BN_bn2hex(rsa->e));

	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);

	EVP_MD_CTX_init(&md_ctx);
	if(EVP_SignInit_ex(&md_ctx, EVP_sha256(), NULL) != 1){
		goto err;
	}

	if(EVP_SignUpdate(&md_ctx, data, inlen) != 1){
		goto err;
	}

	if(EVP_SignFinal(&md_ctx, out, &outlen, pkey) != 1){
		goto err;
	}

	printf("rsa sig sizeof %d\n", sizeof(out));
	printf("%d\n", outlen);

	EVP_MD_CTX_init(&md_ctx2);
	if(EVP_VerifyInit_ex(&md_ctx2, EVP_sha256(), NULL) != 1){
		goto err;
	}

	if(EVP_VerifyUpdate(&md_ctx2, data, inlen) != 1){
		goto err;
	}

	if(EVP_VerifyFinal(&md_ctx2, out, outlen, pkey) != 1){
		printf("error\n");
	} else {
		printf("ok\n");
	}

err:
	RSA_free(rsa);
	return 0;
}