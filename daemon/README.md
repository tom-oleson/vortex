<pre>
         ______
        / ____ \
   ____/ /    \ \
  / ____/   x  \ \
 / /     __/   / / VORTEXD
/ /  x__/  \  / /  Vortex service
\ \     \__/  \ \  Copyright (C) 2019, Tom Oleson, All Rights Reserved.
 \ \____   \   \ \ Made in the U.S.A.
  \____ \   x  / /
       \ \____/ /
        \______/
</pre>


# vortexd
Runs vortex as a daemom


To install, execute:
<pre>
sudo make -f linux.mk
</pre>

This will install vortexd to /etc/init.d and copy vortex into /opt/vortex

NOTE: The make file assumes that vortex is up one directory and has already
been built using its own make file.


<pre>
sudo /etc/init.d/vortexd start
sudo /etc/init.d/vortexd restart
sudo /etc/init.d/vortexd stop
sudo /etc/init.d/vortexd force-reload
</pre>

NOTE:
Configure vortex command line options in vortexd script.

Default build has:
-l8     (log level: trace)
