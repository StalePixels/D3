NAME = D3
BUILD_DIR ?= ./BUILD-$(NAME)
TMP_DIR ?= ./TMP-$(NAME)
SRC_DIRS ?= ./src
DOCS_DIR ?= ./docs
FILES_DIR ?= ./dist
RELEASE_DIR ?= ./RELEASE-$(NAME)
INSTALL_BASE ?= /Volumes/devnext
INSTALL_DIR ?= $(INSTALL_BASE)/$(NAME)

Z88DK_CRT = 0

#DEBUGFLAGS := --list --c-code-in-asm

CC=zcc
CCFLAGS=+zxn -vn -O3 -startup=$(Z88DK_CRT) -clib=new -pragma-include:zpragma.inc
CZFLAGS=-Cz="--clean --fullsize --main-fence 0xC000"
LDFLAGS=-m -lm
INCFLAGS=

SH := /bin/sh
ECHO := @/bin/echo -n
CD := cd
RSYNC := rsync -parv
MKDIR := mkdir -p
RM := rm -rfv
LSH := ls -larth
CP := @cp -rv
MV := @mv -v
FIGLET := @`which figlet`
CAT := @`which cat`
SED := @`which sed`
LS := ls -l@k
VERSION := `cat VERSION`

clean:
	$(RM) $(BUILD_DIR)
	$(RM) $(TMP_DIR)
	$(RM) $(NAME).map
	$(RM) $(NAME)_*.bin
	$(RM) cmake-build-debug
	$(RM) RELEASE/*

deps: incs
	$(ECHO) $(NAME) > NAME

dirs:
	$(MKDIR) $(BUILD_DIR)
	$(MKDIR) $(TMP_DIR)

incs:
	$(ECHO) "#" > $(TMP_DIR)/BANNER
	$(CAT) $(TMP_DIR)/BUILD >> $(TMP_DIR)/BANNER
	$(ECHO) " RUNNING" >> $(TMP_DIR)/BANNER
	$(CAT)  $(TMP_DIR)/BANNER | figlet
	cat templates/build_top.h > src/common/build.h
	cat templates/build_build.h >> src/common/build.h
	$(ECHO) `git shortlog -s -n| grep Rimron| cut -b-7|sed -e 's/^[[:space:]]*//'` > $(TMP_DIR)/BUILD
	$(CAT) $(TMP_DIR)/BUILD >> src/common/build.h
	echo "\"" >> src/common/build.h
	cat templates/build_ver.h >> src/common/build.h
	$(ECHO) `git shortlog -s -n| grep Rimron| cut -b-7|sed -e 's/^[[:space:]]*//'` > $(TMP_DIR)/BUILD
	$(CAT) VERSION >> src/common/build.h
	echo "\"" >> src/common/build.h
	$(ECHO) >> src/common/build.h

	cat templates/build_bottom.h >> src/common/build.h

	$(CAT) VERSION > $(TMP_DIR)/BANNER
	$(ECHO) " done!" >> $(TMP_DIR)/BANNER
	$(CAT) $(TMP_DIR)/BANNER | figlet

all: ini

ini: deps dirs
	$(CC) $(CCFLAGS) $(LDFLAGS) @ini.lst -oINI -create-app \
		-subtype=dotn $(CZFLAGS)
	$(MV) INI $(BUILD_DIR)/INI.DOT

install: install_ini

install_ini:
	$(CP) $(BUILD_DIR)/INI.DOT $(INSTALL_BASE)/dot/INI

q_ini: ini install_ini

uninstall:
	$(RM) $(INSTALL_BASE)/dot/INI
