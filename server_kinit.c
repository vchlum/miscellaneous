// gcc server_kinit.c -o server_kinit -lkrb5 -g

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <krb5.h>
#include <time.h>


#define PBS_SERVICE_NAME "pbs"
#define KRB5CCNAME "FILE:/tmp/krb5cc_pbs_server"

int init_server_ccache(){
		krb5_error_code ret;
		krb5_context context;
		krb5_principal pbs_service = NULL;
		krb5_keytab keytab = NULL;
		krb5_creds *creds = NULL;
		krb5_get_init_creds_opt *opt = NULL;
		krb5_ccache ccache = NULL;
		krb5_creds *mcreds = NULL;
		
		int endtime = 0;

		creds = malloc(sizeof(krb5_creds));
		memset(creds,0,sizeof(krb5_creds));
		mcreds = malloc(sizeof(krb5_creds));
		memset(mcreds,0,sizeof(krb5_creds));
	
		setenv("KRB5CCNAME",KRB5CCNAME,1);
	
		ret = krb5_init_context(&context);
		if (ret) {
				fprintf(stderr, "Cannot initialize Kerberos, exiting.\n");
				goto out;
		}
	
		ret = krb5_sname_to_principal(context, NULL, PBS_SERVICE_NAME, KRB5_NT_SRV_HST, &pbs_service);
		if (ret) {
				fprintf(stderr, "Preparing principal failed (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}
		

		ret = krb5_cc_resolve(context, KRB5CCNAME, &ccache);
		if (ret) {
				fprintf(stderr, "Couldn't resolve ccache name (%s) Will create new one\n", krb5_get_error_message(context, ret));
				// not an error, we will create new ccache
		}

		char *realm;
		char **realms;
		char hostname[1024];
		gethostname(hostname, 1024);
		ret = krb5_get_host_realm(context, hostname, &realms);
		if (ret) {
				fprintf(stderr, "Failed to get host realms (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}
		
		realm = realms[0];
		ret = krb5_build_principal(context, &mcreds->server, strlen(realm), realm, KRB5_TGS_NAME, realm, NULL);
		if (ret) {
				fprintf(stderr, "Couldn't build server principal (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}
				
		ret = krb5_copy_principal(context, pbs_service, &mcreds->client);
		if (ret) {
				fprintf(stderr, "Couldn't copy client principal (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}

		ret = krb5_cc_retrieve_cred(context, ccache, 0, mcreds, creds);
		if (ret) {
				fprintf(stderr, "Couldn't retrieve credentials from cache (%s) Will create new one\n",	krb5_get_error_message(context, ret));
				// not an error, we will create new ccache
		} else {
				endtime = creds->times.endtime;
		}
		
		if (endtime - 60 >= time(NULL)) {
				ret = 0;
				goto out;
		}

		ret = krb5_cc_new_unique(context, "FILE", NULL, &ccache);
		if (ret) {
				fprintf(stderr, "Failed to create ccache (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}

		ret = krb5_cc_resolve(context, KRB5CCNAME, &ccache);
		if (ret) {
				fprintf(stderr, "Couldn't resolve cache name (%s)\n",	krb5_get_error_message(context, ret));
				goto out;
		}
		
		ret = krb5_kt_default(context, &keytab);
		if (ret) {
				fprintf(stderr, "Couldn't open keytab (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}
		ret = krb5_get_init_creds_opt_alloc(context, &opt);
		if (ret) {
				fprintf(stderr, "Couldn't allocate a new initial credential options structure (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}

		krb5_get_init_creds_opt_set_forwardable(opt, 1);

		ret = krb5_get_init_creds_keytab(context, creds, pbs_service, keytab, 0, NULL, opt);
		if (ret) {
				fprintf(stderr, "Couldn't get initial credentials using a key table (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}

		ret = krb5_cc_initialize(context, ccache, creds->client);
		if (ret) {
				fprintf(stderr, "krb5_cc_initialize() failed (%s)\n",	krb5_get_error_message(context, ret));
				goto out;
		}

		ret = krb5_cc_store_cred(context, ccache, creds);
		if (ret) {
				fprintf(stderr, "Couldn't store ccache (%s)\n", krb5_get_error_message(context, ret));
				goto out;
		}

out:
		krb5_free_creds(context, creds);
		krb5_free_creds(context, mcreds);
		krb5_get_init_creds_opt_free(context, opt);
		krb5_free_principal(context, pbs_service);
		if (keytab)
				krb5_kt_close(context, keytab);

		return (ret);
}

int main(int argc, char *argv[]){
		return init_server_ccache();
}
