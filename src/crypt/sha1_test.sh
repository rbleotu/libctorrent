#! /usr/bin/sh

test_bin=./test
test_file=test.in
sizes=( 8 16 32 64 512 1024 2048 1048576 8388608 )

echo 'SHA-1 implementation test'

for i in ${sizes[@]}
do
    printf "Testing on %7u bytes ... " $i
    openssl rand -out $test_file $i
    d1=`$test_bin  $test_file`
    d2=`sha1sum  $test_file | cut -d' ' -f1`
    rm -f $test_file
    if [ $d1 != $d2 ]
    then
        echo 'ERROR!'
        exit 1
    fi
    echo 'OK!'
done
