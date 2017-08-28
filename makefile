#VARIABLES
BIN_DIR=/usr/bin
CONFIG_DIR=/etc/ldap2service
OPENLDAP_DIR=/etc/openldap/schema

all: ldap2service

ldap2service: ldap2service.c ldap2service
	gcc -O2 ldap2service.c -lldap -llber -lconfig -o ldap2service

install: all
	mkdir -p $(BIN_DIR)
	mkdir -p $(CONFIG_DIR)
	install -m 755 ldap2service.conf $(CONFIG_DIR)
	install -m 755 ldap2service $(BIN_DIR)
	install -m 755 ldap2service.time $(CONFIG_DIR)
	install -m 755 mod_vhost_ldap_ng.schema $(OPENLDAP_DIR)
	install -m 755 pureftpd.schema $(OPENLDAP_DIR)
	install -m 755 ldap2dns.schema $(OPENLDAP_DIR)
	ln -s /usr /usr/local/php-default
	mkdir /etc/php-default
	ln -s /etc/php.ini /etc/php-default/php.ini

update:
	install -m 755 ldap2service.conf $(CONFIG_DIR)
	install -m 755 ldap2service $(BIN_DIR)
	install -m 755 ldap2service.time $(CONFIG_DIR)
	install -m 755 mod_vhost_ldap_ng.schema $(OPENLDAP_DIR)
	install -m 755 pureftpd.schema $(OPENLDAP_DIR)
	install -m 755 ldap2dns.schema $(OPENLDAP_DIR)


clean:
	rm -f ldap2service

