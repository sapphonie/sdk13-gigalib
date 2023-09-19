$SentryVersion = "0.6.5"

rm -Recurse -Force sentry-native

git clone https://github.com/getsentry/sentry-native --recursive --depth=1 -b $SentryVersion

copy .\sentry-native\include\sentry.h .\vendored\sentry.h

cd sentry-native

cmake --build build --target clean
cmake -B build `
-DCMAKE_BUILD_TYPE=RelWithDebInfo `
-DSENTRY_BUILD_SHARED_LIBS=ON `
-DSENTRY_BUILD_FORCE32=ON `
-DCRASHPAD_WER_ENABLED=ON `
-DCMAKE_GENERATOR_PLATFORM=Win32

cmake --build build --parallel

cd ..

copy .\sentry-native\build\Debug\sentry.dll ..\..\bin\
copy .\sentry-native\build\Debug\sentry.lib ..\..\bin\
copy .\sentry-native\build\Debug\sentry.pdb ..\..\bin\
copy .\sentry-native\build\crashpad_build\handler\Debug\crashpad_handler.exe ..\..\bin\
copy .\sentry-native\build\crashpad_build\handler\Debug\crashpad_handler.pdb ..\..\bin\
copy .\sentry-native\build\crashpad_build\handler\Debug\crashpad_wer.dll ..\..\bin\
copy .\sentry-native\build\crashpad_build\handler\Debug\crashpad_wer.pdb ..\..\bin\

rm -Recurse -Force sentry-native


pause