# LibreTDS
A fork of FreeTDS with changes that wont be accepted into mainline FreeTDS.
These changes mostly break backwards compatibility. FreeTDS' dblib library
has been altered to be more useful for certain applications.

# License
LibreTDS is a fork of FreeTDS and uses the same license as that project.
LGPL 2.1.

# Requirements
The build process uses libtool, so the following must be installed first
on Debian based systems:
```
sudo apt-get install libtool-bin
```

# Building
After meeting the requirements, it's as simple as:

```
./configure
make
sudo make install
```

