#!/bin/bash
dockerimage="registry.gitlab.steamos.cloud/steamrt/soldier/sdk:latest"
# we do this so that we can be agnostic about where we're invoked from
# meaning you can exec this script anywhere and it should work the same
thisiswhereiam=${BASH_SOURCE[0]}
# this should be /whatever/directory/structure/Open-Fortress-Source
script_folder=$( cd -- "$( dirname -- "${thisiswhereiam}" )" &> /dev/null && pwd )
pushd "${script_folder}" &> /dev/null || exit 99



# this is relative to our current dir
internalscript="_docker_internal_build-sentry.sh"

thisdir=$(pwd)

# add -it flags automatically if in null tty
itflag=""
if [ -t 0 ] ; then
    itflag="-it"
else
    itflag=""
fi

docker run ${itflag}            \
-v ${thisdir}:/"sentry"         \
${dockerimage}                  \
bash ./sentry/${internalscript} "$@"

ecodereal=$?
echo "real exit code ${ecodereal}"


mv  libbreakpad_client.a    ../../bin/ -v
mv  libsentry.a             ../../bin/ -v

exit ${ecodereal}
# --user "$(id -u):$(id -g)"				\
# -v "${dev_srcdir}"/${build_dir}/.ccache:/root/.ccache   \
