#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tcp_helper.h"

/* A TLS client that loads the certificate and key.
 */
 
#define MAX_BUF 1024
#define MSG "GET / HTTP/1.0\r\n\r\n"
 
#define CERT_FILE "client.pem"
#define KEY_FILE "client-key.pem"
#define CAFILE "cacert.pem"
//#define CAFILE "/etc/ssl/certs/ca-certificates.crt"
 
extern int tcp_connect (void);
extern void tcp_close (int sd);
 
static int
cert_callback (gnutls_session_t session,
const gnutls_datum_t * req_ca_rdn, int nreqs,
const gnutls_pk_algorithm_t * sign_algos,
int sign_algos_length, gnutls_pcert_st ** pcert,
unsigned int *pcert_length, gnutls_privkey_t * pkey);
 
gnutls_pcert_st crt;
gnutls_privkey_t key;
 
/* Helper functions to load a certificate and key
 * files into memory.
 */
static gnutls_datum_t
load_file (const char *file)
{
FILE *f;
gnutls_datum_t loaded_file = { NULL, 0 };
long filelen;
void *ptr;
 
if (!(f = fopen (file, "r"))
|| fseek (f, 0, SEEK_END) != 0
|| (filelen = ftell (f)) < 0
|| fseek (f, 0, SEEK_SET) != 0
|| !(ptr = malloc ((size_t) filelen))
|| fread (ptr, 1, (size_t) filelen, f) < (size_t) filelen)
{
if (f)
fclose (f);
return loaded_file;
}
 
loaded_file.data = ptr;
loaded_file.size = (unsigned int) filelen;
return loaded_file;
}
 
static void
unload_file (gnutls_datum_t data)
{
free (data.data);
}
 
/* Load the certificate and the private key.
 */
static void
load_keys (void)
{
int ret;
gnutls_datum_t data;
gnutls_x509_privkey_t x509_key;
 
data = load_file (CERT_FILE);
if (data.data == NULL)
{
fprintf (stderr, "*** Error loading certificate file.\n");
exit (1);
}
 
ret = gnutls_pcert_import_x509_raw (&crt, &data, GNUTLS_X509_FMT_PEM, 0);
/*if (ret < 0)
{
fprintf (stderr, "*** Error loading certificate file: %s\n",
gnutls_strerror (ret));
exit (1);
}*/
 
unload_file (data);
 
data = load_file (KEY_FILE);
if (data.data == NULL)
{
fprintf (stderr, "*** Error loading key file.\n");
exit (1);
}
 
gnutls_x509_privkey_init (&x509_key);
 
ret = gnutls_x509_privkey_import (x509_key, &data, GNUTLS_X509_FMT_PEM);
if (ret < 0)
{
fprintf (stderr, "*** Error loading key file: %s\n",
gnutls_strerror (ret));
exit (1);
}
 
gnutls_privkey_init (&key);
 
ret =
gnutls_privkey_import_x509 (key, x509_key,
GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE);
if (ret < 0)
{
fprintf (stderr, "*** Error importing key: %s\n",
gnutls_strerror (ret));
exit (1);
}
 
unload_file (data);
}
 
int
main (void)
{
int ret, sd, ii;
gnutls_session_t session;
gnutls_priority_t priorities_cache;
char buffer[MAX_BUF + 1];
gnutls_certificate_credentials_t xcred;
/* Allow connections to servers that have OpenPGP keys as well.
  */
 
gnutls_global_init ();

while(1){
load_keys ();
 
/* X509 stuff */
gnutls_certificate_allocate_credentials (&xcred);
 
/* priorities */
gnutls_priority_init (&priorities_cache, "NORMAL", NULL);
 
 
/* sets the trusted cas file
  */
gnutls_certificate_set_x509_trust_file (xcred, CAFILE, GNUTLS_X509_FMT_PEM);
 
gnutls_certificate_set_retrieve_function2 (xcred, cert_callback);
 
/* Initialize TLS session
  */
gnutls_init (&session, GNUTLS_CLIENT);
 
/* Use default priorities */
gnutls_priority_set (session, priorities_cache);
 
/* put the x509 credentials to the current session
  */
gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, xcred);
 
/* connect to the peer
  */
sd = tcp_connect ();
 
gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) sd);
 
/* Perform the TLS handshake
  */
ret = gnutls_handshake (session);
 
if (ret < 0)
{
fprintf (stderr, "*** Handshake failed\n");
gnutls_perror (ret);
goto end;
}
else
{
printf ("- Handshake was completed\n");
}
 
gnutls_record_send (session, MSG, strlen (MSG));
 
ret = gnutls_record_recv (session, buffer, MAX_BUF);
if (ret == 0)
{
printf ("- Peer has closed the TLS connection\n");
goto end;
}
else if (ret < 0)
{
fprintf (stderr, "*** Error: %s\n", gnutls_strerror (ret));
goto end;
}
 
printf ("- Received %d bytes: ", ret);
for (ii = 0; ii < ret; ii++)
{
fputc (buffer[ii], stdout);
}
fputs ("\n", stdout);
 
gnutls_bye (session, GNUTLS_SHUT_RDWR);

end:
 
tcp_close (sd);
 
gnutls_deinit (session);
 
gnutls_certificate_free_credentials (xcred);
gnutls_priority_deinit (priorities_cache);
} 
gnutls_global_deinit ();
 
return 0;
}
 
 
 
/* This callback should be associated with a session by calling
 * gnutls_certificate_client_set_retrieve_function( session, cert_callback),
 * before a handshake.
 */
 
static int
cert_callback (gnutls_session_t session,
const gnutls_datum_t * req_ca_rdn, int nreqs,
const gnutls_pk_algorithm_t * sign_algos,
int sign_algos_length, gnutls_pcert_st ** pcert,
unsigned int *pcert_length, gnutls_privkey_t * pkey)
{
char issuer_dn[256];
int i, ret;
size_t len;
gnutls_certificate_type_t type;
 
/* Print the server's trusted CAs
  */
if (nreqs > 0)
printf ("- Server's trusted authorities:\n");
else
printf ("- Server did not send us any trusted authorities names.\n");
 
/* print the names (if any) */
for (i = 0; i < nreqs; i++)
{
len = sizeof (issuer_dn);
ret = gnutls_x509_rdn_get (&req_ca_rdn[i], issuer_dn, &len);
if (ret >= 0)
{
printf (" [%d]: ", i);
printf ("%s\n", issuer_dn);
}
}
 
/* Select a certificate and return it.
  * The certificate must be of any of the "sign algorithms"
  * supported by the server.
  */
type = gnutls_certificate_type_get (session);
if (type == GNUTLS_CRT_X509)
{
*pcert_length = 1;
*pcert = &crt;
*pkey = key;
}
else
{
return -1;
}
 
return 0;
 
}
