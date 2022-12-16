GIT_VERSION="$(shell git describe  --dirty --always --tags  )"
GIT_HASH=$(shell git rev-parse --verify HEAD | awk 'BEGIN{line="";comma=""}{for (i=0;i<length($$0);i+=8){line =line comma " 0x" substr($$0,i,8);comma=","}}END{print line}')
GIT_REPO="$(shell git ls-remote --get-url)"

VERSION_FILE=src/ApolloSM_device/Version.cc

define VERSION_BODY
#include <ApolloSM_device/Version.hh>                                                                                                   
std::vector<uint32_t> const nVer({$(GIT_HASH)});
char const sVer[]           = $(GIT_VERSION);
char const uri[]            = $(GIT_REPO);
endef
export VERSION_BODY

$(VERSION_FILE):
	@echo "$$VERSION_BODY">$(VERSION_FILE)

clean_version:
	@rm -f $(VERSION_FILE)
