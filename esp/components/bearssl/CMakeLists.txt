set(brssl_path ${CMAKE_CURRENT_LIST_DIR}/../../../vendor/BearSSL)

idf_component_register(SRC_DIRS
	${brssl_path}/src/aead
	${brssl_path}/src/codec
	${brssl_path}/src/ec
	${brssl_path}/src/hash
	${brssl_path}/src/int
	${brssl_path}/src/kdf
	${brssl_path}/src/mac
	${brssl_path}/src/rand
	${brssl_path}/src/rsa
	${brssl_path}/src/ssl
	${brssl_path}/src/symcipher
	${brssl_path}/src/x509
	${brssl_path}/src
	INCLUDE_DIRS ${brssl_path}/inc
	PRIV_INCLUDE_DIRS ${brssl_path}/src
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
	-DBR_USE_ALT_RAND -DBR_USE_UNIX_TIME
)
