# pam_tty.so
_Always a good idea: Replacing code with less code._
### What the module does
**chown(2)** ```/dev/ttyN``` on successful console login as a significantly less complex alternative to **systemd-logind(8)** or **elogind(8)**
### Rationale: Rootless X without complex seat managers
Everyone wants to run the xserver **Xorg(1)** as a normal user, i.e. without the suid bit set on the executable. But not everyone wants to deploy a full-featured seat manager aka login manager like **elogind(8)** or even **systemd-logind(8)** just for this. Fortunately most privileges needed for rootless X can be granted via Unix groups (_video, kvm, input,_ etc). Except for the virtual console TTY devices ```/dev/ttyN```. One way to get around this would be putting the user into group _tty_ and configure the acting udev daemon to use ```rw-rw----``` for the virtual console TTYs, but this could be just too much security dealt for simplicity.

This is where **pam_tty.so** comes into play. **pam_tty.so** changes the ownership of the virtual console TTY device to the user on login, and back to the original value on logout. _Redhat™_ already had something similar (pam_console.so, much more complex than pam_tty.so) until the arrival of **systemd(1)**.
### Build and install pam_tty.so
You will need the usual build environment (compiler, make, etc) and PAM with header files, obviously. Then clone the repo or get and extract the ZIP archive and just hit ```make(1)```. And yes, "βελλεροφων" (=bellerophon) is the hostname of my computer — I did some experiments with IDNs one day and somehow it got stuck then.
```
[bf@βελλεροφων:~/src/pam_tty]$ make
gcc -O2 -Wall -fPIC -c pam_tty.c -o pam_tty.o
gcc -shared -o pam_tty.so pam_tty.o -lpam
[bf@βελλεροφων:~/src/pam_tty]$
```
Copy the ```pam_tty.so``` to the directory with the other PAM modules, e.g. ```/lib64/security/pam_tty.so```
### Configure and activate pam_tty.so
Most systems have a dedicated PAM stack for local logins. Just add it to the end as a **session** module. The only supported option is _debug_, just in case.
```
[bf@βελλεροφων:~/src]$ cat /etc/pam.d/system-local-login 
auth		include		system-login
account		include		system-login
password	include		system-login
session		include		system-login
session		optional	pam_tty.so
[bf@βελλεροφων:~/src]$
```

### Tips for starting rootless Xorg

After logging in at the virtual console, you probably want to start Xorg with
the options ```-keeptty``` and ```vtN``` where **N** is the number of your
virtual console:
```
startx -- -keeptty vt`tty | sed 's/.*tty//'`
```
or, if you do not want Xauthority handled by startx:
```
xinit -- -keeptty vt`tty | sed 's/.*tty//'`
```
