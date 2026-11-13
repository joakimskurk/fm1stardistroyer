/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* Passes on one FM-1, layered (tools/sd_derelict.c, tools/sd_cine.c): each pass is one person playing one FM-1 under
 * tools/sd_play.h's rules, rendered in its own process (a fresh FM-1: sd_play.h's state is the file's), recorded to the
 * same tempo; then the stems at fixed faders, summed, one master gain to a -1 dBFS peak, nothing else added. The
 * stems: DIR/NAME_PASS.wav (DIR: OUT's own folder unless given). Include after sd_play.h and <sys/wait.h>,
 * <unistd.h>. The tool's setup() and score() read layer_pass. */
#ifndef SD_LAYER_H
#define SD_LAYER_H

#define LAYER_MAX 8u
static int layer_pass;


static int16_t *layer_read(const char *path, uint32_t *frames)   /* a stem as play_run wrote it */
{
    FILE *f = fopen(path, "rb");
    uint32_t bytes;
    int16_t *x;
    if (!f) { perror(path); return NULL; }
    fseek(f, 40, SEEK_SET);
    if (fread(&bytes, 4, 1, f) != 1) { fclose(f); return NULL; }
    x = malloc(bytes);
    if (!x || fread(x, 1, bytes, f) != bytes) { fclose(f); free(x); return NULL; }
    fclose(f);
    *frames = bytes / 4u;
    return x;
}

static int layer_mix(const char *what, const char *out, char stems[][512], const char *const *names,
                     const double *gain, uint32_t n_pass)
{
    int16_t *x[LAYER_MAX];
    uint32_t n[LAYER_MAX], len = 0, longest = 0, i, k;
    double *m, peak = 0.0, g;
    FILE *f;
    for (k = 0; k < n_pass; k++) {
        double e2 = 0.0;
        if (!(x[k] = layer_read(stems[k], &n[k]))) return 1;
        if (n[k] > len) len = n[k];
        for (i = 0; i < 2u * n[k]; i++) e2 += (double)x[k][i] * x[k][i];
        printf("stem %-6s %6.1f s, RMS %.1f dBFS, fader %.2f\n", names[k], n[k] / (double)SD_FS,
               10.0 * log10(e2 / (2.0 * n[k]) / (32767.0 * 32767.0) + 1e-12), gain[k]);
    }
    longest = len;
    m = calloc(2u * len, sizeof *m);
    if (!m) return 1;
    for (k = 0; k < n_pass; k++)
        for (i = 0; i < 2u * n[k]; i++) m[i] += gain[k] * x[k][i];
    for (i = 0; i < 2u * len; i++) if (fabs(m[i]) > peak) peak = fabs(m[i]);
    g = 32767.0 * pow(10.0, -1.0 / 20.0) / (peak > 1.0 ? peak : 1.0);
    while (len > SD_FS && fabs(m[2u * len - 1u] * g) < 3.3 && fabs(m[2u * len - 2u] * g) < 3.3) len--;   /* -80 dBFS */
    len = len + SD_FS / 2u > longest ? len : len + SD_FS / 2u;   /* the end: half a second past the last sound */
    f = fopen(out, "wb");
    if (!f) { perror(out); return 1; }
    {
        uint32_t fs = SD_FS, br = SD_FS * 4u, bytes = len * 4u, riff = bytes + 36u;
        fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f);
        fwrite("WAVEfmt \x10\0\0\0\x01\0\x02\0", 1, 16, f);
        fwrite(&fs, 4, 1, f); fwrite(&br, 4, 1, f);
        fwrite("\x04\0\x10\0data", 1, 8, f); fwrite(&bytes, 4, 1, f);
    }
    for (i = 0; i < 2u * len; i++) { int16_t o = (int16_t)lrint(m[i] * g); fwrite(&o, 2, 1, f); }
    fclose(f);
    printf("%s -> %s (%.1f s, %u passes layered; the master %+.1f dB to a -1 dBFS peak)\n", what, out,
           len / (double)SD_FS, n_pass, 20.0 * log10(g));
    for (k = 0; k < n_pass; k++) free(x[k]);
    free(m);
    return 0;
}

/* the tool's main: OUT.wav [STEMS_DIR]; each pass in its own process, then the layering */
static int layer_main(int argc, char **argv, const char *what, const char *const *names, const double *gain,
                      uint32_t n_pass, void (*setup)(void), int (*score)(void))
{
    char stems[LAYER_MAX][512], dir[400];
    const char *slash;
    uint32_t k;
    if (argc < 2) { fprintf(stderr, "usage: sd_%s OUT.wav [STEMS_DIR]\n", what); return 2; }
    slash = strrchr(argv[1], '/');
    if (argc > 2) snprintf(dir, sizeof dir, "%s", argv[2]);
    else if (slash) snprintf(dir, sizeof dir, "%.*s", (int)(slash - argv[1]), argv[1]);
    else snprintf(dir, sizeof dir, ".");
    for (k = 0; k < n_pass; k++) {
        pid_t pid;
        int st_;
        layer_pass = (int)k;
        snprintf(stems[k], sizeof stems[k], "%s/%s_%s.wav", dir, what, names[k]);
        printf("---- pass %u: %s\n", k + 1u, names[k]);
        fflush(stdout);
        pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) exit(play_run(stems[k], names[k], setup, score));   /* a fresh FM-1 */
        if (waitpid(pid, &st_, 0) < 0 || !WIFEXITED(st_) || WEXITSTATUS(st_)) {
            fprintf(stderr, "sd_%s: pass %u (%s) did not finish\n", what, k + 1u, names[k]);
            return 1;
        }
    }
    return layer_mix(what, argv[1], stems, names, gain, n_pass);
}
#endif
