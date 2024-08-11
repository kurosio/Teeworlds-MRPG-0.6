geolite2cpp
============

[![License](https://img.shields.io/badge/license-Apache%202-4EB1BA.svg)](https://www.apache.org/licenses/LICENSE-2.0.html)

C++ API for MaxMind's simplified version of the GeoLite2 Database.

Documentation
=============
Please refer to [http://dev.maxmind.com/](http://dev.maxmind.com/).

# Dependence
```
sudo apt install cmake libmaxminddb-dev
```

# Usage
```
./test -i 36.110.59.146
ip=36.110.59.146, country=47, prov=1, city=0, isp=1
```

If you installed mmdb-bin, then you can use the mmdblookup command to view the IP
```
mmdblookup --file ipdb.mmdb --ip 36.110.59.146

  {
    "city": 
      0 <uint16>
    "country": 
      47 <uint16>
    "isp": 
      1 <uint16>
    "prov": 
      1 <uint16>
  }
```

