#!/bin/sh

## ensure kernel headers are available
sudo apt-get install linux-headers-generic

## ensure signing key exists
dst=/lib/modules/$(uname -r)/build/certs
sudo mkdir -p $dst
cd $dst
if [ ! -e "signing_key.pem" ]; then
sudo tee x509.genkey > /dev/null << 'EOF'
[ req ]
default_bits = 4096
distinguished_name = req_distinguished_name
prompt = no
string_mask = utf8only
x509_extensions = myexts
[ req_distinguished_name ]
CN = Modules
[ myexts ]
basicConstraints=critical,CA:FALSE
keyUsage=digitalSignature
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid
EOF
sudo openssl req -new -nodes -utf8 -sha512 -days 36500 -batch -x509 -config x509.genkey -outform DER -out signing_key.x509 -keyout signing_key.pem
fi
cd -

## build, install, configure
make
sudo make install
sudo cp 99-kromek.rules /etc/udev/rules.d
sudo udevadm control --reload-rules
sudo cp kromekusb.conf /usr/lib/modules-load.d
sudo depmod
