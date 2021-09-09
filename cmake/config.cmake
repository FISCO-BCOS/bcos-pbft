hunter_config(bcos-framework VERSION 3.0.0-local
    URL https://${URL_BASE}/FISCO-BCOS/bcos-framework/archive/7daefa6c031a80b70557e8a6bf04fc05d6a9828a.tar.gz
    SHA1 b4f93eb69ee43a168e444749c6b65d101e8ed31d
    CMAKE_ARGS HUNTER_PACKAGE_LOG_BUILD=ON HUNTER_PACKAGE_LOG_INSTALL=ON HUNTER_KEEP_PACKAGE_SOURCES=ON #DEBUG=ON
)

hunter_config(wedpr-crypto VERSION 1.1.0-10f314de
	URL https://${URL_BASE}/WeBankBlockchain/WeDPR-Lab-Crypto/archive/10f314de45ec31ce9e330922b522ce173662ed33.tar.gz
	SHA1 626df59f87ea2c6bb5128f7d104588179809910b
	CMAKE_ARGS HUNTER_PACKAGE_LOG_BUILD=OFF HUNTER_PACKAGE_LOG_INSTALL=ON
)