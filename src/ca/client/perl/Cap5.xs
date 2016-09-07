/* Provides an EPICS Channel Access client interface for Perl5. */

/* This macro disables perl's reentr.inc file, which we don't need
 * here and just generates unnecessary compiler warnings. */
#define REENTRINC

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "cadef.h"
#include "db_access.h"
#include "epicsVersion.h"
#include "alarm.h"

typedef union {
    dbr_long_t iv;
    dbr_double_t nv;
    dbr_string_t pv;
} CA_data;

typedef struct CA_channel {
    chid chan;
    CA_data data;   /* Value storage for CA::get */
    char *sdata;    /* String storage for CA::get */
    size_t ssize;   /* Length allocated for sdata, excluding nil */
    SV *chan_ref;
    SV *conn_sub;
    SV *rights_sub;
} CA_channel;

static
void *p5_ctx;

static
const char * get_error_msg(int status) {
    static const char * const messages[] = {
        "ECA_NORMAL - Normal successful completion",
        "ECA_MAXIOC - Maximum simultaneous IOC connections exceeded",
        "ECA_UKNHOST - Unknown internet host",
        "ECA_UKNSERV - Unknown internet service",
        "ECA_SOCK - Unable to allocate a new socket",
        "ECA_CONN - Unable to connect to internet host or service",
        "ECA_ALLOCMEM - Unable to allocate additional dynamic memory",
        "ECA_UKNCHAN - Unknown IO channel",
        "ECA_UKNFIELD - Record field specified inappropriate for channel specified",
        "ECA_TOLARGE - The requested data transfer is greater than available memory or EPICS_CA_MAX_ARRAY_BYTES",
        "ECA_TIMEOUT - User specified timeout on IO operation expired",
        "ECA_NOSUPPORT - Sorry, that feature is planned but not supported at this time",
        "ECA_STRTOBIG - The supplied string is unusually large",
        "ECA_DISCONNCHID - The request was ignored because the specified channel is disconnected",
        "ECA_BADTYPE - The data type specifed is invalid",
        "ECA_CHIDNOTFND - Remote Channel not found",
        "ECA_CHIDRETRY - Unable to locate all user specified channels",
        "ECA_INTERNAL - Channel Access Internal Failure",
        "ECA_DBLCLFAIL - The requested local DB operation failed",
        "ECA_GETFAIL - Channel read request failed",
        "ECA_PUTFAIL - Channel write request failed",
        "ECA_ADDFAIL - Channel subscription request failed",
        "ECA_BADCOUNT - Invalid element count requested",
        "ECA_BADSTR - Invalid string",
        "ECA_DISCONN - Virtual circuit disconnect",
        "ECA_DBLCHNL - Identical process variable names on multiple servers",
        "ECA_EVDISALLOW - Request inappropriate within subscription (monitor) update callback",
        "ECA_BUILDGET - Database value get for that channel failed during channel search",
        "ECA_NEEDSFP - Unable to initialize without the vxWorks VX_FP_TASK task option set",
        "ECA_OVEVFAIL - Event queue overflow has prevented first pass event after event add",
        "ECA_BADMONID - Bad event subscription (monitor) identifier",
        "ECA_NEWADDR - Remote channel has new network address",
        "ECA_NEWCONN - New or resumed network connection",
        "ECA_NOCACTX - Specified task isnt a member of a CA context",
        "ECA_DEFUNCT - Attempt to use defunct CA feature failed",
        "ECA_EMPTYSTR - The supplied string is empty",
        "ECA_NOREPEATER - Unable to spawn the CA repeater thread- auto reconnect will fail",
        "ECA_NOCHANMSG - No channel id match for search reply- search reply ignored",
        "ECA_DLCKREST - Reseting dead connection- will try to reconnect",
        "ECA_SERVBEHIND - Server (IOC) has fallen behind or is not responding- still waiting",
        "ECA_NOCAST - No internet interface with broadcast available",
        "ECA_BADMASK - Invalid event selection mask",
        "ECA_IODONE - IO operations have completed",
        "ECA_IOINPROGRESS - IO operations are in progress",
        "ECA_BADSYNCGRP - Invalid synchronous group identifier",
        "ECA_PUTCBINPROG - Put callback timed out",
        "ECA_NORDACCESS - Read access denied",
        "ECA_NOWTACCESS - Write access denied",
        "ECA_ANACHRONISM - Requested feature is no longer supported",
        "ECA_NOSEARCHADDR - Empty PV search address list",
        "ECA_NOCONVERT - No reasonable data conversion between client and server types",
        "ECA_BADCHID - Invalid channel identifier",
        "ECA_BADFUNCPTR - Invalid function pointer",
        "ECA_ISATTACHED - Thread is already attached to a client context",
        "ECA_UNAVAILINSERV - Not supported by attached service",
        "ECA_CHANDESTROY - User destroyed channel",
        "ECA_BADPRIORITY - Invalid channel priority",
        "ECA_NOTTHREADED - Preemptive callback not enabled - additional threads may not join context",
        "ECA_16KARRAYCLIENT - Client's protocol revision does not support transfers exceeding 16k bytes",
        "ECA_CONNSEQTMO - Virtual circuit connection sequence aborted",
        "ECA_UNRESPTMO - Virtual circuit unresponsive"
    };

    return messages[CA_EXTRACT_MSG_NO(status)];
}


static
chtype best_type(CA_channel *pch) {
    switch (ca_field_type(pch->chan)) {
    case DBF_STRING:
    case DBF_ENUM:
        return DBF_STRING;
    case DBF_CHAR:
        if (ca_element_count(pch->chan) > 1)
            return DBF_CHAR;
        /* Fall through */
    case DBF_INT:
    case DBF_LONG:
        return DBF_LONG;
    case DBF_FLOAT:
    case DBF_DOUBLE:
        return DBF_DOUBLE;
    }
    croak("Unexpected field type %s",
        dbf_type_to_text(ca_field_type(pch->chan)));
}


static
SV * newSVdbf(chtype type, const void *dbr, int index) {
    switch (type) {
        char *pc;
        size_t len;

    case DBR_STRING:
        pc = (char *)dbr + index * MAX_STRING_SIZE;
        len = strlen(pc);
        return newSVpv(pc, len < MAX_STRING_SIZE ? len : MAX_STRING_SIZE);
    case DBR_LONG:
        return newSViv(((dbr_long_t *)dbr)[index]);
    case DBR_DOUBLE:
        return newSVnv(((dbr_double_t *)dbr)[index]);
    default:
        croak("Unexpected data type %s", dbf_type_to_text(type));
    }
}


static
SV * newSValarm(int sevr) {
    SV *alarm = &PL_sv_undef;
    if (sevr) {
        alarm = newSViv(sevr);
        sv_setpv(alarm, epicsAlarmSeverityStrings[sevr]);
        SvIOK_on(alarm);
    }
    return alarm;
}

static
void hashAdd(HV *hash, const char *key, I32 klen, SV *val) {
    SV **result = hv_store(hash, key, klen, val, 0);

    if (result == NULL)
        SvREFCNT_dec(val);
}

static
SV * newSVdbr(struct event_handler_args *peha) {
    const int is_primitive = dbr_type_is_plain(peha->type) ||
        (peha->type == DBR_CLASS_NAME);
    HV *hash;
    SV *val;
    chtype value_type;
    union db_access_val *u;

    if (dbr_type_is_STRING(peha->type) ||
        peha->type == DBR_STSACK_STRING ||
        peha->type == DBR_CLASS_NAME)
        value_type = DBR_STRING;
    else if (dbr_type_is_CHAR(peha->type))
        value_type = DBR_CHAR;
    else if (dbr_type_is_LONG(peha->type))
        value_type = DBR_LONG;
    else if (dbr_type_is_DOUBLE(peha->type))
        value_type = DBR_DOUBLE;
    else if (dbr_type_is_ENUM(peha->type))
        /* Only seen as DBR_GR_ENUM and DBR_CTRL_ENUM */
        value_type = DBR_ENUM;
    else {
        croak("Unexpected data type %s",
            dbf_type_to_text(peha->type));
    }

    if (is_primitive) {
        if (value_type == DBR_CHAR) {
            /* Long string => Perl scalar */
            ((char *)peha->dbr) [peha->count - 1] = 0;
            return newSVpv(peha->dbr, 0);
        }

        if (peha->count != 1) {
            /* Array of values => Perl array reference */
            AV *array;
            int i;

            array = newAV();
            for (i = 0; i < peha->count; i++) {
                av_push(array, newSVdbf(value_type, peha->dbr, i));
            }
            return newRV_noinc((SV *)array);
        }

        /* Single value => Perl scalar */
        return newSVdbf(value_type, peha->dbr, 0);
    }

    /* Compound => Perl hash reference */
    u = (union db_access_val *)peha->dbr;
    hash = newHV();

    /* Add basic meta-data */
    hashAdd(hash, "TYPE", 4,
        newSVpv(dbr_type_to_text(peha->type), 0));
    hashAdd(hash, "COUNT", 5, newSViv(peha->count));

    /* Alarm status and severity are always in the same place */
    if (u->slngval.status)
        val = newSVpv(epicsAlarmConditionStrings[u->slngval.status], 0);
    else
        val = &PL_sv_undef;
    hashAdd(hash, "status", 6, val);
    hashAdd(hash, "severity", 8,
        newSValarm(u->slngval.severity));

    if (peha->type == DBR_GR_ENUM ||
        peha->type == DBR_CTRL_ENUM) {
        AV *strings = newAV();
        int n = u->genmval.no_str;
        int i;

        val = newSViv(u->genmval.value);

        for (i = 0; i < n; i++) {
            size_t slen = strlen(u->genmval.strs[i]);
            if (slen > MAX_ENUM_STRING_SIZE)
                slen = MAX_ENUM_STRING_SIZE;
            av_push(strings, newSVpv(u->genmval.strs[i], slen));
            if (i == u->genmval.value) {
                sv_setpvn(val, u->genmval.strs[i], slen);
                SvIOK_on(val);
            }
        }
        hashAdd(hash, "strs", 4,
            newRV_noinc((SV *)strings));
        hashAdd(hash, "no_str", 6,
            newSViv(u->genmval.no_str));
        hashAdd(hash, "value", 5, val);

        return newRV_noinc((SV *)hash);
    }

    /* Value */
    if (value_type == DBR_CHAR) {
        char *str = dbr_value_ptr(peha->dbr, peha->type);

        /* Long string => Perl scalar */
        str[peha->count - 1] = 0;
        val = newSVpv(str, 0);
    } else if (peha->count == 1) {
        /* Single value => Perl scalar */
        val = newSVdbf(value_type,
            dbr_value_ptr(peha->dbr, peha->type), 0);
    } else {
        /* Array of values => Perl array reference */
        AV *array = newAV();
        int i;

        for (i = 0; i < peha->count; i++) {
            av_push(array, newSVdbf(value_type,
                dbr_value_ptr(peha->dbr, peha->type), i));
        }
        val = newRV_noinc((SV *)array);
    }
    hashAdd(hash, "value", 5, val);

    /* Timestamp follows status and severity in DBR_TIME */
    if (dbr_type_is_TIME(peha->type)) {
        struct timespec t;

        epicsTimeToTimespec(&t, &u->tlngval.stamp);
        hashAdd(hash, "stamp", 5,
            newSViv(t.tv_sec));
        hashAdd(hash, "stamp_fraction", 14,
            newSVnv((double)t.tv_nsec / 1e9));
    }
    else if (peha->type == DBR_STSACK_STRING) {
        struct dbr_stsack_string *s = (struct dbr_stsack_string *)peha->dbr;

        hashAdd(hash, "ackt", 4,
            newSViv(s->ackt));
        hashAdd(hash, "acks", 4,
            newSValarm(s->acks));
    }
    else if (value_type != DBR_STRING &&
        (dbr_type_is_GR(peha->type) ||
        dbr_type_is_CTRL(peha->type))) {
        char *units;
        size_t ulen;
        void *limit;
        int i = dbr_type_is_CTRL(peha->type) ? 7 : 5;

        if (value_type == DBR_DOUBLE) {
            units = u->gdblval.units;
            limit = &u->gdblval.upper_disp_limit;
            hashAdd(hash, "precision", 9,
                newSViv(u->gdblval.precision));
        } else { /* value_type == DBR_LONG */
            units = u->glngval.units;
            limit = &u->glngval.upper_disp_limit;
        }

        ulen = strlen(units);
        hashAdd(hash, "units", 5, newSVpv(units,
            ulen < MAX_UNITS_SIZE ? ulen : MAX_UNITS_SIZE));

        while (i >= 0) {
            static const char * const limit_name[] = {
                "upper_disp_limit", "lower_disp_limit",
                "upper_alarm_limit", "upper_warning_limit",
                "lower_warning_limit", "lower_alarm_limit",
                "upper_ctrl_limit", "lower_ctrl_limit",
            };

            hashAdd(hash, limit_name[i], strlen(limit_name[i]),
                newSVdbf(value_type, limit, i));
            i--;
        }
    }

    return newRV_noinc((SV *)hash);
}


enum io_type {
    IO_GET,
    IO_PUT,
    IO_MONITOR,
};

static
void io_handler(struct event_handler_args *peha, enum io_type io) {
    PERL_SET_CONTEXT(p5_ctx);
    {
        CA_channel *pch = ca_puser(peha->chid);
        SV *code = (SV *)peha->usr;
        SV *status = &PL_sv_undef;
        SV *data = &PL_sv_undef;
        dSP;

        ENTER;
        SAVETMPS;

        if (peha->status != ECA_NORMAL) {
            status = sv_2mortal(newSVpv(get_error_msg(peha->status), 0));
        } else if (io != IO_PUT) {
            data = sv_2mortal(newSVdbr(peha));
        }

        sv_setsv(ERRSV, &PL_sv_undef);

        PUSHMARK(SP);
        XPUSHs(pch->chan_ref);
        XPUSHs(status);
        XPUSHs(data);
        PUTBACK;

        call_sv(code, G_VOID | G_DISCARD | G_EVAL | G_KEEPERR);

        if (io != IO_MONITOR)
            SvREFCNT_dec(code);

        if (SvTRUE(ERRSV))
            croak(NULL);

        FREETMPS;
        LEAVE;
    }
}


static
int replace_handler(SV * sub, SV ** ph_sub, long *phandler) {
    if (SvOK(sub) && SvTRUE(sub)) {
        if (*ph_sub != NULL) {
            SvSetSV(*ph_sub, sub);
            return FALSE;
        }
        *ph_sub = newSVsv(sub);
    } else {
        if (*ph_sub == NULL)
            return FALSE;

        SvREFCNT_dec(*ph_sub);
        *ph_sub = NULL;
        *phandler = 0;
    }
    return TRUE;
}


/******************************************************************************/

/* CA::new($class, $name, [\&sub]) */

static
void connect_handler(struct connection_handler_args cha) {
    CA_channel *pch = ca_puser(cha.chid);

    PERL_SET_CONTEXT(p5_ctx);
    {
        dSP;

        SvSetSV(ERRSV, &PL_sv_undef);

        PUSHMARK(SP);
        XPUSHs(pch->chan_ref);
        XPUSHs(cha.op == CA_OP_CONN_UP ? &PL_sv_yes : &PL_sv_no);
        PUTBACK;

        call_sv(pch->conn_sub, G_EVAL | G_VOID | G_DISCARD | G_KEEPERR);

        if (SvTRUE(ERRSV))
            croak(NULL);
    }
}

SV * CA_new(const char *class, const char *name, ...) {
    dXSARGS;
    SV *ca_ref = newSViv(0);
    SV *ca_obj = newSVrv(ca_ref, class);
    CA_channel *pch;
    caCh *handler;
    int status;

    Newz(0, pch, 1, CA_channel);
    sv_setiv(ca_obj, (IV)pch);
    SvREADONLY_on(ca_obj);

    pch->chan_ref = ca_ref;
    (void) SvREFCNT_inc(ca_ref);

    if (items > 2
        && SvOK(ST(2))) {
        /* Connection handler provided */
        pch->conn_sub = newSVsv(ST(2));
        handler = &connect_handler;
    } else
        handler = NULL;

    status = ca_create_channel(name, handler, pch, 0, &pch->chan);
    if (status != ECA_NORMAL) {
        SvREFCNT_dec(ca_ref);
        if (pch->conn_sub)
            SvREFCNT_dec(pch->conn_sub);
        croak("%s", get_error_msg(status));
    }

    return ca_ref;
}

static int destroyed = 0;

/* CA::DESTROY($ca_ref) */

void CA_DESTROY(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    int status;

    status = destroyed ? ECA_NORMAL : ca_clear_channel(pch->chan);

    if (pch->conn_sub)
        SvREFCNT_dec(pch->conn_sub);

    if (pch->rights_sub)
        SvREFCNT_dec(pch->rights_sub);

    if (pch->sdata)
        Safefree(pch->sdata);

    SvREFCNT_dec(pch->chan_ref);
    Safefree(pch);

    if (status != ECA_NORMAL)
        croak("%s", get_error_msg(status));
}


/* CA::context_destroy($class) */

void CA_context_destroy(const char *class) {
    ca_context_destroy();
    destroyed = 1;
}


/* CA::change_connection_event($ca_ref, \$sub) */

void CA_change_connection_event(SV *ca_ref, SV *sub) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    caCh *handler = &connect_handler;
    int status;

    if (! replace_handler(sub, &pch->conn_sub, (long *)&handler))
        return;

    status = ca_change_connection_event(pch->chan, handler);

    if (status != ECA_NORMAL) {
        croak("%s", get_error_msg(status));
    }
}

/* CA::replace_access_rights_event($ca_ref, \$sub) */

static
void rights_handler(struct access_rights_handler_args arha) {
    CA_channel *pch = ca_puser(arha.chid);

    PERL_SET_CONTEXT(p5_ctx);
    {
        dSP;

        SvSetSV(ERRSV, &PL_sv_undef);

        PUSHMARK(SP);
        XPUSHs(pch->chan_ref);
        XPUSHs(arha.ar.read_access ? &PL_sv_yes : &PL_sv_no);
        XPUSHs(arha.ar.write_access ? &PL_sv_yes : &PL_sv_no);
        PUTBACK;

        call_sv(pch->rights_sub, G_EVAL | G_VOID | G_DISCARD | G_KEEPERR);

        if (SvTRUE(ERRSV))
            croak(NULL);
    }
}

void CA_replace_access_rights_event(SV *ca_ref, SV *sub) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    caArh *handler = &rights_handler;
    int status;

    if (! replace_handler(sub, &pch->rights_sub, (long *)&handler))
        return;

    status = ca_replace_access_rights_event(pch->chan, handler);

    if (status != ECA_NORMAL) {
        croak("%s", get_error_msg(status));
    }
}


/* CA::put($ca_ref, @values) */

void CA_put(SV *ca_ref, SV *val, ...) {
    dXSARGS;
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    int num_values = items - 1;
    int status;

    if (num_values == 1) {
        if (ca_field_type(pch->chan) == DBF_CHAR &&
            ca_element_count(pch->chan) > 1) {
            size_t len;
            char *long_string = SvPV(val, len);

            status = ca_array_put(DBF_CHAR, len+1, pch->chan, long_string);
        } else {
            union {
                dbr_long_t   dbr_long;
                dbr_double_t dbr_double;
                dbr_string_t dbr_string;
            } data;
            chtype type = best_type(pch);

            switch (type) {
            case DBF_LONG:
                data.dbr_long = SvIV(val);
                break;
            case DBF_DOUBLE:
                data.dbr_double = SvNV(val);
                break;
            case DBF_STRING:
                strncpy(data.dbr_string, SvPV_nolen(val), MAX_STRING_SIZE);
                break;
            }
            status = ca_put(type, pch->chan, &data);
        }
    } else {
        union {
            dbr_char_t   *dbr_char;
            dbr_long_t   *dbr_long;
            dbr_double_t *dbr_double;
            char         *dbr_string;
            void         *dbr;
        } p;
        int i;
        chtype type = best_type(pch);

        switch (type) {
        case DBF_CHAR:
            New(0, p.dbr_char, num_values, dbr_char_t);
            for (i = 0; i < num_values; i++) {
                p.dbr_char[i] = SvIV(ST(i + 1));
            }
            break;
        case DBF_LONG:
            New(0, p.dbr_long, num_values, dbr_long_t);
            for (i = 0; i < num_values; i++) {
                p.dbr_long[i] = SvIV(ST(i + 1));
            }
            break;
        case DBF_DOUBLE:
            New(0, p.dbr_double, num_values, dbr_double_t);
            for (i = 0; i < num_values; i++) {
                p.dbr_double[i] = SvNV(ST(i + 1));
            }
            break;
        case DBF_STRING:
            New(0, p.dbr_string, num_values * MAX_STRING_SIZE, char);
            for (i = 0; i < num_values; i++) {
                char * src = SvPV_nolen(ST(i + 1));
                strncpy(p.dbr_string + i, src, MAX_STRING_SIZE);
            }
            break;
        }

        status = ca_array_put(type, num_values, pch->chan, p.dbr);
        Safefree(p.dbr);
    }
    if (status != ECA_NORMAL) {
        croak("%s", get_error_msg(status));
    }
    XSRETURN(0);
}


/* CA::put_callback($ca_ref, \&sub, @values) */

static
void put_handler(struct event_handler_args eha) {
    io_handler(&eha, IO_PUT);
}

void CA_put_callback(SV *ca_ref, SV *sub, SV *val, ...) {
    dXSARGS;
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    SV *put_sub = newSVsv(sub);
    int num_values = items - 2;
    int status;

    if (num_values == 1) {
        if (ca_field_type(pch->chan) == DBF_CHAR &&
            ca_element_count(pch->chan) > 1) {
            size_t len;
            char *long_string = SvPV(val, len);

            status = ca_array_put_callback(DBF_CHAR, len+1, pch->chan,
                long_string, put_handler, put_sub);
        } else {
            union {
                dbr_long_t   dbr_long;
                dbr_double_t dbr_double;
                dbr_string_t dbr_string;
            } data;
            chtype type = best_type(pch);

            switch (type) {
            case DBF_LONG:
                data.dbr_long = SvIV(val);
                break;
            case DBF_DOUBLE:
                data.dbr_double = SvNV(val);
                break;
            case DBF_STRING:
                strncpy(data.dbr_string, SvPV_nolen(val), MAX_STRING_SIZE);
                break;
            }

            status = ca_put_callback(type, pch->chan, &data, put_handler, put_sub);
        }
    } else {
        union {
            dbr_char_t   *dbr_char;
            dbr_long_t   *dbr_long;
            dbr_double_t *dbr_double;
            char         *dbr_string;
            void         *dbr;
        } p;
        int i;
        chtype type = best_type(pch);

        switch (type) {
        case DBF_CHAR:
            New(0, p.dbr_char, num_values, dbr_char_t);
            for (i = 0; i < num_values; i++) {
                p.dbr_char[i] = SvIV(ST(i + 1));
            }
            break;
        case DBF_LONG:
            New(0, p.dbr_long, num_values, dbr_long_t);
            for (i = 0; i < num_values; i++) {
                p.dbr_long[i] = SvIV(ST(i + 2));
            }
            break;
        case DBF_DOUBLE:
            New(0, p.dbr_double, num_values, dbr_double_t);
            for (i = 0; i < num_values; i++) {
                p.dbr_double[i] = SvNV(ST(i + 2));
            }
            break;
        case DBF_STRING:
            New(0, p.dbr_string, num_values * MAX_STRING_SIZE, char);
            for (i = 0; i < num_values; i++) {
                char * src = SvPV_nolen(ST(i + 2));
                strncpy(p.dbr_string + i, src, MAX_STRING_SIZE);
            }
            break;
        }

        status = ca_array_put_callback(type, num_values, pch->chan, p.dbr,
            put_handler, put_sub);
        Safefree(p.dbr);
    }
    if (status != ECA_NORMAL) {
        SvREFCNT_dec(put_sub);
        croak("%s", get_error_msg(status));
    }
    XSRETURN(0);
}


/* CA::put_acks($ca_ref, $sevr, [\&sub]) */

void CA_put_acks(SV *ca_ref, SV *sevr, ...) {
    dXSARGS;
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    dbr_put_acks_t acks;
    int status;

    if (! SvOK(sevr)) {
        acks = NO_ALARM;
    } else if (SvIOK(sevr)) {
        acks = SvIV(sevr);
        if (acks > INVALID_ALARM)
            croak("Bad acknowledgement severity %d", acks);
    } else {
        size_t slen;
        char *sname = SvPV(sevr, slen);
        for (acks = NO_ALARM; acks <= INVALID_ALARM; acks++) {
            if (strcmp(sname, epicsAlarmSeverityStrings[acks]) == 0)
                break;
        }
        if (acks > INVALID_ALARM)
            croak("Bad acknowledgment severity '%s'", sname);
    }

    if (items > 2) {
        SV *put_sub = newSVsv(ST(2));
        status = ca_put_callback(DBR_PUT_ACKS, pch->chan, &acks,
            put_handler, put_sub);
        if (status != ECA_NORMAL)
            SvREFCNT_dec(put_sub);
    } else
        status = ca_put(DBR_PUT_ACKS, pch->chan, &acks);

    if (status != ECA_NORMAL)
        croak("%s", get_error_msg(status));

    XSRETURN(0);
}


/* CA::put_ackt($ca_ref, $trans, [\&sub]) */

void CA_put_ackt(SV *ca_ref, int ack, ...) {
    dXSARGS;
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    dbr_put_ackt_t ackt = !! ack;   /* 0 or 1 only */
    int status;

    if (items > 2) {
        SV *put_sub = newSVsv(ST(2));
        status = ca_put_callback(DBR_PUT_ACKT, pch->chan, &ackt,
            put_handler, put_sub);
        if (status != ECA_NORMAL)
            SvREFCNT_dec(put_sub);
    } else
        status = ca_put(DBR_PUT_ACKS, pch->chan, &ackt);

    if (status != ECA_NORMAL)
        croak("%s", get_error_msg(status));

    XSRETURN(0);
}


/* CA::get($ca_ref) */

void CA_get(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    size_t count = ca_element_count(pch->chan);
    int status;

    if (ca_field_type(pch->chan) == DBF_CHAR &&
        count > 1) {
        if (!pch->sdata) {
            New(0, pch->sdata, count + 1, char);
            pch->ssize = count;
        } else if (pch->ssize < count) { /* Reconnected to larger array? */
            Safefree(pch->sdata);
            New(0, pch->sdata, count + 1, char);
            pch->ssize = count;
        }
        status = ca_array_get(DBF_CHAR, 0, pch->chan, pch->sdata);
    } else {
        status = ca_get(best_type(pch), pch->chan, &pch->data);
    }
    if (status != ECA_NORMAL) {
        croak("%s", get_error_msg(status));
    }
}


/* CA::value($ca_ref) */

SV * CA_value(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    if (ca_field_type(pch->chan) == DBF_CHAR &&
        ca_element_count(pch->chan) > 1 &&
        pch->sdata) {
        return newSVpv(pch->sdata, 0);
    }
    return newSVdbf(best_type(pch), &pch->data, 0);
}


/* CA::get_callback($ca_ref, \&sub, [$type | $count]) */

static
void get_handler(struct event_handler_args eha) {
    io_handler(&eha, IO_GET);
}

void CA_get_callback(SV *ca_ref, SV *sub, ...) {
    dXSARGS;
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    SV *get_sub = newSVsv(sub);
    int status;
    chtype type = best_type(pch);
    int count = 0;
    int i = 2;
    const char *croak_msg;

    while (items > i
        && SvOK(ST(i))) {
        if (SvIOK(ST(i))) {
            /* Interger => Count arg, zero means current size */
            count = SvIV(ST(i));
            if (count < 0 || count > ca_element_count(pch->chan)) {
                croak_msg = "Requested array size is out of range";
                goto exit_croak;
            }
        } else if (SvPOKp(ST(i))) {
            /* String => Type arg */
            char *treq = SvPV_nolen(ST(i));

            dbr_text_to_type(treq, type);
            if (type < 0 ||
                type == DBR_PUT_ACKT ||
                type == DBR_PUT_ACKS) {
                croak_msg = "Requested DBR type is invalid";
                goto exit_croak;
            } else if (type == DBR_GR_ENUM ||
                type == DBR_CTRL_ENUM ||
                type == DBR_CLASS_NAME ||
                type == DBR_STSACK_STRING)
                /* The above types are supported */ ;
            else if (dbr_type_is_SHORT(type))
                type += (DBR_LONG - DBR_SHORT);
            else if (dbr_type_is_FLOAT(type))
                type += (DBR_DOUBLE - DBR_FLOAT);
            else if (dbr_type_is_ENUM(type))
                type += (DBR_STRING - DBR_ENUM);
        }
        i++;
    }

    status = ca_array_get_callback(type, count,
        pch->chan, get_handler, get_sub);
    if (status != ECA_NORMAL) {
        croak_msg = get_error_msg(status);
        goto exit_croak;
    }

    XSRETURN(0);
    return;

exit_croak:
    SvREFCNT_dec(get_sub);
    croak("%s", croak_msg);
}


/* CA::create_subscription($ca_ref, $mask, \&sub, [$type | $count]) */

static
void subscription_handler(struct event_handler_args eha) {
    io_handler(&eha, IO_MONITOR);
}

SV * CA_create_subscription(SV *ca_ref, const char *mask_str, SV *sub, ...) {
    dXSARGS;
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    SV *mon_sub = newSVsv(sub);
    SV *mon_ref = newSViv(0);
    SV *mon_obj = newSVrv(mon_ref, "CA::Subscription");
    chtype type = best_type(pch);
    int count = ca_element_count(pch->chan);
    int i = 3;
    int mask = 0;
    evid event;
    int status;
    const char *croak_msg;

    if (strchr(mask_str, 'v') || strchr(mask_str, 'V')) mask |= DBE_VALUE;
    if (strchr(mask_str, 'l') || strchr(mask_str, 'L')) mask |= DBE_LOG;
    if (strchr(mask_str, 'a') || strchr(mask_str, 'A')) mask |= DBE_ALARM;
    if (strchr(mask_str, 'p') || strchr(mask_str, 'P')) mask |= DBE_PROPERTY;

    while (items > i
        && SvOK(ST(i))) {
        if (SvIOK(ST(i))) {
            /* Interger => Count arg, zero means current size */
            count = SvIV(ST(i));
            if (count < 0 || count > ca_element_count(pch->chan)) {
                croak_msg = "Requested array size is out of range";
                goto exit_croak;
            }
        } else if (SvPOKp(ST(i))) {
            /* String => Type arg */
            size_t tlen;
            char *treq = SvPV(ST(i), tlen);

            dbr_text_to_type(treq, type);
            if (type < 0) {
                croak_msg = "Unknown CA data type";
                goto exit_croak;
            }
            if (type == DBR_PUT_ACKT ||
                type == DBR_PUT_ACKS) {
                croak_msg = "DBR_PUT_ACK types are write-only";
                goto exit_croak;
            } else if (type == DBR_GR_ENUM ||
                type == DBR_CTRL_ENUM ||
                type == DBR_CLASS_NAME ||
                type == DBR_STSACK_STRING)
                /* These above types are supported */ ;
            else if (dbr_type_is_SHORT(type))
                type += (DBR_LONG - DBR_SHORT);
            else if (dbr_type_is_FLOAT(type))
                type += (DBR_DOUBLE - DBR_FLOAT);
            else if (dbr_type_is_ENUM(type))
                type += (DBR_STRING - DBR_ENUM);
        }
        i++;
    }

    status = ca_create_subscription(type, count, pch->chan, mask,
        subscription_handler, mon_sub, &event);
    if (status != ECA_NORMAL) {
        croak_msg = get_error_msg(status);
        goto exit_croak;
    }

    sv_setiv(mon_obj, (IV)event);
    SvREADONLY_on(mon_obj);
    (void) SvREFCNT_inc(mon_ref);

    return mon_ref;

exit_croak:
    SvREFCNT_dec(mon_ref);
    SvREFCNT_dec(mon_sub);
    croak("%s", croak_msg);
}


/* CA::clear_subscription($class, $subscription) */

void CA_clear_subscription(const char *class, SV *mon_ref) {
    evid event = (evid)SvIV(SvRV(mon_ref));
    int status;

    if (! sv_isa(mon_ref, "CA::Subscription")) {
        croak("Not a CA::Subscription");
    }

    status = ca_clear_subscription(event);

    if (status != ECA_NORMAL) {
        croak("%s", get_error_msg(status));
    }
}


/* CA::pend_io($class, $timeout) */

void CA_pend_io(const char *class, double timeout) {
    int status = ca_pend_io(timeout);
    if (status != ECA_NORMAL) {
        croak("%s", get_error_msg(status));
    }
}

/* CA::test_io($class) */

int CA_test_io(const char *class) {
    return (ca_test_io() == ECA_IODONE);
}

/* CA::pend_event($class, $timeout) */

void CA_pend_event(const char *class, double timeout) {
    int status = ca_pend_event(timeout);
    if (status != ECA_TIMEOUT) {
        croak("%s", get_error_msg(status));
    }
}

/* CA::poll($class) */

void CA_poll(const char *class) {
    ca_poll();
}


/* CA::flush_io($class) */

void CA_flush_io(const char *class) {
    ca_flush_io();
}


/* CA::version($class) */

const char * CA_version(const char *class) {
    return EPICS_VERSION_STRING;
}

/* CA::add_exception_event($class, \&sub) */

static
SV * exception_sub = NULL;

static
void exception_handler(struct exception_handler_args eha) {
    if (! exception_sub)
        return;

    PERL_SET_CONTEXT(p5_ctx);
    {
        SV *channel = &PL_sv_undef;
        SV *status = &PL_sv_undef;
        HV *hash = newHV();
        SV *op;
        const char *opString[] = {
            "GET", "PUT", "CREATE_CHANNEL", "ADD_EVENT", "CLEAR_EVENT", "OTHER"
        };
        dSP;

        ENTER;
        SAVETMPS;

        if (eha.chid) {
            CA_channel *pch = ca_puser(eha.chid);
            channel = pch->chan_ref;
        }
        if (eha.stat != ECA_NORMAL) {
            status = sv_2mortal(newSVpv(get_error_msg(eha.stat), 0));
        }

        op = newSViv(eha.op);
        sv_setpv(op, opString[eha.op]);
        SvIOK_on(op);
        hashAdd(hash, "OP", 2, op);
        hashAdd(hash, "TYPE", 4,
            newSVpv(dbr_type_to_text(eha.type), 0));
        hashAdd(hash, "COUNT", 5, newSViv(eha.count));
        if (eha.pFile)
            hashAdd(hash, "FILE", 4, newSVpv(eha.pFile, 0));
        if (eha.lineNo)
            hashAdd(hash, "LINE", 4, newSVuv(eha.lineNo));

        PUSHMARK(SP);
        XPUSHs(channel);
        XPUSHs(status);
        XPUSHs(sv_2mortal(newSVpv(eha.ctx, 0)));
        XPUSHs(sv_2mortal(newRV_noinc((SV *)hash)));
        PUTBACK;

        call_sv(exception_sub, G_EVAL | G_VOID | G_DISCARD);

        FREETMPS;
        LEAVE;
    }
}

void CA_add_exception_event(const char *class, SV *sub) {
    caExceptionHandler *handler = exception_handler;
    int status;

    if (! replace_handler(sub, &exception_sub, (long *)&handler))
        return;

    status = ca_add_exception_event(handler, NULL);

    if (status != ECA_NORMAL) {
        SvREFCNT_dec(exception_sub);
        exception_sub = NULL;
        croak("%s", get_error_msg(status));
    }
}


/* CA::replace_printf_handler($class, \&sub) */

static
SV * printf_sub = NULL;

#ifndef va_copy
#  ifdef __GNUC__
#    define va_copy(d, s) __va_copy(d, s)
#  else
#    define va_copy(d, s) ((d) = (s))
/* The above macro is NOT PORTABLE but works on Windows when
 * stdarg.h doesn't provide va_copy(). Some architectures need
 *   define va_copy(d, s) ((*d) = (*s))
 * while others may be even more complicated, but hopefully the
 * system stdarg.h header defines va_copy() in those cases.
 */
#  endif
#endif

static
int printf_handler(const char *format, va_list args) {
    if (! printf_sub)
        return 0;

    PERL_SET_CONTEXT(p5_ctx);
    {
        SV *printf_str;
        dSP;
        va_list argcopy;

        ENTER;
        SAVETMPS;

        va_copy(argcopy, args);

        printf_str = NEWSV(0, strlen(format) + 32);
        sv_vsetpvf(printf_str, format, &argcopy);
        va_end(argcopy);

        PUSHMARK(SP);
        XPUSHs(sv_2mortal(printf_str));
        PUTBACK;

        call_sv(printf_sub, G_EVAL | G_VOID | G_DISCARD);

        FREETMPS;
        LEAVE;
    }
    return 0;
}

void CA_replace_printf_handler(const char *class, SV *sub) {
    caPrintfFunc *handler = printf_handler;
    int status;

    if (! replace_handler(sub, &printf_sub, (long *)&handler))
        return;

    status = ca_replace_printf_handler(handler);

    if (status != ECA_NORMAL) {
        SvREFCNT_dec(printf_sub);
        printf_sub = NULL;
        croak("%s", get_error_msg(status));
    }
}


/* CA::field_type($ca_ref) */

const char * CA_field_type(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    chtype t = ca_field_type(pch->chan);
    if (t == TYPENOTCONN)
        return "TYPENOTCONN";
    return dbr_type_to_text(t);
}


/* CA::element_count($ca_ref) */

int CA_element_count(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    return ca_element_count(pch->chan);
}


/* CA::name($ca_ref) */

const char * CA_name(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    return ca_name(pch->chan);
}


/* CA::state($ca_ref) */

const char * CA_state(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    static const char * const state_name[] = {
        "never connected", "previously connected", "connected", "closed"
    };
    return state_name[ca_state(pch->chan)];
}


/* CA::is_connected($ca_ref) */

int CA_is_connected(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    return ca_state(pch->chan) == cs_conn;
}


/* CA::host_name($ca_ref) */

const char * CA_host_name(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    return ca_host_name(pch->chan);
}


/* CA::read_access($ca_ref) */

int CA_read_access(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    return ca_read_access(pch->chan);
}


/* CA::write_access($ca_ref) */

int CA_write_access(SV *ca_ref) {
    CA_channel *pch = (CA_channel *)SvIV(SvRV(ca_ref));
    return ca_write_access(pch->chan);
}

/******************************************************************************/

/* Ensure that the generated boot_Cap5 function is visible
 * outside of the libCap5.so shared library when compiling
 * with GCC4+ and -fvisibility=hidden is used.
 */
#if __GNUC__ >= 4
XS(boot_Cap5) __attribute__ ((visibility ("default")));
#endif


MODULE = Cap5	PACKAGE = Cap5

MODULE = Cap5	PACKAGE = CA	PREFIX = CA_

PROTOTYPES: DISABLE

BOOT:
    p5_ctx = Perl_get_context();


SV *
CA_new (class, name, ...)
	const char *	class
	const char *	name
    PREINIT:
	I32* temp;
    CODE:
	temp = PL_markstack_ptr++;
	RETVAL = CA_new(class, name);
	PL_markstack_ptr = temp;
    OUTPUT:
	RETVAL

void
CA_DESTROY (ca_ref)
	SV *	ca_ref

void
CA_context_destroy (class)
	const char *	class

void
CA_change_connection_event (ca_ref, sub)
	SV *	ca_ref
	SV *	sub

void
CA_replace_access_rights_event (ca_ref, sub)
	SV *	ca_ref
	SV *	sub

void
CA_put (ca_ref, val, ...)
	SV *	ca_ref
	SV *	val
    PREINIT:
	I32* temp;
    PPCODE:
	temp = PL_markstack_ptr++;
	CA_put(ca_ref, val);
	if (PL_markstack_ptr != temp) {
          /* truly void, because dXSARGS not invoked */
	  PL_markstack_ptr = temp;
	  XSRETURN_EMPTY; /* return empty stack */
        }
        /* must have used dXSARGS; list context implied */
	return; /* assume stack size is correct */

void
CA_put_callback (ca_ref, sub, val, ...)
	SV *	ca_ref
	SV *	sub
	SV *	val
    PREINIT:
	I32* temp;
    PPCODE:
	temp = PL_markstack_ptr++;
	CA_put_callback(ca_ref, sub, val);
	if (PL_markstack_ptr != temp) {
          /* truly void, because dXSARGS not invoked */
	  PL_markstack_ptr = temp;
	  XSRETURN_EMPTY; /* return empty stack */
        }
        /* must have used dXSARGS; list context implied */
	return; /* assume stack size is correct */

void
CA_put_acks (ca_ref, sevr, ...)
	SV *	ca_ref
	SV *	sevr
    PREINIT:
	I32* temp;
    PPCODE:
	temp = PL_markstack_ptr++;
	CA_put_acks(ca_ref, sevr);
	if (PL_markstack_ptr != temp) {
          /* truly void, because dXSARGS not invoked */
	  PL_markstack_ptr = temp;
	  XSRETURN_EMPTY; /* return empty stack */
        }
        /* must have used dXSARGS; list context implied */
	return; /* assume stack size is correct */

void
CA_put_ackt (ca_ref, ack, ...)
	SV *	ca_ref
	int	ack
    PREINIT:
	I32* temp;
    PPCODE:
	temp = PL_markstack_ptr++;
	CA_put_ackt(ca_ref, ack);
	if (PL_markstack_ptr != temp) {
          /* truly void, because dXSARGS not invoked */
	  PL_markstack_ptr = temp;
	  XSRETURN_EMPTY; /* return empty stack */
        }
        /* must have used dXSARGS; list context implied */
	return; /* assume stack size is correct */

void
CA_get (ca_ref)
	SV *	ca_ref

SV *
CA_value (ca_ref)
	SV *	ca_ref

void
CA_get_callback (ca_ref, sub, ...)
	SV *	ca_ref
	SV *	sub
    PREINIT:
	I32* temp;
    PPCODE:
	temp = PL_markstack_ptr++;
	CA_get_callback(ca_ref, sub);
	if (PL_markstack_ptr != temp) {
          /* truly void, because dXSARGS not invoked */
	  PL_markstack_ptr = temp;
	  XSRETURN_EMPTY; /* return empty stack */
        }
        /* must have used dXSARGS; list context implied */
	return; /* assume stack size is correct */

SV *
CA_create_subscription (ca_ref, mask_str, sub, ...)
	SV *	ca_ref
	const char *	mask_str
	SV *	sub
    PREINIT:
	I32* temp;
    CODE:
	temp = PL_markstack_ptr++;
	RETVAL = CA_create_subscription(ca_ref, mask_str, sub);
	PL_markstack_ptr = temp;
    OUTPUT:
        RETVAL

void
CA_clear_subscription (class, mon_ref)
	const char *	class
	SV *	mon_ref

void
CA_pend_io (class, timeout)
	const char *	class
	double	timeout

int
CA_test_io (class)
	const char *	class

void
CA_pend_event (class, timeout)
	const char *	class
	double	timeout

void
CA_poll (class)
	const char *	class

void
CA_flush_io (class)
	const char *	class

const char *
CA_version (class)
	const char *	class

void
CA_add_exception_event (class, sub)
	const char *	class
	SV *	sub

void
CA_replace_printf_handler (class, sub)
	const char *	class
	SV *	sub

const char *
CA_field_type (ca_ref)
	SV *	ca_ref

int
CA_element_count (ca_ref)
	SV *	ca_ref

const char *
CA_name (ca_ref)
	SV *	ca_ref

const char *
CA_state (ca_ref)
	SV *	ca_ref

int
CA_is_connected (ca_ref)
	SV *	ca_ref

const char *
CA_host_name (ca_ref)
	SV *	ca_ref

int
CA_read_access (ca_ref)
	SV *	ca_ref

int
CA_write_access (ca_ref)
	SV *	ca_ref

