#ifndef APP_DEFAULTS_H
#define APP_DEFAULTS_H

/** Factory defaults — edit here once; applied when field is empty in saved settings. */
#ifndef APP_DEFAULT_SENDER_EMAIL
#define APP_DEFAULT_SENDER_EMAIL     ""
#endif
#ifndef APP_DEFAULT_RECIPIENT_EMAIL
#define APP_DEFAULT_RECIPIENT_EMAIL  ""
#endif
/* Gmail App Password (16 chars). I-set sa Settings sa device — huwag i-commit ang totoong password. */
#ifndef APP_DEFAULT_SMTP_APP_PASSWORD
#define APP_DEFAULT_SMTP_APP_PASSWORD ""
#endif

#ifndef APP_SMTP_HOST
#define APP_SMTP_HOST  "smtp.gmail.com"
#endif

#endif
