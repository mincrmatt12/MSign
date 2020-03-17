#include "TimeLib.h"
#include <cstdio>
#include <cstring>
#include <oauth1.h>
#include <Arduino.h>
#include <libb64/cencode.h>
#include "bearssl/bearssl_hmac.h"
#include "config.h"
#include "util.h"

// Helper routines

bool percent_escape_char_needed(char ch)
{
	if ((ch >= '0' && ch <= '9') ||
		(ch >= 'A' && ch <= 'Z') ||
		(ch >= 'a' && ch <= 'z') ||
		 ch == '-' || ch == '.'  ||
		 ch == '_' || ch == '~')
	{
		return false;
	}
	return true;
}

// Main api

size_t oauth1::percent_encoded_length(const char *in) {
	int i;
	for (i = 0; *in != 0; ++in) {
		++i;
		if (percent_escape_char_needed(*in)) i += 2;
	}
	return i;
}

void oauth1::percent_encode(const char *in, char *out) {
	for (int i = 0; i < strlen(in); ++i) {
		if (percent_escape_char_needed(in[i])) {
			*out++ = '%';
			snprintf(out, 3, "%02X", in[i]);
			out += 2;
		}
		else {
			*out++ = in[i];
		}
	}
	*out = 0;
}

char * oauth1::generate_authorization(const char * const params[][2], const char * base_url, const char * method) {
	// Step 1: Construct the parameter string.
	//
	// This entails:
	// 	- Percent encode them
		
	// Step 1a: Compute the size of the parameter string
	char timestamp[16] = {0};
	snprintf(timestamp, 16, "%ld", now());
	
	char nonce[33] = {0};
	for (int i = 0; i < 32; ++i) {
		auto j = random(3);
		if (j == 0) nonce[i] = '0' + random(10);
		else if (j == 1) nonce[i] = 'A' + random(26);
		else        nonce[i] = 'a' + random(26);
	}

	// Load secure params
	char * secure_params = strdup(config::manager.get_value(config::Entry::ALERT_TWITTER_API_KEY));
	char * consumer_key = strtok(secure_params, ":");
	char * consumer_secret = strtok(nullptr, ":");
	char * api_token = strtok(nullptr, ":");
	char * token_secret = strtok(nullptr, ":");
	char sig_base_str[500] = {0};

	{
		char unencoded_param_str[400] = {0}; // null
		int addi = 0;

		int param_provided_index = 0;
		for (;params[param_provided_index][0] != nullptr; ++param_provided_index) {
			if (params[param_provided_index][0][0] >= 'o') break;
			strcpy(unencoded_param_str + addi, params[param_provided_index][0]);
			addi += strlen(params[param_provided_index][0]);
			unencoded_param_str[addi++] = '=';
			percent_encode(params[param_provided_index][1], unencoded_param_str + addi);
			addi += percent_encoded_length(params[param_provided_index][1]);
			unencoded_param_str[addi++] = '&'; // always will be followed by oauth stuff
		}
		
		strcpy_P(unencoded_param_str + addi, PSTR("oauth_consumer_key="));
		addi += 19;
		percent_encode(consumer_key, unencoded_param_str + addi);
		addi += percent_encoded_length(consumer_key);
		unencoded_param_str[addi++] = '&';
		addi += sprintf_P(unencoded_param_str + addi, PSTR("oauth_nonce=%s&oauth_signature_method=HMAC-SHA1&oauth_timestamp=%s&oauth_token="), nonce, timestamp);
		percent_encode(api_token, unencoded_param_str + addi);
		addi += percent_encoded_length(api_token);
		unencoded_param_str[addi++] = '&';
		addi += sprintf_P(unencoded_param_str + addi, PSTR("oauth_version=1.0"));

		for (;params[param_provided_index][0] != nullptr; ++param_provided_index) {
			unencoded_param_str[addi++] = '&';
			strcpy(unencoded_param_str + addi, params[param_provided_index][0]);
			addi += strlen(params[param_provided_index][0]);
			unencoded_param_str[addi++] = '=';
			percent_encode(params[param_provided_index][1], unencoded_param_str + addi);
			addi += percent_encoded_length(params[param_provided_index][1]);
		}

		unencoded_param_str[addi] = 0;

		Log.println(F("oauth1 paramstr:"));
		Log.println(unencoded_param_str);

		addi = 0;

		// Create signature base string
		
		addi += sprintf_P(sig_base_str, PSTR("%s&"), method);
		percent_encode(base_url, sig_base_str + addi);
		addi += percent_encoded_length(base_url);
		sig_base_str[addi++] = '&';
		percent_encode(unencoded_param_str, sig_base_str + addi);
		addi += percent_encoded_length(unencoded_param_str);

		sig_base_str[addi] = 0;
		Log.println(F("sig_base_str"));
		Log.println(sig_base_str);
	}

	char encoded_signature[48] = {0};

	// Sign that shit
	{
		// Create a signing key
		size_t sign_key_length = 1 + percent_encoded_length(consumer_secret) + percent_encoded_length(token_secret);
		char sign_key[sign_key_length + 1];
		sign_key[sign_key_length] = 0;
		percent_encode(consumer_secret, sign_key);
		percent_encode(token_secret, sign_key + (1 + percent_encoded_length(consumer_secret)));
		sign_key[percent_encoded_length(consumer_secret)] = '&';

		br_hmac_key_context kctx;
		br_hmac_key_init(&kctx, &br_sha1_vtable, sign_key, sign_key_length);

		br_hmac_context ctx;
		br_hmac_init(&ctx, &kctx, 0);

		// churn it in
		br_hmac_update(&ctx, sig_base_str, strlen(sig_base_str));

		// dump it out
		char unencoded_signature[20];
		br_hmac_out(&ctx, unencoded_signature);
		
		// encode that stuff
		base64_encodestate encoder;

		char signature[48] = {0};

		base64_init_encodestate(&encoder);
		base64_encode_blockend(signature + base64_encode_block(unencoded_signature, 20, signature, &encoder), &encoder);

		percent_encode(signature, encoded_signature);
	}

	// Laziness workaround

	Log.println(F("Signature:"));
	Log.println(encoded_signature);

	char * out = (char *)malloc(400);
	memset(out, 0, 400);

	char encoded_consumer_key[percent_encoded_length(consumer_key)]; percent_encode(consumer_key, encoded_consumer_key);
	char encoded_api_token[percent_encoded_length(api_token)]; percent_encode(api_token, encoded_api_token);

	snprintf_P(out, 400, PSTR("OAuth oauth_consumer_key=\"%s\", oauth_nonce=\"%s\", oauth_signature=\"%s\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\"%s\", oauth_token=\"%s\", oauth_version=\"1.0\""), 
			encoded_consumer_key, nonce, encoded_signature, timestamp, encoded_api_token);

	Log.println(F("Authorizing with:"));
	Log.println(out);

	free(secure_params);

	return out;
}
