./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc -b transfer-test -k 10Ghpc   -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc2 -b transfer-test -k 10Ghpc2 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc3 -b transfer-test -k 10Ghpc3 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc4 -b transfer-test -k 10Ghpc4 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc5 -b transfer-test -k 10Ghpc5 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc6 -b transfer-test -k 10Ghpc6 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc7 -b transfer-test -k 10Ghpc7 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc8 -b transfer-test -k 10Ghpc8 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc8 -b transfer-test -k 10Ghpc9 -j $1 &
./s3bin/s3-upload -e http://nimbus.pawsey.org.au:8080 -f 10Ghpc8 -b transfer-test -k 10Ghpc10 -j $1 &
wait