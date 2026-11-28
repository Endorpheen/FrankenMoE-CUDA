static char * host_dupz(sl_u8 s) {
    char * p = (char *) malloc((size_t) s.len + 1);
    if (p == NULL) {
        return NULL;
    }
    if (s.len > 0) {
        memcpy(p, s.data, (size_t) s.len);
    }
    p[s.len] = '\0';
    return p;
}

double gs_frexp_mant(double x) {
    int e = 0;
    return frexp(x, &e);
}

double gs_json_strtod(sl_u8 s) {
    char * t = host_dupz(s);
    if (t == NULL) {
        return 0.0;
    }
    const double v = strtod(t, NULL);
    free(t);
    return v;
}

int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    char * f = host_dupz(fmt);
    if (f == NULL) {
        return -1;
    }
    const time_t t = epoch < 0 ? time(NULL) : (time_t) epoch;
    const struct tm * tmv = localtime(&t);
    if (tmv == NULL) {
        free(f);
        return -1;
    }
    const size_t w = strftime((char *) out.data, (size_t) out.len, f, tmv);
    free(f);
    return (int64_t) w;
}
