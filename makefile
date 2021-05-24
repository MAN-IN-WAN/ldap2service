#VARIABLES
BIN_DIR=/usr/bin
CONFIG_DIR=/etc/ldap2service
OPENLDAP_DIR=/etc/openldap/schema
CCFLAGS=gcc -O2

all: ldap2service

debug: CCFLAGS += -DDEBUG -g
debug: ldap2service

ldap2service:
	$(CCFLAGS) ldap2service.c -lldap -llber -lconfig -o ldap2service

install: all
	@[ ! -d $(BIN_DIR) ] && mkdir -p $(BIN_DIR) || true
	@[ ! -d $(CONFIG_DIR) ] && mkdir -p $(CONFIG_DIR) || true
	install -m 755 ldap2service.conf $(CONFIG_DIR)
	install -m 755 ldap2service $(BIN_DIR)
	install -m 755 ldap2service.time $(CONFIG_DIR)
	install -m 755 mod_vhost_ldap_ng.schema $(OPENLDAP_DIR)
	install -m 755 pureftpd.schema $(OPENLDAP_DIR)
	install -m 755 ldap2dns.schema $(OPENLDAP_DIR)
	@[ ! -L /usr/local/php-default ] && ln -s /usr /usr/local/php-default || true
	@[ ! -d /etc/php-default ] && mkdir /etc/php-default || true
	@[ ! -L /etc/php-default/php.ini ] && ln -s /etc/php.ini /etc/php-default/php.ini || true

update:
	install -m 755 ldap2service $(BIN_DIR)
	install -m 755 mod_vhost_ldap_ng.schema $(OPENLDAP_DIR)
	install -m 755 pureftpd.schema $(OPENLDAP_DIR)
	install -m 755 ldap2dns.schema $(OPENLDAP_DIR)


clean:
	rm -f ldap2service
