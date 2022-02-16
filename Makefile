
all: pam_tty.so

clean:
	rm -f pam_tty.so pam_tty.o

pam_tty.o: pam_tty.c
	gcc -O2 -Wall -fPIC -c $< -o $@

pam_tty.so: pam_tty.o
	gcc -shared -o $@ $< -lpam

