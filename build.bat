@echo off

set FLAGS=-Zi -W4 -FC -GR- -EHa- -fp:except -nologo -Zf

pushd build\debug

cl %FLAGS% ..\..\main.c

popd