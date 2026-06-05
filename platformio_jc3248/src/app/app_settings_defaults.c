#include "app_settings_defaults.h"
#include "app_defaults.h"
#include "app_types.h"
#include <string.h>
#include <ctype.h>

static void trim_spaces(char * s)
{
    if(!s) return;
    char * w = s;
    for(const char * p = s; *p; p++) {
        if(!isspace((unsigned char)*p))
            *w++ = *p;
    }
    *w = '\0';
}

static bool apply_str(char * dst, size_t dst_sz, const char * val)
{
    if(!dst || dst_sz == 0 || !val || !val[0]) return false;
    if(strcmp(dst, val) == 0) return false;
    strncpy(dst, val, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
    return true;
}

bool app_settings_apply_defaults(app_settings_t * settings)
{
    if(!settings) return false;

    bool changed = false;
    changed |= apply_str(settings->sender_email, sizeof(settings->sender_email),
                         APP_DEFAULT_SENDER_EMAIL);
    changed |= apply_str(settings->recipient_email, sizeof(settings->recipient_email),
                         APP_DEFAULT_RECIPIENT_EMAIL);
    if(APP_DEFAULT_SMTP_APP_PASSWORD[0]) {
        char trimmed[APP_SMTP_PASS_LEN];
        strncpy(trimmed, APP_DEFAULT_SMTP_APP_PASSWORD, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        trim_spaces(trimmed);
        changed |= apply_str(settings->smtp_app_password, sizeof(settings->smtp_app_password), trimmed);
    }
    trim_spaces(settings->smtp_app_password);
    return changed;
}
