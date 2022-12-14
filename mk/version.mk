GIT_VERSION="$(shell git --no-pager  show -s --format="commit date %ci  %d"  ; git describe  --dirty --always --tags )"
GIT_HASH="$(shell git rev-parse --verify HEAD)"
GIT_REPO="$(shell git remote -v)"


define VERSION_BODY
#include <ApolloSM_device/Version.hh>                                                                                                   
VersionTracker const Version = VersionTracker({$(GIT_HASH)},
                                              $(GIT_VERSION),
                                              $(GIT_REPO));
endef
export VERSION_BODY

all:
        @echo "$$VERSION_BODY">src/ApolloSM_device/version.cc
