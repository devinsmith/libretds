# LibreTDS
A fork of FreeTDS with changes that wont be accepted into mainline FreeTDS.
These changes mostly break backwards compatibility.

# Requirements
The build process uses libtool, so the following must be installed first
on Debian based systems:
```
sudo apt-get install libtool-bin
```

# Building
It's as simple as:

```
./configure
make
sudo make install
```

