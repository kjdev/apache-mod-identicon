# mod_identicon #

mod_identicon is identicon handler module for Apache HTTPD Server.

## Dependencies ##

* GD
* libmemcached [optional]

## Build ##

    % ./autogen.sh (or autoreconf -i)
    % ./configure [OPTION]
    % make
    % make install

### Build Options ###

enable memcache. [Default: disable]

* --enable-identicon-memcache

apache path.

* --with-apxs=PATH
* --with-apr=PATH
* --with-apreq2=PATH

gd path. [Default: /usr/include]

* --with-gd=PATH

libmemcached path. [Default: /usr/include/libmemcached]

* --with-libmemcached=PATH

## Configration ##

httpd.conf:

    LoadModule identicon_module modules/mod_identicon.so
    <Location /identicon>
        SetHandler identicon
    </Location>

enable memcache:

    IdenticonMemcacheHost   localhost:11211
    IdenticonMemcacheExpire 30

## Request Parameter ##

 parameter | description
 --------- | -----------------------------
 u         | user hash
 s         | image size (default: 80)
 t         | background(white) transparent

## Example ##

    http://localhost/identicon?u=xxxxxxxxxx
    http://localhost/identicon?u=xxxxxxxxxx&s=40
    http://localhost/identicon?u=xxxxxxxxxx&s=40&t=1
