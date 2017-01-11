/*
 ============================================================================
 Name        : openssl.c
 Author      : junma
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>

#undef PROG
#define PROG    verify_main

static int MS_CALLBACK cb(int ok, X509_STORE_CTX *ctx);
static int check(X509_STORE *ctx, char *file,
        STACK_OF(X509) *uchain, STACK_OF(X509) *tchain,
        STACK_OF(X509_CRL) *crls, ENGINE *e);
static int v_verbose=0, vflags = 0;

int MAIN(int, char **);

int CAfilecheck(int argc, char **argv)
{
    ENGINE *e = NULL;
    int i,ret=1, badarg = 0;
    char *CApath=NULL,*CAfile=NULL;
    char *untfile = NULL, *trustfile = NULL, *crlfile = NULL;
    STACK_OF(X509) *untrusted = NULL, *trusted = NULL;
    STACK_OF(X509_CRL) *crls = NULL;
    X509_STORE *cert_ctx=NULL;
    X509_LOOKUP *lookup=NULL;
    X509_VERIFY_PARAM *vpm = NULL;
#ifndef OPENSSL_NO_ENGINE
    char *engine=NULL;
#endif

    cert_ctx=X509_STORE_new();
    if (cert_ctx == NULL) goto end;
    X509_STORE_set_verify_cb(cert_ctx,cb);

    ERR_load_crypto_strings();

    apps_startup();

    if (bio_err == NULL)
        if ((bio_err=BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

    if (!load_config(bio_err, NULL))
        goto end;
    printf("argv is ****%s\n",*argv);

    argc--;
    argv++;
    for (;;)
    {
        if (argc >= 1)
        {
            printf("argv is ****%s\n",*argv);
            if (strcmp(*argv,"-CApath") == 0)
            {
            if (argc-- < 1) goto end;
            CApath= *(++argv);
            printf("CApath is ****%s\n",CApath);
            }
            else if (strcmp(*argv,"-CAfile") == 0)
            {
            if (argc-- < 1) goto end;
            CAfile= *(++argv);
            printf("CAfile is ****%s\n",CAfile);
            }
            else if (args_verify(&argv, &argc, &badarg, bio_err,
                                &vpm))
            {
            if (badarg)
                goto end;
            continue;
            }
            else if (strcmp(*argv,"-untrusted") == 0)
            {
                if (argc-- < 1) goto end;
                untfile= *(++argv);
            }
            else if (strcmp(*argv,"-trusted") == 0)
            {
                if (argc-- < 1) goto end;
                trustfile= *(++argv);
            }
            else if (strcmp(*argv,"-CRLfile") == 0)
            {
                if (argc-- < 1) goto end;
                crlfile= *(++argv);
            }
#ifndef OPENSSL_NO_ENGINE
            else if (strcmp(*argv,"-engine") == 0)
            {
                if (--argc < 1) goto end;
                engine= *(++argv);
            }
#endif
            else if (strcmp(*argv,"-help") == 0)
                goto end;
            else if (strcmp(*argv,"-verbose") == 0)
                v_verbose=1;
            else if (argv[0][0] == '-')
                goto end;
            else
                break;
            argc--;
            argv++;
        }
        else
            break;
    }

    printf("****==begin check==******\n");
#ifndef OPENSSL_NO_ENGINE
        e = setup_engine(bio_err, engine, 0);
#endif

    if (vpm)
        X509_STORE_set1_param(cert_ctx, vpm);

    lookup=X509_STORE_add_lookup(cert_ctx,X509_LOOKUP_file());
    if (lookup == NULL) abort();
    if (CAfile) {
        i=X509_LOOKUP_load_file(lookup,CAfile,X509_FILETYPE_PEM);
        if(!i) {
            BIO_printf(bio_err, "Error loading file %s\n", CAfile);
            ERR_print_errors(bio_err);
            goto end;
        }
    } else X509_LOOKUP_load_file(lookup,NULL,X509_FILETYPE_DEFAULT);

    lookup=X509_STORE_add_lookup(cert_ctx,X509_LOOKUP_hash_dir());
    if (lookup == NULL) abort();
    if (CApath) {
        i=X509_LOOKUP_add_dir(lookup,CApath,X509_FILETYPE_PEM);
        if(!i) {
            BIO_printf(bio_err, "Error loading directory %s\n", CApath);
            ERR_print_errors(bio_err);
            goto end;
        }
    } else X509_LOOKUP_add_dir(lookup,NULL,X509_FILETYPE_DEFAULT);

    ERR_clear_error();

    if(untfile)
        {
        untrusted = load_certs(bio_err, untfile, FORMAT_PEM,
                    NULL, e, "untrusted certificates");
        if(!untrusted)
            goto end;
        }

    if(trustfile)
        {
        trusted = load_certs(bio_err, trustfile, FORMAT_PEM,
                    NULL, e, "trusted certificates");
        if(!trusted)
            goto end;
        }

    if(crlfile)
        {
        crls = load_crls(bio_err, crlfile, FORMAT_PEM,
                    NULL, e, "other CRLs");
        if(!crls)
            goto end;
        }

    ret = 0;
    if (argc < 1)
    {
        if (1 != check(cert_ctx, NULL, untrusted, trusted, crls, e))
            ret = -1;
    }
    else
    {
        for (i=0; i<argc; i++)
            if (1 != check(cert_ctx,argv[i], untrusted, trusted, crls, e))
                ret = -1;
    }

end:
    if (ret == 1) {
        BIO_printf(bio_err,"usage: verify [-verbose] [-CApath path] [-CAfile file] [-purpose purpose] [-crl_check]");
        BIO_printf(bio_err," [-attime timestamp]");
#ifndef OPENSSL_NO_ENGINE
        BIO_printf(bio_err," [-engine e]");
#endif
        BIO_printf(bio_err," cert1 cert2 ...\n");

        BIO_printf(bio_err,"recognized usages:\n");
        for(i = 0; i < X509_PURPOSE_get_count(); i++)
            {
            X509_PURPOSE *ptmp;
            ptmp = X509_PURPOSE_get0(i);
            BIO_printf(bio_err, "\t%-10s\t%s\n",
                   X509_PURPOSE_get0_sname(ptmp),
                   X509_PURPOSE_get0_name(ptmp));
            }
    }
    if (vpm) X509_VERIFY_PARAM_free(vpm);
    if (cert_ctx != NULL) X509_STORE_free(cert_ctx);
    sk_X509_pop_free(untrusted, X509_free);
    sk_X509_pop_free(trusted, X509_free);
    sk_X509_CRL_pop_free(crls, X509_CRL_free);
    apps_shutdown();
    OPENSSL_EXIT(ret < 0 ? 2 : ret);
}

static int check(X509_STORE *ctx, char *file,
        STACK_OF(X509) *uchain, STACK_OF(X509) *tchain,
        STACK_OF(X509_CRL) *crls, ENGINE *e)
    {
    X509 *x=NULL;
    int i=0,ret=0;
    X509_STORE_CTX *csc;

    x = load_cert(bio_err, file, FORMAT_PEM, NULL, e, "certificate file");
    if (x == NULL)
        goto end;
    fprintf(stdout,"%s: ",(file == NULL)?"stdin":file);

    csc = X509_STORE_CTX_new();
    if (csc == NULL)
        {
        ERR_print_errors(bio_err);
        goto end;
        }
    X509_STORE_set_flags(ctx, vflags);
    if(!X509_STORE_CTX_init(csc,ctx,x,uchain))
        {
        ERR_print_errors(bio_err);
        goto end;
        }
    if(tchain) X509_STORE_CTX_trusted_stack(csc, tchain);
    if (crls)
        X509_STORE_CTX_set0_crls(csc, crls);
    i=X509_verify_cert(csc);
    X509_STORE_CTX_free(csc);

    ret=0;
end:
    if (i > 0)
        {
        fprintf(stdout,"OK\n");
        ret=1;
        }
    else
        ERR_print_errors(bio_err);
    if (x != NULL) X509_free(x);

    return(ret);
    }

static int MS_CALLBACK cb(int ok, X509_STORE_CTX *ctx)
    {
    int cert_error = X509_STORE_CTX_get_error(ctx);
    X509 *current_cert = X509_STORE_CTX_get_current_cert(ctx);

    if (!ok)
        {
        if (current_cert)
            {
            X509_NAME_print_ex_fp(stdout,
                X509_get_subject_name(current_cert),
                0, XN_FLAG_ONELINE);
            printf("\n");
            }
        printf("%serror %d at %d depth lookup:%s\n",
            X509_STORE_CTX_get0_parent_ctx(ctx) ? "[CRL path]" : "",
            cert_error,
            X509_STORE_CTX_get_error_depth(ctx),
            X509_verify_cert_error_string(cert_error));
        switch(cert_error)
            {
            case X509_V_ERR_NO_EXPLICIT_POLICY:
                policies_print(NULL, ctx);
            case X509_V_ERR_CERT_HAS_EXPIRED:

            /* since we are just checking the certificates, it is
             * ok if they are self signed. But we should still warn
             * the user.
             */

            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
            /* Continue after extension errors too */
            case X509_V_ERR_INVALID_CA:
            case X509_V_ERR_INVALID_NON_CA:
            case X509_V_ERR_PATH_LENGTH_EXCEEDED:
            case X509_V_ERR_INVALID_PURPOSE:
            case X509_V_ERR_CRL_HAS_EXPIRED:
            case X509_V_ERR_CRL_NOT_YET_VALID:
            case X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION:
            ok = 1;

            }

        return ok;

        }
    if (cert_error == X509_V_OK && ok == 2)
        policies_print(NULL, ctx);
    if (!v_verbose)
        ERR_clear_error();
    return(ok);
    }
int MAIN(int argc,char **argv)
{
    int ret;
    ret = CAfilecheck(argc, argv);
    printf("%d\n");
}
