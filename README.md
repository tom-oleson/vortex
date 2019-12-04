<pre>
         ______
        / ____ \
   ____/ /    \ \
  / ____/   __ \ \
 / /    \__/   / / VORTEX
/ /  \__/  \__/ /  Rotational Data Cache
\ \     \__/  \ \  Copyright (C) 2019, Tom Oleson, All Rights Reserved.
 \ \____/  \   \ \ Made in the U.S.A.
  \____ \  /   / /
       \ \____/ /
        \______/
</pre>

# Rotational Data Cache

Requires libcm_64.so from project common

Features:
1. Non-blocking I/O
2. Service multiple client connections on TCP/IP
3. Event driven (epoll)
4. Rotational journaling data log
5. Journals loaded on restart
6. Uses Linux system resources/libraries; no third-party libraries
7. Event loop in its own thread
8. Single thread listener and thread-pool dispatcher
9. Written in C++ to be Fast!


<pre>
operations:

+key-token{SP}value-token         (create/update)

$key-token                        (read)

!key-token                        (read then delete)

-key-token                        (delete)

*key-token{SP}#tag-token [+key2]  (watch: receive change notifications and optionally copy data to second key)

@key-token{SP}#tag-token [+key2]  (watch: delete after change notification and optionally copy data to second key)

examples:

+key "string"
+key 'string'
+key number-digits
+key { object-fields }
+key [ list-fields ]
+key ( list-fields )
+"key" "string"
+'key' 'string'

</pre>
