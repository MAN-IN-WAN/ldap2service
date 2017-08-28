#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ldap.h>
#include <libconfig.h>
#include <time.h>
#include <syslog.h>
#include "base64.h"

int configFile();
int configValue(char *name, char type, void *value);
int settingValue(config_setting_t *setting, char *name, char type, void *value);
int dispatchClasses();
int classApache();
int classAccount();
int classFTP();
int classDNS();
char *replaceAll(char *content, char *target, char *value);
int settingApache();
char tmplog[1024];

config_t config;
char *ldap_host;
int ldap_port;
char *ldap_bind;
char *ldap_pwd;
char *ldap_base;
char *ldap_filter;
config_setting_t *configApache;
config_setting_t *configAccount;
config_setting_t *configFTP;
config_setting_t *configDNS;
char* tagApache;
char* tagAccount;
char* tagFTP;
char* tagDNS;
char lastTimeStamp[16];
char newTimeStamp[16];
char *attrNames[100];
char **attrValues[100];
int attrCount = 0;
// apache
char *apFileContent, *apFileContentSsl, *apFileObject, *apFolder, *apRestart, *apCommand;
struct apReplace {
	char *target;
	char *value;
	char *prefix;
	char *separator;
	char *defaultValue;
};
struct apReplace apReplacements[20];
int apReplaceCount = 0;
int apUpdated = 0;
// account
char *acFileContent, *acFileObject, *acFolder, *acRestart, *acFileName;
char *acCommand;
struct apReplace acReplacements[20];
int acReplaceCount = 0;
// dns
char *dnRestart;
int dnUpdated = 0;


// write syslog
void logmsg(char *msg) {
  char *buf[1024];
  sprintf(buf, "ldap2service : %s", msg);
  syslog(LOG_INFO, buf);
}

// read configuration file
int configFile() {
  int ok = 1;

  config_init(&config);
  if(! config_read_file(&config, "/etc/ldap2service/ldap2service.conf")) { //"/home/paul/wks/ldap2service/Debug/ldap2service.cfg"
    sprintf(tmplog, "Configuration file error line:%d - %s", config_error_line(&config), config_error_text(&config));
    logmsg(tmplog);
    return 0;
  }

  ok &= configValue("ldap.host", 's', &ldap_host);
  ok &= configValue("ldap.port", 'i', &ldap_port);
  ok &= configValue("ldap.bind", 's', &ldap_bind);
  ok &= configValue("ldap.password", 's', &ldap_pwd);
  ok &= configValue("ldap.base", 's', &ldap_base);
  ok &= configValue("ldap.filter", 's', &ldap_filter);
  ok &= configValue("classes.apache", 'v', &configApache);
  ok &= configValue("classes.account", 'v', &configAccount);
  ok &= configValue("classes.ftp", 'v', &configFTP);
  ok &= configValue("classes.dns", 'v', &configDNS);

  if(ok) {    ok &= settingValue(configApache, "objectClass", 's', &tagApache);
    ok &= settingValue(configAccount, "objectClass", 's', &tagAccount);
    ok &= settingValue(configFTP, "objectClass", 's', &tagFTP);
    ok &= settingValue(configDNS, "objectClass", 's', &tagDNS);

    settingApache();
    settingAccount();
    settingDNS();
  }

//printf("===========\n%s %d %s %s %s %s\n",ldap_host, ldap_port, ldap_bind, ldap_pwd, ldap_base, ldap_filter);
  return ok;
}

// read configuration value
int configValue(char *name, char type, void *value) {
  int ok = 1;
  config_setting_t *v;

  switch(type) {
    case 's': ok = config_lookup_string(&config, name, value); break;
    case 'i': ok = config_lookup_int(&config, name, value); break;
    case 'd': ok = config_lookup_int64(&config, name, value); break;
    case 'f': ok = config_lookup_float(&config, name, value); break;
    case 'v':
    	v = config_lookup(&config, name);
		ok = (v != NULL);
    	*((config_setting_t **)value) = v;
    	break;
  }
  if(! ok) {
    sprintf(tmplog, "'%s' missing in configuration file.", name);
    logmsg(tmplog);
  }

  return ok;
}


// read setting value
int settingValue(config_setting_t *setting, char *name, char type, void *value) {
  int ok = 1;
  config_setting_t *v;

  switch(type) {
    case 's': ok = config_setting_lookup_string(setting, name, value); break;
    case 'i': ok = config_setting_lookup_int(setting, name, value); break;
    case 'd': ok = config_setting_lookup_int64(setting, name, value); break;
    case 'f': ok = config_setting_lookup_float(setting, name, value); break;
    case 'v':
      v = config_setting_get_member(setting, name);
      ok = v != NULL;
  	  *((config_setting_t **)value) = v;
      break;
  }
  if(! ok) {
    sprintf(tmplog, "'%s' setting missing in configuration file.", name);
    logmsg(tmplog);
  }

  return ok;
}


// read last time stamp
int getLastTimeStamp() {
  FILE *f = fopen("/etc/ldap2service/ldap2service.time", "r");
  if(! f) {
    logmsg("Error opening 'ladp2config.time' file.");
    return 0;
  }
  fgets(lastTimeStamp, 15, f);
  lastTimeStamp[14] = '\0';
logmsg("++++++++++++++++lastTimeStamp");
logmsg(lastTimeStamp);
  strcpy(newTimeStamp, lastTimeStamp);
  fclose(f);
  return 1;
}

int saveNewTimeStamp() {
	FILE *f = fopen("/etc/ldap2service/ldap2service.time", "w");
	if(! f) return 0;

	fputs(newTimeStamp, f);
	fclose(f);
  sprintf(tmplog, "New timestamp %s", newTimeStamp);
	return 1;
}


/*
int getNewTimeStamp() {
	time_t mytime;
	struct tm *mytm;
	mytime=time(NULL)-3600 - 60;
	mytm=localtime(&mytime);
	strftime(newTimeStamp, 16, "%Y%m%d%H%M%S", mytm);
	printf("%s\n",newTimeStamp);
}
*/


int dispatchClasses() {
  int i, j, ok = 0;

  for(i=0; i < attrCount; i++) {
          if(! strcmp(attrNames[i], "modifyTimestamp")) {
            if(strcmp(attrValues[i][0], newTimeStamp) > 0)
              strcpy(newTimeStamp, attrValues[i][0]);
	      newTimeStamp[14] = '\0';
          }
	  else if(! strcmp(attrNames[i], "objectClass")) {

		  for(j=0; attrValues[i][j] != NULL; j++) {
				if(! strcasecmp(attrValues[i][j], tagApache)) { ok = classApache(); break; }
				else if(! strcasecmp(attrValues[i][j], tagAccount)) { ok = classAccount(); break; }
				else if(! strcasecmp(attrValues[i][j], tagFTP)) { ok = classFTP(); break; }
				else if(! strcasecmp(attrValues[i][j], tagDNS)) { ok = classDNS(); break; }
		  }
	  }
  }
  return ok;
}

int settingApache() {
	config_setting_t *replacements, *replacement;
	int count, i;

	settingValue(configApache, "folder", 's', &apFolder);
	settingValue(configApache, "fileObject", 's', &apFileObject);
	settingValue(configApache, "fileContent", 's', &apFileContent);
	settingValue(configApache, "fileContentSsl", 's', &apFileContentSsl);
	settingValue(configApache, "restart", 's', &apRestart);
	settingValue(configApache, "replacements", 'v', &replacements);
	settingValue(configApache, "command", 's', &apCommand);
	count = apReplaceCount = config_setting_length(replacements);
	for(i = 0; i < count; i++) {
		replacement = config_setting_get_elem(replacements, i);
		settingValue(replacement, "target", 's', &apReplacements[i].target);
		settingValue(replacement, "value", 's', &apReplacements[i].value);
		settingValue(replacement, "prefix", 's', &apReplacements[i].prefix);
		settingValue(replacement, "separator", 's', &apReplacements[i].separator);
		settingValue(replacement, "defaultValue", 's', &acReplacements[i].defaultValue);
	}
}

int settingAccount() {
	config_setting_t *replacements, *replacement;
	int count, i;

	settingValue(configAccount, "folder", 's', &acFolder);
	settingValue(configAccount, "fileName", 's', &acFileName);
	settingValue(configAccount, "fileContent", 's', &acFileContent);
	settingValue(configAccount, "restart", 's', &apRestart);
	settingValue(configAccount, "command", 's', &acCommand);
	settingValue(configAccount, "replacements", 'v', &replacements);
	count = acReplaceCount = config_setting_length(replacements);
	for(i = 0; i < count; i++) {
		replacement = config_setting_get_elem(replacements, i);
		settingValue(replacement, "target", 's', &acReplacements[i].target);
		settingValue(replacement, "value", 's', &acReplacements[i].value);
		settingValue(replacement, "prefix", 's', &acReplacements[i].prefix);
		settingValue(replacement, "separator", 's', &acReplacements[i].separator);
		settingValue(replacement, "defaultValue", 's', &acReplacements[i].defaultValue);
	}
}

int settingDNS() {
	settingValue(configDNS, "restart", 's', &dnRestart);
}



int classApache() {
  char *fileData, *fileTemp, *fileDataSsl, *fileTempSsl;
  int i, j, k, active, sslactive, passwordprotected;
  char fileName[256];
  char fileNameSsl[256];
  char fileNameFullChain[256];
  char fileNamePrivkey[256];
  char *fileDataFullChain;
  char *fileDataPrivkey;
  char *tmp;
  char *tmps;
  char *command, *tmpCommand;
  char val[8096];
  FILE *f;

  // chemin et nom du fichier du site
  fileName[0] = '\0';
  for(i = 0; i < attrCount; i++) {
	  //printf("attr %s => %s\n",attrNames[i],attrValues[i][0]);
	  
          //nom du fichier
	  if(! strcmp(apFileObject, attrNames[i])) {
		 strcat(strcpy(fileName, apFolder), attrValues[i][0]);
  		 strcat(strcpy(fileNameSsl, fileName), ".ssl");
		 strcat(strcat(strcpy(fileNameFullChain, "/etc/ldap2service/"), attrValues[i][0]),"-fullchain.pem");
		 strcat(strcat(strcpy(fileNamePrivkey, "/etc/ldap2service/"), attrValues[i][0]),"-privkey.pem");
	  }

	  //config active
	  if (! strcmp("apacheVhostEnabled",attrNames[i])) {
		if (! strcmp("yes", attrValues[i][0])){
			active = 1;
		}else{
			active=0;
		}
	  }
	
	 // ssl active
	  if (! strcmp("apacheSslEnabled",attrNames[i])) {
		if (! strcmp("yes", attrValues[i][0])){
			sslactive = 1;
		}else{
			sslactive = 0;
		}
	  }

	  // full chain
	  if(! strcmp("apacheCertificate", attrNames[i])) {
		fileDataFullChain = malloc(strlen(attrValues[i][0])+1);
		strcpy(fileDataFullChain,attrValues[i][0]);
		fileDataFullChain[strlen(attrValues[i][0])] = '\0';
	  }

	  // priv key
	  if(! strcmp("apacheCertificateKey", attrNames[i])) {
		fileDataPrivkey = malloc(strlen(attrValues[i][0])+1);
		strcpy(fileDataPrivkey,attrValues[i][0]);
		fileDataPrivkey[strlen(attrValues[i][0])] = '\0';
	  }
	  
	  // password protected
	  if(! strcmp("apacheHtPasswordEnabled", attrNames[i])) {
		if (! strcmp("yes", attrValues[i][0])){
			passwordprotected = 1;
		}
	  }
  }

  // desactivation
  if (!active){
	apUpdated = 1;
	//suppression des fichiers
	remove(fileName);
	remove(fileNameSsl);
	return 1;
  }

  fileData = malloc(32000);
  strcpy(fileData, apFileContent);
  for(i = 0; i < apReplaceCount; i++) {
	  for(j = 0; j < attrCount; j++) {
		  if(! strcmp(attrNames[j], apReplacements[i].value)) {
			  strcpy(val, apReplacements[i].prefix);
			  for(k=0; attrValues[j][k] != NULL; k++) {
				  if(k > 0) strcat(val, apReplacements[i].separator);
				  strcat(val, attrValues[j][k]);
				  if(! apReplacements[i].separator[0]) break;
			  }
			  fileTemp = fileData;
			  fileData = replaceAll(fileTemp, apReplacements[i].target, val);
			  free(fileTemp);
			  break;
		  }
	  }
  }

  for(i = 0; i < apReplaceCount; i++) {
	  fileTemp = fileData;
	  fileData = replaceAll(fileTemp, apReplacements[i].target, "");
	  free(fileTemp);
  }

  if(*fileName) {
    sprintf(tmplog, "Apache file : %s", fileName);
    logmsg(tmplog);
	  f = fopen(fileName, "w");
	  if(f) {
		  fputs(fileData, f);
		  fclose(f);
		  apUpdated = 1;
	  }
  }

  free(fileData);

  // si proteger par mot de passe
  if (passwordprotected){
	apUpdated = 1;
  	command = malloc(4096);
	  strcpy(command, apCommand);
	  for(i = 0; i < apReplaceCount; i++) {
	          for(j = 0; j < attrCount; j++) {
	                  if(! strcmp(attrNames[j], apReplacements[i].value)) {
	                          *val = '\0';
				  for(k=0; attrValues[j][k] != NULL; k++) {
	                                  if(k > 0) strcat(val, apReplacements[i].separator);
	                                  strcat(val, attrValues[j][k]);
        	                          if(! apReplacements[i].separator[0]) break;
	                          }
	
        	                  tmpCommand = command;
	                          command = replaceAll(tmpCommand, apReplacements[i].target, val);
	                          free(tmpCommand);
	                          break;
        	          }
	          }
	  }

  	//execution de la commande
	sprintf(tmplog, "Apache command : %s", command);
	logmsg(tmplog);
	system(command);
  }

  if (sslactive) {
	apUpdated = 1;
	printf("creation config ssl %s\n",fileNameSsl);
	  //creation du fichier ssl
	  fileDataSsl = malloc(32000);
	  strcpy(fileDataSsl, apFileContentSsl);
	  for(i = 0; i < apReplaceCount; i++) {
	          for(j = 0; j < attrCount; j++) {
	                  if(! strcmp(attrNames[j], apReplacements[i].value)) {
	                          strcpy(val, apReplacements[i].prefix);
	                          for(k=0; attrValues[j][k] != NULL; k++) {
	                                  if(k > 0) strcat(val, apReplacements[i].separator);
	                                  strcat(val, attrValues[j][k]);
	                                  if(! apReplacements[i].separator[0]) break;
	                          }
	                          fileTempSsl = fileDataSsl;
	                          fileDataSsl = replaceAll(fileTempSsl, apReplacements[i].target, val);
	                          free(fileTempSsl);
	                          break;
        	          }
	          }
	  }

	  for(i = 0; i < apReplaceCount; i++) {
	          fileTempSsl = fileDataSsl;
	          fileDataSsl = replaceAll(fileTempSsl, apReplacements[i].target, "");
	          free(fileTempSsl);
	  }
	  
	 //ecriture du fichier apache ssl
	 if(*fileNameSsl) {
	    sprintf(tmplog, "Apache file ssl : %s", fileNameSsl);
	    logmsg(tmplog);
	          f = fopen(fileNameSsl, "w");
	          if(f) {
	                  fputs(fileDataSsl, f);
	                  fclose(f);
	                  apUpdated = 1;
        	  }
	  }

	 //creation du fichier fullchain
	 f = fopen(fileNameFullChain, "w");
	 if(f) {
		 int fclen;
		 tmp  = unbase64(fileDataFullChain,(int)strlen(fileDataFullChain),&fclen);
		 char *tmp2 = (char *) malloc(fclen+1);
		 strncpy(tmp2,tmp,fclen);
		 free(tmp);
		 tmp2[fclen]='\0';
		 //printf("creation fullchain ssl %s \n",tmp2);
                 fputs(tmp2, f);
                 fclose(f);
		 free(tmp2);
         }
	 
	 //creation du fichier privkey
         f = fopen(fileNamePrivkey, "w");
         if(f) {
		 int pklen;
		 tmps = unbase64(fileDataPrivkey,(int)strlen(fileDataPrivkey),&pklen);
		 char * tmps2 = (char *) malloc(pklen+1);
		 strncpy(tmps2,tmps,pklen);
		 free(tmps);
		 tmps2[pklen]='\0';
		 //printf("creation privkey ssl %s \n",tmps2);
                 fputs(tmps2, f);
                 fclose(f);
		 free(tmps2);
         }
  }else{
	printf("suppression config ssl %s\n",fileNameSsl);
	 remove(fileNameSsl);
  }
	

  return 1;
}

int classAccount() {
  char *fileData, *fileTemp, *fileNemp, *fileName;
  int i, j, k;
  char *command, *tmpCommand;
  char val[1024];
  FILE *f;

  // ******************************
  // Execution commande
  // ******************************
  command = malloc(4096);
  strcpy(command, acCommand);
  for(i = 0; i < acReplaceCount; i++) {
	  for(j = 0; j < attrCount; j++) {
		  if(! strcmp(attrNames[j], acReplacements[i].value)) {
                          *val = '\0';
			  if(attrValues[j][0] != NULL) {
				  strcat(val, attrValues[j][0]);
			  }
			  tmpCommand = command;
			  command = replaceAll(tmpCommand, acReplacements[i].target, val);
			  free(tmpCommand);
			  break;
		  }
	  }
  }
  sprintf(tmplog, "Account command : %s", command);
  logmsg(tmplog);
  system(command);
  free(command);


  // ******************************
  // Fichier de configuration
  // ******************************
  fileData = malloc(32000);
  fileName = malloc(32000);
  strcpy(fileData, acFileContent);
  strcat(strcpy(fileName, acFolder), acFileName);
  // chemin et nom du fichier du site (remplacement des chaines)
  for(i = 0; i < acReplaceCount; i++) {
          for(j = 0; j < attrCount; j++) {
                  if(! strcmp(attrNames[j], acReplacements[i].value)) {
                          strcpy(val, acReplacements[i].prefix);
                          for(k=0; attrValues[j][k] != NULL; k++) {
                                  if(k > 0) strcat(val, acReplacements[i].separator);
                                  strcat(val, attrValues[j][k]);
                                  if(! acReplacements[i].separator[0]) break;
                          }
			  fileTemp = fileData;
                          fileData = replaceAll(fileTemp, acReplacements[i].target, val);
                          fileNemp = fileName;
                          fileName = replaceAll(fileNemp, acReplacements[i].target, val);
                          free(fileTemp);
                          free(fileNemp);
                          break;
                  }
          }
  }
  for(i = 0; i < acReplaceCount; i++) {

          fileTemp = fileData;
          fileData = replaceAll(fileTemp, acReplacements[i].target, acReplacements[i].defaultValue);
          fileNemp = fileName;
          fileName = replaceAll(fileNemp, acReplacements[i].target, acReplacements[i].defaultValue);
	  //free(fileTemp);
	  //free(fileNemp);
  }
  //Sortie log et ecriture du fichier
  if(*fileName) {
    sprintf(tmplog, "Account file : %s", fileName);
    logmsg(tmplog);
          f = fopen(fileName, "w");
          if(f) {
                  fputs(fileData, f);
                  fclose(f);
                  apUpdated = 1;
          }
  }
  free(fileData);
  return 1;
}

char *replaceAll(char *content, char *target, char *value) {
  char *pos, *last, *buffer;
  int lent, lenv, lenb, count = 0, start;

  lent = strlen(target);
  lenv = strlen(value);

  // buffer length
  pos = content;
  while((pos = strstr(pos, target)) != NULL) {
	  count++;
	  pos += lent;
  }
  lenb = strlen(content) + lenv * count - lent * count;
  buffer = malloc(lenb + 1);

  // replace
  start = 0;
  last = content;
  while((pos = strstr(last, target)) != NULL) {
	  count = pos - last;
	  strncpy(buffer + start, last, count);
	  start += count;
	  strcpy(buffer + start, value);
	  start += lenv;
	  last = pos + lent;
  }
  strcpy(buffer + start, last);
  return buffer;
}


int classFTP() {
  return 1;
}

int classDNS() {
  dnUpdated = 1;
  return 1;
}


int addAttribute(char *name, char **values) {
	attrNames[attrCount] = malloc(strlen(name)+1);
	strcpy(attrNames[attrCount], name);
	attrValues[attrCount] = values;
	attrCount++;
	return 1;
}

int clearAttributes() {
	int i;
	for(i = 0; i < attrCount; i++) {
		free(attrNames[i]);
	}
	attrCount = 0;
}



int main( int argc, char *argv[] ) {
  int version = LDAP_VERSION3, rc, i;
  LDAP *ld;
  LDAPMessage *searchResult, *entry;
  char *attribute, **values;
  BerElement *ber;
  struct timeval timeOut = {10,0}; // 10 second connecion/search timeout
  char filter[1024];
  char* returnAttr[] = {"*","modifyTimestamp",NULL};

  // open syslog
  openlog("slog", LOG_PID|LOG_CONS, LOG_USER);
  logmsg("Start");

  // read config file
  if(! configFile()) { config_destroy(&config); return 1; }
  if(! getLastTimeStamp()) { config_destroy(&config); return 1; }

  logmsg("Connexion");
  ldap_set_option( NULL, LDAP_OPT_PROTOCOL_VERSION, &version );
  ld = (LDAP *)ldap_init(ldap_host, ldap_port);
  if(ld == NULL ) {
    logmsg("*** Connexion error ***");
    config_destroy(&config); return 1;
  }

  logmsg("Bind");
  rc = ldap_simple_bind_s(ld, ldap_bind, ldap_pwd);
  if(rc != LDAP_SUCCESS) {
    logmsg("*** Bind error ***");
    ldap_unbind(ld);
    config_destroy(&config); return 1;
  }

  sprintf(filter, ldap_filter, lastTimeStamp, lastTimeStamp);
  sprintf(tmplog, "Search : %s", filter);
  logmsg(tmplog);
  rc = ldap_search_ext_s(
            ld,		// LDAP session handle
            ldap_base,	// Search Base
            LDAP_SCOPE_SUBTREE,	// Search Scope – everything below o=Acme
            filter, 	// Search Filter – only inetOrgPerson objects
            returnAttr,	// returnAllAttributes – NULL means Yes
            0,		// attributesOnly – False means we want values
            NULL,	// Server controls – There are none
            NULL,	// Client controls – There are none
            &timeOut,	// search Timeout
            LDAP_NO_LIMIT,	// no size limit
            &searchResult);
  if(rc != LDAP_SUCCESS) {
    logmsg("*** Search error ***");
    ldap_unbind(ld);
    config_destroy(&config); return 1;
  }

  //getNewTimeStamp();
  strcpy(newTimeStamp, lastTimeStamp);
  newTimeStamp[14] = '\0';

  sprintf(tmplog, "Entries:%d", ldap_count_entries(ld, searchResult));
  logmsg(tmplog);

  for(entry = ldap_first_entry(ld, searchResult); entry != NULL; entry = ldap_next_entry(ld, entry)) {
    //printf("-------------\n");
    for(attribute = ldap_first_attribute(ld, entry, &ber); attribute != NULL; attribute = ldap_next_attribute(ld, entry, ber)) {
    	values = (char **)ldap_get_values(ld, entry, attribute);
    	addAttribute(attribute, values);
    	ldap_memfree( attribute );
    }
	dispatchClasses();
	clearAttributes();
    ber_free( ber, 0 );
  }
  ldap_msgfree(searchResult);

  ldap_unbind(ld);

  if(apUpdated != 0) {
    logmsg(apRestart);
    system(apRestart);
  }
  if(dnUpdated) {
    logmsg(dnRestart);
    system(dnRestart);
  }
  saveNewTimeStamp();
  logmsg(tmplog);

  config_destroy(&config);

  // close syslog
  logmsg("End");
  closelog();

  return( 0 );
}




