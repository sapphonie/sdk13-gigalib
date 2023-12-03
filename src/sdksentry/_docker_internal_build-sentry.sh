#!/bin/bash
# sentry
pwd

rm -rfv sentry-native

cd /sentry

SentryVersion="0.6.7"
git config --global --add safe.directory '*'
git clone https://github.com/getsentry/sentry-native --recursive --depth=1 -b ${SentryVersion}
cd ./sentry-native

    # static
    cmake -B build                                              \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo                       \
        -DSENTRY_BUILD_SHARED_LIBS=OFF                          \
        -DSENTRY_BUILD_FORCE32=yes                              \
        -DCMAKE_LIBRARY_PATH=/usr/lib/i386-linux-gnu            \
        -DCMAKE_INCLUDE_PATH=/usr/include/i386-linux-gnu
    cmake --build build --parallel || exit 69

    # shared - do we need to do this
    # cmake -B build                                          \
    #     -DCMAKE_BUILD_TYPE=RelWithDebInfo                   \
    #     -DSENTRY_BUILD_SHARED_LIBS=ON                       \
    #     -DSENTRY_BUILD_FORCE32=yes                          \
    #     -DCMAKE_LIBRARY_PATH=/usr/lib/i386-linux-gnu        \
    #     -DCMAKE_INCLUDE_PATH=/usr/include/i386-linux-gnu
    # cmake --build build --parallel

cd ..
find . -name libbreakpad_client.a   -exec cp {} ./ -v \;
find . -name libsentry.a            -exec cp {} ./ -v \;
rm -rfv sentry-native



exit 0