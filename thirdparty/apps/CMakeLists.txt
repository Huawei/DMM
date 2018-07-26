LINK_DIRECTORIES(${LIB_PATH_SHARED})
SET(NGINX_URL http://nginx.org/download/nginx-1.12.2.tar.gz)
SET(NGINX_DOWNLOAD_DIR ${CMAKE_CURRENT_LIST_DIR}/nginx/)
SET(NGINX_SRC ${CMAKE_CURRENT_LIST_DIR}/nginx/nginx-1.12.2)
SET(NGINX_RELEASE ${CMAKE_CURRENT_LIST_DIR}/nginx/release)

if(EXISTS "${NGINX_DOWNLOAD_DIR}/nginx-1.12.2.tar.gz")
    SET(NGINX_DOWNLOAD_CMD tar -xvf ${NGINX_DOWNLOAD_DIR}/nginx-1.12.2.tar.gz  -C ${NGINX_DOWNLOAD_DIR}/nginx-1.12.2 --strip-components 1)
else()
    SET(NGINX_DOWNLOAD_CMD wget --no-check-certificate -O nginx-1.12.2.tar.gz ${NGINX_URL} && tar -xvf ${NGINX_DOWNLOAD_DIR}/nginx-1.12.2.tar.gz  -C ${NGINX_DOWNLOAD_DIR}/nginx-1.12.2 --strip-components 1)
endif()

INCLUDE(ExternalProject)
ExternalProject_Add(
		NGINX
		URL ${NGINX_URL}
		DOWNLOAD_DIR ${NGINX_DOWNLOAD_DIR}
		DOWNLOAD_COMMAND ${NGINX_DOWNLOAD_CMD}
		BUILD_IN_SOURCE 1
		SOURCE_DIR ${NGINX_SRC}
		PATCH_COMMAND echo "./configure --with-ld-opt=\"-L${LIB_PATH_SHARED}/ -lnStackAPI -Wl,-rpath=${LIB_PATH_SHARED}\" --sbin-path=${NGINX_RELEASE}/nginx --conf-path=${NGINX_RELEASE}/nginx.conf --pid-path=${NGINX_RELEASE}/nginx.pid " > configure.sh
		COMMAND sed -i -e "48,49 s/^/#/" ${CMAKE_CURRENT_LIST_DIR}/nginx/nginx-1.12.2/auto/os/linux
		CONFIGURE_COMMAND sh configure.sh
		BUILD_COMMAND make
		INSTALL_COMMAND make install
		DEPENDS nStackAPI
)

set_target_properties(NGINX PROPERTIES EXCLUDE_FROM_ALL TRUE)
