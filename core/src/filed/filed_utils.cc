

#include "filed/filed_utils.h"
#include "filed/filed_globals.h"
#include "lib/parse_conf.h"


namespace filedaemon {

/**
 * Make a quick check to see that we have all the
 * resources needed.
 */
bool CheckResources()
{
  bool OK = true;
  DirectorResource* director;
  const std::string& configfile = my_config->get_base_config_path();

  LockRes(my_config);

  me = (ClientResource*)my_config->GetNextRes(R_CLIENT, nullptr);
  my_config->own_resource_ = me;
  if (!me) {
    Emsg1(M_FATAL, 0,
          _("No File daemon resource defined in %s\n"
            "Without that I don't know who I am :-(\n"),
          configfile.c_str());
    OK = false;
  } else {
    // Sanity check.
    if (me->MaxConnections < (2 * me->MaxConcurrentJobs)) {
      me->MaxConnections = (2 * me->MaxConcurrentJobs) + 2;
    }

    if (my_config->GetNextRes(R_CLIENT, (BareosResource*)me) != nullptr) {
      Emsg1(M_FATAL, 0, _("Only one Client resource permitted in %s\n"),
            configfile.c_str());
      OK = false;
    }
    MyNameIs(0, nullptr, me->resource_name_);
    if (!me->messages) {
      me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, nullptr);
      if (!me->messages) {
        Emsg1(M_FATAL, 0, _("No Messages resource defined in %s\n"),
              configfile.c_str());
        OK = false;
      }
    }
    if (me->pki_encrypt || me->pki_sign) {
#ifndef HAVE_CRYPTO
      Jmsg(nullptr, M_FATAL, 0,
           _("PKI encryption/signing enabled but not compiled into Bareos.\n"));
      OK = false;
#endif
    }

    /* pki_encrypt implies pki_sign */
    if (me->pki_encrypt) { me->pki_sign = true; }

    if ((me->pki_encrypt || me->pki_sign) && !me->pki_keypair_file) {
      Emsg2(M_FATAL, 0,
            _("\"PKI Key Pair\" must be defined for File"
              " daemon \"%s\" in %s if either \"PKI Sign\" or"
              " \"PKI Encrypt\" are enabled.\n"),
            me->resource_name_, configfile.c_str());
      OK = false;
    }

    /* If everything is well, attempt to initialize our public/private keys */
    if (OK && (me->pki_encrypt || me->pki_sign)) {
      char* filepath = nullptr;
      /* Load our keypair */
      me->pki_keypair = crypto_keypair_new();
      if (!me->pki_keypair) {
        Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
        OK = false;
      } else {
        if (!CryptoKeypairLoadCert(me->pki_keypair, me->pki_keypair_file)) {
          Emsg2(M_FATAL, 0,
                _("Failed to load public certificate for File"
                  " daemon \"%s\" in %s.\n"),
                me->resource_name_, configfile.c_str());
          OK = false;
        }

        if (!CryptoKeypairLoadKey(me->pki_keypair, me->pki_keypair_file,
                                  nullptr, nullptr)) {
          Emsg2(M_FATAL, 0,
                _("Failed to load private key for File"
                  " daemon \"%s\" in %s.\n"),
                me->resource_name_, configfile.c_str());
          OK = false;
        }
      }

      // Trusted Signers. We're always trusted.
      me->pki_signers = new alist<X509_KEYPAIR*>(10, not_owned_by_alist);
      if (me->pki_keypair) {
        me->pki_signers->append(crypto_keypair_dup(me->pki_keypair));
      }

      /* If additional signing public keys have been specified, load them up */
      if (me->pki_signing_key_files) {
        foreach_alist (filepath, me->pki_signing_key_files) {
          X509_KEYPAIR* keypair;

          keypair = crypto_keypair_new();
          if (!keypair) {
            Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
            OK = false;
          } else {
            if (CryptoKeypairLoadCert(keypair, filepath)) {
              me->pki_signers->append(keypair);

              /* Attempt to load a private key, if available */
              if (CryptoKeypairHasKey(filepath)) {
                if (!CryptoKeypairLoadKey(keypair, filepath, nullptr,
                                          nullptr)) {
                  Emsg3(M_FATAL, 0,
                        _("Failed to load private key from file %s for File"
                          " daemon \"%s\" in %s.\n"),
                        filepath, me->resource_name_, configfile.c_str());
                  OK = false;
                }
              }

            } else {
              Emsg3(M_FATAL, 0,
                    _("Failed to load trusted signer certificate"
                      " from file %s for File daemon \"%s\" in %s.\n"),
                    filepath, me->resource_name_, configfile.c_str());
              OK = false;
            }
          }
        }
      }

      /*
       * Crypto recipients. We're always included as a recipient.
       * The symmetric session key will be encrypted for each of these readers.
       */
      me->pki_recipients = new alist<X509_KEYPAIR*>(10, not_owned_by_alist);
      if (me->pki_keypair) {
        me->pki_recipients->append(crypto_keypair_dup(me->pki_keypair));
      }


      /* If additional keys have been specified, load them up */
      if (me->pki_master_key_files) {
        foreach_alist (filepath, me->pki_master_key_files) {
          X509_KEYPAIR* keypair;

          keypair = crypto_keypair_new();
          if (!keypair) {
            Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
            OK = false;
          } else {
            if (CryptoKeypairLoadCert(keypair, filepath)) {
              me->pki_recipients->append(keypair);
            } else {
              Emsg3(M_FATAL, 0,
                    _("Failed to load master key certificate"
                      " from file %s for File daemon \"%s\" in %s.\n"),
                    filepath, me->resource_name_, configfile.c_str());
              OK = false;
            }
          }
        }
      }
    }
  }


  /* Verify that a director record exists */
  LockRes(my_config);
  director = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  UnlockRes(my_config);
  if (!director) {
    Emsg1(M_FATAL, 0, _("No Director resource defined in %s\n"),
          configfile.c_str());
    OK = false;
  }

  UnlockRes(my_config);

  if (OK) {
    CloseMsg(nullptr);              /* close temp message handler */
    InitMsg(nullptr, me->messages); /* open user specified message handler */
    if (me->secure_erase_cmdline) {
      SetSecureEraseCmdline(me->secure_erase_cmdline);
    }
    if (me->log_timestamp_format) {
      SetLogTimestampFormat(me->log_timestamp_format);
    }
  }

  return OK;
}


}  // namespace filedaemon
