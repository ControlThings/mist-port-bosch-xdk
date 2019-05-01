# This makefile triggers the targets in the application.mk

# The default value "../../.." assumes that this makefile is placed in the 
# folder xdk110/Apps/<App Folder> where the BCDS_BASE_DIR is the parent of 
# the xdk110 folder.
BCDS_BASE_DIR ?= ../../..

# Macro to define Start-up method. change this macro to "CUSTOM_STARTUP" to have custom start-up.
export BCDS_SYSTEM_STARTUP_METHOD = DEFAULT_STARTUP
export BCDS_APP_NAME = mist-port-bosch-xdk
export BCDS_APP_DIR = $(CURDIR)
export BCDS_APP_SOURCE_DIR = $(BCDS_APP_DIR)/source

#Please refer BCDS_CFLAGS_COMMON variable in application.mk file
#and if any addition flags required then add that flags only in the below macro 
#export BCDS_CFLAGS_COMMON = 

export BCDS_TARGET_BOARD = BSP_XDK110
#List all the application header file under variable BCDS_XDK_INCLUDES
export BCDS_XDK_INCLUDES = \
	-I $(BCDS_APP_SOURCE_DIR) \
	-I $(BCDS_APP_SOURCE_DIR)/spiffs/src \
	-I $(BCDS_APP_SOURCE_DIR)/wish-c99/src \
	-I $(BCDS_APP_SOURCE_DIR)/wish-c99/deps/wish-rpc-c99/src \
	-I $(BCDS_APP_SOURCE_DIR)/mist_port \
	-I $(BCDS_APP_SOURCE_DIR)/wish-c99/deps/bson \
	-I $(BCDS_APP_SOURCE_DIR)/wish-c99/deps/uthash/src \
	-I $(BCDS_APP_SOURCE_DIR)/wish-c99/deps/ed25519/src \
	-I $(BCDS_APP_SOURCE_DIR)/wish-c99/deps/mbedtls/include \
	-I $(BCDS_APP_SOURCE_DIR)/mist-c99/wish_app \
	-I $(BCDS_APP_SOURCE_DIR)/mist-c99/src \
	-I $(BCDS_APP_SOURCE_DIR)/apps/mist_config 

#List all the application source file under variable BCDS_XDK_APP_SOURCE_FILES in a similar pattern as below
export BCDS_XDK_APP_SOURCE_FILES = \
	$(BCDS_APP_SOURCE_DIR)/Main.c \
	$(BCDS_APP_SOURCE_DIR)/mist-port-bosch-xdk.c \
	$(BCDS_APP_SOURCE_DIR)/my_tests.c \
	$(BCDS_APP_SOURCE_DIR)/spiffs/src/spiffs_cache.c \
	$(BCDS_APP_SOURCE_DIR)/spiffs/src/spiffs_check.c \
	$(BCDS_APP_SOURCE_DIR)/spiffs/src/spiffs_gc.c \
	$(BCDS_APP_SOURCE_DIR)/spiffs/src/spiffs_hydrogen.c \
	$(BCDS_APP_SOURCE_DIR)/spiffs/src/spiffs_nucleus.c \
	$(BCDS_APP_SOURCE_DIR)/spiffs_integration.c \
	$(BCDS_APP_SOURCE_DIR)/mist_port/port_main.c \
	$(BCDS_APP_SOURCE_DIR)/mist_port/port_net.c \
	$(BCDS_APP_SOURCE_DIR)/mist_port/port_platform.c \
	$(BCDS_APP_SOURCE_DIR)/mist_port/relay_client.c \
	$(BCDS_APP_SOURCE_DIR)/mist_port/port_service_ipc.c \
	$(BCDS_APP_SOURCE_DIR)/mist_port/event.c \
	$(BCDS_APP_SOURCE_DIR)/mist_port/inet_aton.c \
	$(BCDS_APP_SOURCE_DIR)/apps/mist_config/mist_config.c
	
SRC_DIRS = $(BCDS_APP_SOURCE_DIR)/wish-c99/src $(BCDS_APP_SOURCE_DIR)/mist-c99/src $(BCDS_APP_SOURCE_DIR)/wish-c99/deps/bson \
$(BCDS_APP_SOURCE_DIR)/wish-c99/deps/wish-rpc-c99/src $(BCDS_APP_SOURCE_DIR)/wish-c99/deps/mbedtls/library \
$(BCDS_APP_SOURCE_DIR)/wish-c99/deps/ed25519/src

SRC_DIRS += $(BCDS_APP_SOURCE_DIR)/mist-c99/wish_app


BCDS_XDK_APP_SOURCE_FILES += $(foreach sdir,$(SRC_DIRS),$(wildcard $(sdir)/*.c))

export BCDS_CFLAGS_COMMON = -DWITHOUT_STRTOIMAX -DMIST_RPC_REPLY_BUF_LEN=1400 -DMBEDTLS_CONFIG_FILE=\"mist_port/mbedtls_config.h\"

.PHONY: clean	debug release flash_debug_bin flash_release_bin
	
clean: 
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk clean

debug: 
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk debug
	
release: 
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk release
	
flash_debug_bin: 
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk flash_debug_bin
	
flash_release_bin: 
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk flash_release_bin

cleanlint: 
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk cleanlint
	
lint: 
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk lint
	
cdt:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk cdt	